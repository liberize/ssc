#pragma once
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils.h"
#ifdef __linux__
#include "elf.h"
#else
#error Mounting squashfs works for linux only!
#endif

#define bswap16(value) ((((value) & 0xff) << 8) | ((value) >> 8))
#define bswap32(value) (((uint32_t)bswap16((uint16_t)((value) & 0xffff)) << 16) | (uint32_t)bswap16((uint16_t)((value) >> 16)))
#define bswap64(value) (((uint64_t)bswap32((uint32_t)((value) & 0xffffffff)) << 32) | (uint64_t)bswap32((uint32_t)((value) >> 32)))

extern "C" int fusefs_main(int argc, char *argv[], void (*mounted) (void));

ssize_t get_elf_size(const char *path) {
    FILE* fp = NULL;
    ssize_t size = -1;
    Elf64_Ehdr ehdr;
    Elf64_Shdr shdr;
    int bswap = 0;
    off_t last_shdr_offset, sht_end, last_section_end;
    
    fp = fopen(path, "rb");
    if (fp == NULL) {
        LOGE("Cannot open elf file");
        goto exit;
    }
    if (fread(ehdr.e_ident, 1, EI_NIDENT, fp) != EI_NIDENT) {
        LOGE("Failed to read e_ident from elf file");
        goto exit;
    }
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB && ehdr.e_ident[EI_DATA] != ELFDATA2MSB) {
        LOGE("Unknown ELF data order");
        goto exit;
    }
#if   __BYTE_ORDER == __LITTLE_ENDIAN
    bswap = ehdr.e_ident[EI_DATA] != ELFDATA2LSB;
#elif __BYTE_ORDER == __BIG_ENDIAN
    bswap = ehdr.e_ident[EI_DATA] != ELFDATA2LSB;
#else
#error "Unknown machine endian"
#endif
    fseeko(fp, 0, SEEK_SET);
    if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr ehdr32;
        Elf32_Shdr shdr32;
        if (fread(&ehdr32, 1, sizeof(ehdr32), fp) != sizeof(ehdr32)) {
            LOGE("Failed to read elf header");
            goto exit;
        }
        ehdr.e_shoff     = bswap ? bswap32(ehdr32.e_shoff)     : ehdr32.e_shoff;
        ehdr.e_shentsize = bswap ? bswap16(ehdr32.e_shentsize) : ehdr32.e_shentsize;
        ehdr.e_shnum     = bswap ? bswap16(ehdr32.e_shnum)     : ehdr32.e_shnum;

        last_shdr_offset = ehdr.e_shoff + (ehdr.e_shentsize * (ehdr.e_shnum - 1));
        fseeko(fp, last_shdr_offset, SEEK_SET);
        if (fread(&shdr32, 1, sizeof(shdr32), fp) != sizeof(shdr32)) {
            LOGE("Failed to read ELF section header");
            goto exit;
        }
        shdr.sh_offset   = bswap ? bswap32(shdr32.sh_offset) : shdr32.sh_offset;
        shdr.sh_size     = bswap ? bswap32(shdr32.sh_size)   : shdr32.sh_size;
    } else if (ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr ehdr64;
        Elf64_Shdr shdr64;
        if (fread(&ehdr64, 1, sizeof(ehdr64), fp) != sizeof(ehdr64)) {
            LOGE("Failed to read elf header");
            goto exit;
        }
        ehdr.e_shoff     = bswap ? bswap64(ehdr64.e_shoff)     : ehdr64.e_shoff;
        ehdr.e_shentsize = bswap ? bswap16(ehdr64.e_shentsize) : ehdr64.e_shentsize;
        ehdr.e_shnum     = bswap ? bswap16(ehdr64.e_shnum)     : ehdr64.e_shnum;

        last_shdr_offset = ehdr.e_shoff + (ehdr.e_shentsize * (ehdr.e_shnum - 1));
        fseeko(fp, last_shdr_offset, SEEK_SET);
        if (fread(&shdr64, 1, sizeof(shdr64), fp) != sizeof(shdr64)) {
            LOGE("Failed to read ELF section header");
            goto exit;
        }
        shdr.sh_offset   = bswap ? bswap64(shdr64.sh_offset) : shdr64.sh_offset;
        shdr.sh_size     = bswap ? bswap64(shdr64.sh_size)   : shdr64.sh_size;
    } else {
        LOGE("Unknown ELF class");
        goto exit;
    }

    sht_end = ehdr.e_shoff + (ehdr.e_shentsize * ehdr.e_shnum);
    last_section_end = shdr.sh_offset + shdr.sh_size;
    size = sht_end > last_section_end ? sht_end : last_section_end;
exit:
    fclose(fp);
    return size;
}

static int keepalive_pipe[2];

static void *write_pipe_thread(void *arg) {
    char c[32];
    while (write(keepalive_pipe[1], c, sizeof(c)) != -1);
    kill(getpid(), SIGTERM);
    return NULL;
}

static void fuse_mounted(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, write_pipe_thread, keepalive_pipe);
}

FORCE_INLINE std::string mount_squashfs() {
    auto exe_path = get_exe_path();
    auto fs_offset = get_elf_size(exe_path.c_str());
    if (fs_offset < 0) {
        LOGE("failed to get size of current elf");
        exit(1);
    }
    char mount_dir[PATH_MAX];
    strcpy(mount_dir, tmpdir());
    strcat(mount_dir, OBF("/ssc"));
    mkdir(mount_dir, 0755);
    strcat(mount_dir, "/XXXXXX");
    if (!mkdtemp(mount_dir)) {
        LOGE("failed to create mount directory");
        exit(1);
    }
    strcat(mount_dir, "/");
    if (pipe(keepalive_pipe) == -1) {
        LOGE("failed to create pipe");
        exit(1);
    }
    int pid = fork();
    if (pid == -1) {
        LOGE("failed to fork");
        exit(1);
    } else if (pid == 0) {
        close(keepalive_pipe[0]);
        
        char options[128];
        sprintf(options, "ro,offset=%ld", fs_offset);
        const char *argv[5] = { exe_path.c_str(), "-o", options, exe_path.c_str(), mount_dir };
        int r = fusefs_main(5, (char**) argv, fuse_mounted);  // daemonize on success
        if (r != 0)
            LOGE("failed to mount squashfs");
        _Exit(r);
    }

    char c;
    close(keepalive_pipe[1]);
    waitpid(pid, NULL, 0);
    if (read(keepalive_pipe[0], &c, 1) <= 0)
        exit(1);
    
    return mount_dir;
}
