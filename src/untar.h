#pragma once
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <zlib.h>
#include "utils.h"

// https://mort.coffee/home/tar/
// https://serverfault.com/questions/250511/which-tar-file-format-should-i-use
// https://www.gnu.org/software/tar/manual/html_node/Formats.html
// https://www.gnu.org/software/tar/manual/html_node/Standard.html

#ifdef _MSC_VER
    #define strtoull _strtoui64
    #define snprintf _snprintf
#endif

// ustar
#define TAR_T_REGULAR1 0
#define TAR_T_REGULAR2 '0'
#define TAR_T_HARD '1'
#define TAR_T_SYMBOLIC '2'
#define TAR_T_CHARSPECIAL '3'
#define TAR_T_BLOCKSPECIAL '4'
#define TAR_T_DIRECTORY '5'
#define TAR_T_FIFO '6'
#define TAR_T_CONTIGUOUS '7'
// gnu
#define TAR_T_LONGNAME 'L'
#define TAR_T_LONGLINK 'K'
// pax
#define TAR_T_GLOBALEXTENDED 'g'
#define TAR_T_EXTENDED 'x'


#define TAR_BLOCK_SIZE 512

struct tar_header_s
{
    // v7 (pre-POSIX.1-1988)
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;      // or linkflag
    char linkname[100];

    // ustar (POSIX 1003.1)
    char magic[8];      // 6 bytes magic + 2 bytes version
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    union {
        char prefix[155];
        // gnu
        struct {
            char atime[12];
            char ctime[12];
            // ...
        };
    };
};

typedef struct tar_header_s tar_header_t;

struct tar_header_parsed_s
{
    char *path;
    unsigned long long mode;
    unsigned long long uid;
    unsigned long long gid;
    unsigned long long size;
    double mtime;
    unsigned long long chksum;
    char typeflag;
    char *linkpath;

    char magic[8];
    char *uname;
    char *gname;
    unsigned long long devmajor;
    unsigned long long devminor;

    double atime;
    double ctime;

    char path_buf[257];
    char linkpath_buf[101];
    char uname_buf[33];
    char gname_buf[33];
};

typedef struct tar_header_parsed_s tar_header_parsed_t;

struct pax_header_parsed_s
{
    char has_uid;
    char has_gid;
    char has_size;
    char has_mtime;
    char has_atime;
    char has_ctime;

    char *path;
    unsigned long long uid;
    unsigned long long gid;
    unsigned long long size;
    double mtime;
    double atime;
    double ctime;
    char *linkpath;
    char *uname;
    char *gname;
    // ...
};

typedef struct pax_header_parsed_s pax_header_parsed_t;

struct tar_context_s
{
    int entry_index;
    int empty_count;
    FILE *fp_writer;
    // gnu
    char *longname;
    int longname_wpos;
    char *longlink;
    int longlink_wpos;
    // pax
    char *pax_header;
    int pax_wpos;
    pax_header_parsed_t pax_parsed;
};

typedef struct tar_context_s tar_context_t;

static unsigned long long decode_number(char *buffer, int size)
{
    unsigned long long r = 0;
    unsigned char *p = (unsigned char*)buffer;
    if ((p[0] & 0x80) != 0) {    // base256, gnu
        int negative = p[0] & 0x40;
        r = negative ? p[0] : (p[0] & 0x7f);
        for (int i = 1; i < size; i++) {
            r = (r << 8) | p[i];
        }
    } else {    // oct
        int i = 0;
        for (; i < size && buffer[i] == ' '; i++);
        for (; i < size && buffer[i] >= '0' && buffer[i] <= '7'; i++) {
            r = (r << 3) | (buffer[i] - '0');
        }
    }
    return r;
}

FORCE_INLINE int parse_header(tar_context_t *context, tar_header_t *raw, tar_header_parsed_t *parsed)
{
    memset(parsed, 0, sizeof(tar_header_parsed_t));

    parsed->path = parsed->path_buf;
    parsed->linkpath = parsed->linkpath_buf;
    parsed->uname = parsed->uname_buf;
    parsed->gname = parsed->gname_buf;

    parsed->mode   = decode_number(raw->mode, sizeof(raw->mode));
    parsed->uid    = decode_number(raw->uid, sizeof(raw->uid));
    parsed->gid    = decode_number(raw->gid, sizeof(raw->gid));
    parsed->size   = decode_number(raw->size, sizeof(raw->size));
    parsed->mtime  = decode_number(raw->mtime, sizeof(raw->mtime));
    parsed->chksum = decode_number(raw->chksum, sizeof(raw->chksum));
    parsed->typeflag = raw->typeflag;
    strncpy(parsed->linkpath, raw->linkname, sizeof(raw->linkname));
    memcpy(parsed->magic, raw->magic, sizeof(raw->magic));
    
    strncpy(parsed->uname, raw->uname, sizeof(raw->uname));
    strncpy(parsed->gname, raw->gname, sizeof(raw->gname));
    parsed->devmajor = decode_number(raw->devmajor, sizeof(raw->devmajor));
    parsed->devminor = decode_number(raw->devminor, sizeof(raw->devminor));
    
    if (strcmp(parsed->magic, "ustar") == 0) {  // ustar
        strncpy(parsed->path, raw->prefix, sizeof(raw->prefix));
        if (parsed->path[0])
            strcat(parsed->path, "/");
    } else if (strcmp(parsed->magic, "ustar  ") == 0) {  // gnu
        parsed->atime = decode_number(raw->atime, sizeof(raw->atime));
        parsed->ctime = decode_number(raw->ctime, sizeof(raw->ctime));
    }
    strncat(parsed->path, raw->name, sizeof(raw->name));

    if (context->pax_header) {
        if (context->pax_parsed.path)
            parsed->path = context->pax_parsed.path;
        if (context->pax_parsed.linkpath)
            parsed->linkpath = context->pax_parsed.linkpath;
        if (context->pax_parsed.has_uid)
            parsed->uid = context->pax_parsed.uid;
        if (context->pax_parsed.has_gid)
            parsed->gid = context->pax_parsed.gid;
        if (context->pax_parsed.has_size)
            parsed->size = context->pax_parsed.size;
        if (context->pax_parsed.has_mtime)
            parsed->mtime = context->pax_parsed.mtime;
        if (context->pax_parsed.has_atime)
            parsed->atime = context->pax_parsed.atime;
        if (context->pax_parsed.has_ctime)
            parsed->ctime = context->pax_parsed.ctime;
        if (context->pax_parsed.uname)
            parsed->uname = context->pax_parsed.uname;
        if (context->pax_parsed.gname)
            parsed->gname = context->pax_parsed.gname;
    } else {
        if (context->longname)
            parsed->path = context->longname;
        if (context->longlink)
            parsed->linkpath = context->longlink;
    }

    return 0;
}

// https://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html#tag_20_92_13_03
// TODO: convert charset
FORCE_INLINE int parse_pax_header(tar_context_t *context)
{
    char *line_beg = context->pax_header, *end = line_beg + context->pax_wpos;
    char *line_end, *p;
    while (line_beg < end) {
        unsigned long len = strtoul(line_beg, &p, 10);
        if (!len || *p != ' ')
            return -1;
        if (line_beg[len - 1] != '\n')
            return -2;
        line_beg[len - 1] = '\0';
        line_end = line_beg + len;
        char *key = ++p;
        while (p < line_end && *p != '=')
            ++p;
        if (p >= line_end)
            return -3;
        *p++ = '\0';
        LOGD("Found pax record. entry_index=%d key=%s val=%s", context->entry_index, key, p);
        if (strcmp(key, "path") == 0) {
            context->pax_parsed.path = p;
        } else if (strcmp(key, "linkpath") == 0) {
            context->pax_parsed.linkpath = p;
        } else if (strcmp(key, "size") == 0) {
            context->pax_parsed.size = strtoull(p, NULL, 10);
            context->pax_parsed.has_size = 1;
        } else if (strcmp(key, "uid") == 0) {
            context->pax_parsed.uid = strtoull(p, NULL, 10);
            context->pax_parsed.has_uid = 1;
        } else if (strcmp(key, "gid") == 0) {
            context->pax_parsed.gid = strtoull(p, NULL, 10);
            context->pax_parsed.has_gid = 1;
        } else if (strcmp(key, "mtime") == 0) {
            context->pax_parsed.mtime = strtod(p, NULL);
            context->pax_parsed.has_mtime = 1;
        } else if (strcmp(key, "atime") == 0) {
            context->pax_parsed.atime = strtod(p, NULL);
            context->pax_parsed.has_atime = 1;
        } else if (strcmp(key, "ctime") == 0) {
            context->pax_parsed.ctime = strtod(p, NULL);
            context->pax_parsed.has_ctime = 1;
        } else if (strcmp(key, "uname") == 0) {
            context->pax_parsed.uname= p;
        } else if (strcmp(key, "gname") == 0) {
            context->pax_parsed.gname = p;
        } else {
            LOGD("Ignore pax record. key=%s", key);
        }
        line_beg = line_end;
    }
    return 0;
}

FORCE_INLINE void reset_overrides(tar_context_t *context)
{
    free(context->longname);
    context->longname = NULL;
    context->longname_wpos = 0;

    free(context->longlink);
    context->longlink = NULL;
    context->longlink_wpos = 0;

    free(context->pax_header);
    context->pax_header = NULL;
    context->pax_wpos = 0;
    memset(&context->pax_parsed, 0, sizeof(context->pax_parsed));
}

// TODO: delete file if path exists
FORCE_INLINE int handle_entry_header(tar_context_t *context, tar_header_parsed_t *entry)
{
    LOGD("Found entry. index=%d type=%c path=%s size=%llu", context->entry_index, entry->typeflag, entry->path, entry->size);
    
    switch (entry->typeflag) {
        case TAR_T_REGULAR1:
        case TAR_T_REGULAR2:
        case TAR_T_CONTIGUOUS: {
            size_t len = strlen(entry->path);
            while (--len > 0 && entry->path[len] != '/');
            if (len > 0) {
                entry->path[len] = '\0';
                int rc = mkdir_recursive(entry->path);
                entry->path[len] = '/';
                if (rc != 0) {
                    LOGE("Could not make directory");
                    return -1;
                }
            }
            int fd = open(entry->path, O_WRONLY | O_CREAT | O_TRUNC, entry->mode);
            if (fd < 0) {
                LOGE("Unable to open file for writing");
                return -1;
            }
            if ((context->fp_writer = fdopen(fd, "wb")) == NULL) {
                LOGE("Could not open output file");
                close(fd);
                return -1;
            }
            break;
        }

        case TAR_T_HARD:
            if (link(entry->linkpath, entry->path) < 0) {
                LOGE("Unable to create hardlink");
                return -1;
            }
            break;

        case TAR_T_SYMBOLIC:
            if (symlink(entry->linkpath, entry->path) < 0) {
                LOGE("Unable to create symlink");
                return -1;
            }
            break;

        case TAR_T_CHARSPECIAL:
            if (mknod(entry->path, S_IFCHR | entry->mode, (entry->devmajor << 20) | entry->devminor) < 0) {
                LOGE("Unable to create char device");
                return -1;
            }
            break;

        case TAR_T_BLOCKSPECIAL:
            if (mknod(entry->path, S_IFBLK | entry->mode, (entry->devmajor << 20) | entry->devminor) < 0) {
                LOGE("Unable to create block device");
                return -1;
            }
            break;

        case TAR_T_DIRECTORY:
            if (mkdir_recursive(entry->path, entry->mode) < 0) {
                LOGE("Unable to create directory");
                return -1;
            }
            break;

        case TAR_T_FIFO:
            if (mkfifo(entry->path, entry->mode) < 0) {
                LOGE("Unable to create fifo");
                return -1;
            }
            break;
        
        case TAR_T_LONGNAME:
            free(context->longname);
            context->longname_wpos = 0;
            context->longname = (char*) malloc(entry->size);
            if (!context->longname) {
                LOGE("Unable to alloc memory for long name! size=%d", entry->size);
                return -1;
            }
            break;
        
        case TAR_T_LONGLINK:
            free(context->longlink);
            context->longlink_wpos = 0;
            context->longlink = (char*) malloc(entry->size);
            if (!context->longlink) {
                LOGE("Unable to alloc memory for long linkname! size=%d", entry->size);
                return -1;
            }
            break;
        
        case TAR_T_GLOBALEXTENDED:
            break;      // ignore for now
        case TAR_T_EXTENDED:
            memset(&context->pax_parsed, 0, sizeof(context->pax_parsed));
            free(context->pax_header);
            context->pax_wpos = 0;
            context->pax_header = (char*) malloc(entry->size);
            if (!context->pax_header) {
                LOGE("Unable to alloc memory for pax header! size=%d", entry->size);
                return -1;
            }
            break;
        
        default:
            LOGD("Ignore entry. entry_index=%d type=%c path=%s", context->entry_index, entry->typeflag, entry->path);
            break;
    }

    return 0;
}

FORCE_INLINE int handle_entry_data(tar_context_t *context, tar_header_parsed_t *entry, unsigned char *block, int length)
{
    switch (entry->typeflag) {
        case TAR_T_LONGNAME:
            memcpy(context->longname + context->longname_wpos, block, length);
            context->longname_wpos += length;
            break;
        
        case TAR_T_LONGLINK:
            memcpy(context->longlink + context->longlink_wpos, block, length);
            context->longlink_wpos += length;
            break;
        
        case TAR_T_GLOBALEXTENDED:
            break;
        case TAR_T_EXTENDED:
            memcpy(context->pax_header + context->pax_wpos, block, length);
            context->pax_wpos += length;
            break;

        default:
            if (context->fp_writer != NULL)
                if (fwrite(block, 1, length, context->fp_writer) != length)
                    LOGE("Failed to write to output file!");
            break;
    }

    return 0;
}

FORCE_INLINE int handle_entry_end(tar_context_t *context, tar_header_parsed_t *entry)
{
    if (context->fp_writer != NULL) {
        fclose(context->fp_writer);
        context->fp_writer = NULL;
    }

    switch (entry->typeflag) {
        case TAR_T_LONGNAME:
        case TAR_T_LONGLINK:
            break;
        case TAR_T_GLOBALEXTENDED:
            break;      // ignore for now
        case TAR_T_EXTENDED: {
            int r = parse_pax_header(context);
            if (r != 0)
                LOGE("Failed to parse pax header! ret=%d", r);
            break;
        }
        default: {
            // FIXME: directory mtime should be set after all files in it have been extracted
            struct stat st;
            if (lstat(entry->path, &st) == 0) {
                struct timeval tvs[2];
                tvs[0].tv_sec = time(NULL);     // atime should be set to now, not atime in archive
                tvs[0].tv_usec = 0;
                tvs[1].tv_sec = (long) entry->mtime;
                tvs[1].tv_usec = (long) ((entry->mtime - tvs[1].tv_sec) * 1000000);
                if (lutimes(entry->path, tvs) < 0)
                    LOGE("Unable to set mtime and atime");
            }
            reset_overrides(context);
            break;
        }
    }

    return 0;
}

FORCE_INLINE int read_block(FILE *fp, unsigned char *buffer)
{
    int num_read = fread(buffer, 1, TAR_BLOCK_SIZE, fp);
    if (num_read < TAR_BLOCK_SIZE) {
        LOGE("Not enough data to read! num_read=%d", num_read);
        return -1;
    }
    return 0;
}

FORCE_INLINE int untar(FILE *fp)
{
    int i;
    int remain_size, current_size;
    unsigned char buffer[TAR_BLOCK_SIZE + 1];

    tar_header_parsed_t header_parsed;
    tar_context_t context;
    memset(&context, 0, sizeof(context));

    while (context.empty_count < 2) {

        if (read_block(fp, buffer) != 0)
            break;

        for (i = 0; i < TAR_BLOCK_SIZE && !buffer[i]; i++);
        if (i >= TAR_BLOCK_SIZE) {
            context.empty_count++;
            context.entry_index++;
            continue;
        }
        context.empty_count = 0;
        
        if (parse_header(&context, (tar_header_t*) buffer, &header_parsed) != 0)
            break;

        if (handle_entry_header(&context, &header_parsed) != 0)
            break;
    
        remain_size = header_parsed.size;
        while (remain_size > 0) {
            if (read_block(fp, buffer) != 0)
                break;

            current_size = remain_size < TAR_BLOCK_SIZE ? remain_size : TAR_BLOCK_SIZE;
            buffer[current_size] = 0;

            if (handle_entry_data(&context, &header_parsed, buffer, current_size) != 0)
                break;
            
            remain_size -= current_size;
        }

        handle_entry_end(&context, &header_parsed);

        if (remain_size > 0)
            break;

        context.entry_index++;
    }

    reset_overrides(&context);
    return context.empty_count < 2 ? -1 : 0;
}

FORCE_INLINE int gunzip(char *data, int size, int fd)
{
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    zs.next_in = (Bytef*)data;
    zs.avail_in = size;

    if (inflateInit2(&zs, (15 + 32)) != Z_OK) {
        LOGE("inflateInit failed while decompressing.");
        return -1;
    }

    int ret;
    char outbuffer[8192];
    int outsize = 0;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = (Bytef*)outbuffer;
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (outsize < zs.total_out) {
            write(fd, outbuffer, zs.total_out - outsize);
            outsize = zs.total_out;
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        LOGE("Exception during zlib decompression!");
        return -1;
    }

    return 0;
}

FORCE_INLINE int extract_tar_gz_from_mem(char *data, int size)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        LOGE("Failed to create pipe");
        return -1;
    }

    int p = fork();
    if (p < 0) {
        LOGE("failed to fork process!");
        return -1;

    } else if (p > 0) { // parent process
        close(pipe_fds[1]);
        int r;
        FILE *fp = fdopen(pipe_fds[0], "rb");
        if (fp != NULL) {
            r = untar(fp);
            fclose(fp);
        } else {
            LOGE("Could not open pipe fd.");
            r = -1;
            close(pipe_fds[0]);
        }

        kill(p, SIGKILL);
        waitpid(p, 0, 0);
        return r;

    } else { // child process
        close(pipe_fds[0]);
        int r = gunzip(data, size, pipe_fds[1]);
        close(pipe_fds[1]);
        exit(r);
    }
}
