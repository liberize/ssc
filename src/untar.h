#pragma once
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include <thread>
#include "utils.h"

#define GET_NUM_BLOCKS(filesize) (int)ceil((double)filesize / (double)TAR_BLOCK_SIZE)

#ifdef _MSC_VER
    #define strtoull _strtoui64
    #define snprintf _snprintf
#endif

#define TAR_T_NORMAL1 0
#define TAR_T_NORMAL2 '0'
#define TAR_T_HARD '1'
#define TAR_T_SYMBOLIC '2'
#define TAR_T_CHARSPECIAL '3'
#define TAR_T_BLOCKSPECIAL '4'
#define TAR_T_DIRECTORY '5'
#define TAR_T_FIFO '6'
#define TAR_T_CONTIGUOUS '7'
#define TAR_T_GLOBALEXTENDED 'g'
#define TAR_T_EXTENDED 'x'

#define TAR_BLOCK_SIZE 512

#define TAR_HT_PRE11988 1
#define TAR_HT_P10031 2


// Describes a header for TARs conforming to pre-POSIX.1-1988 .
struct header_s
{
    char filename[100];
    char filemode[8];
    char uid[8];
    char gid[8];
    char filesize[12];
    char mtime[12];
    char checksum[8];
    char type;
    char link_target[100];

    char ustar_indicator[6];
    char ustar_version[2];
    char user_name[32];
    char group_name[32];
    char device_major[8];
    char device_minor[8];
};

typedef struct header_s header_t;

struct header_translated_s
{
    char filename[101];
    unsigned long long filemode;
    unsigned long long uid;
    unsigned long long gid;
    unsigned long long filesize;
    unsigned long long mtime;
    unsigned long long checksum;
    char type;
    char link_target[101];

    char ustar_indicator[6];
    char ustar_version[3];
    char user_name[32];
    char group_name[32];
    unsigned long long device_major;
    unsigned long long device_minor;
};

typedef struct header_translated_s header_translated_t;


FORCE_INLINE int parse_header(const unsigned char buffer[512], header_t *header)
{
    memcpy(header, buffer, sizeof(header_t));
    return 0;
}

static char *trim(char *raw, int length)
{
    int i = 0;
    int j = length - 1;

    // Determine left padding.
    while ((raw[i] == 0 || raw[i] == ' ')) {
        i++;
        if (i >= length) {
            raw[0] = '\0';
            return raw;
        }
    }

    // Determine right padding.
    while ((raw[j] == 0 || raw[j] == ' ')) {
        j--;
        if (j <= i)
            break;
    }

    // Place the terminator.
    raw[j + 1] = 0;

    // Return an offset pointer.
    return &raw[i];
}

static unsigned long long decode_number(char *buffer, int size)
{
    unsigned char *p = (unsigned char*)buffer;
    if ((p[0] & 0x80) != 0) {    // base256
        int negative = p[0] & 0x40;
        unsigned long long r = negative ? p[0] : (p[0] & 0x7f);
        for (int i = 1; i < size; i++) {
            r = (r << 8) | p[i];
        }
        return r;
    } else {
        char *buffer_ptr = trim(buffer, size);
        return strtoull(buffer_ptr, NULL, 8);
    }
}

FORCE_INLINE int translate_header(header_t *raw_header, header_translated_t *parsed)
{
    char buffer[101];
    char *buffer_ptr;

    memcpy(buffer, raw_header->filename, 100);
    buffer_ptr = trim(buffer, 100);
    strcpy(parsed->filename, buffer_ptr);

    memcpy(buffer, raw_header->filemode, 8);
    parsed->filemode = decode_number(buffer, 8);

    memcpy(buffer, raw_header->uid, 8);
    parsed->uid = decode_number(buffer, 8);

    memcpy(buffer, raw_header->gid, 8);
    parsed->gid = decode_number(buffer, 8);

    memcpy(buffer, raw_header->filesize, 12);
    parsed->filesize = decode_number(buffer, 12);

    memcpy(buffer, raw_header->mtime, 12);
    parsed->mtime = decode_number(buffer, 12);

    memcpy(buffer, raw_header->checksum, 8);
    parsed->checksum = decode_number(buffer, 8);

    parsed->type = raw_header->type;

    memcpy(buffer, raw_header->link_target, 100);
    buffer_ptr = trim(buffer, 100);
    strcpy(parsed->link_target, buffer_ptr);

    memcpy(buffer, raw_header->ustar_indicator, 6);
    buffer_ptr = trim(buffer, 6);
    strcpy(parsed->ustar_indicator, buffer_ptr);
    
    memcpy(buffer, raw_header->ustar_version, 2);
    buffer_ptr = trim(buffer, 2);
    strcpy(parsed->ustar_version, buffer_ptr);
    
    if (strcmp(parsed->ustar_indicator, "ustar") == 0) {
        memcpy(buffer, raw_header->user_name, 32);
        buffer_ptr = trim(buffer, 32);
        strcpy(parsed->user_name, buffer_ptr);

        memcpy(buffer, raw_header->group_name, 32);
        buffer_ptr = trim(buffer, 32);
        strcpy(parsed->group_name, buffer_ptr);

        memcpy(buffer, raw_header->device_major, 8);
        parsed->device_major = decode_number(buffer, 8);

        memcpy(buffer, raw_header->device_minor, 8);
        parsed->device_minor = decode_number(buffer, 8);
    } else {
        strcpy(parsed->user_name, "");
        strcpy(parsed->group_name, "");

        parsed->device_major = 0;
        parsed->device_minor = 0;
    }

    return 0;
}

FORCE_INLINE int read_block(FILE *fp, unsigned char *buffer)
{
    int num_read = fread(buffer, 1, TAR_BLOCK_SIZE, fp);
    if (num_read < TAR_BLOCK_SIZE) {
        errln("Not enough data to read");
        return -1;
    }
    return 0;
}

FORCE_INLINE int entry_header_cb(header_translated_t *entry, int entry_index, FILE **fp_writer)
{
    //printf("entry %d: filename %s type %c\n", entry_index, entry->filename, entry->type);
    switch (entry->type) {
        case TAR_T_NORMAL1:
        case TAR_T_NORMAL2:
        case TAR_T_CONTIGUOUS: {
            size_t len = strlen(entry->filename);
            while (--len > 0 && entry->filename[len] != '/');
            if (len > 0) {
                entry->filename[len] = '\0';
                int rc = mkdir_recursive(entry->filename);
                entry->filename[len] = '/';
                if (rc != 0) {
                    perror("Could not make directory");
                    return -1;
                }
            }
            int fd = open(entry->filename, O_WRONLY | O_CREAT | O_TRUNC, entry->filemode);
            if (fd < 0) {
                perror("Unable to open file for writing");
                return -1;
            }
            if ((*fp_writer = fdopen(fd, "wb")) == NULL) {
                perror("Could not open output file");
                close(fd);
                return -1;
            }
            break;
        }

        case TAR_T_HARD:
            if (link(entry->link_target, entry->filename) < 0) {
                perror("Unable to create hardlink");
                return -1;
            }
            break;

        case TAR_T_SYMBOLIC:
            if (symlink(entry->link_target, entry->filename) < 0) {
                perror("Unable to create symlink");
                return -1;
            }
            break;

        case TAR_T_CHARSPECIAL:
            if (mknod(entry->filename, S_IFCHR | entry->filemode, (entry->device_major << 20) | entry->device_minor) < 0) {
                perror("Unable to create char device");
                return -1;
            }
            break;

        case TAR_T_BLOCKSPECIAL:
            if (mknod(entry->filename, S_IFBLK | entry->filemode, (entry->device_major << 20) | entry->device_minor) < 0) {
                perror("Unable to create block device");
                return -1;
            }
            break;

        case TAR_T_DIRECTORY:
            if (mkdir_recursive(entry->filename, entry->filemode) < 0) {
                perror("Unable to create directory");
                return -1;
            }
            break;

        case TAR_T_FIFO:
            if (mkfifo(entry->filename, entry->filemode) < 0) {
                perror("Unable to create fifo");
                return -1;
            }
            break;
        
    }

    return 0;
}

FORCE_INLINE int entry_data_cb(header_translated_t *entry, int entry_index, FILE *fp_writer, unsigned char *block, int length)
{
    if (fp_writer != NULL)
        if (fwrite(block, 1, length, fp_writer) != length)
            errln("Failed to write to output file");

    return 0;
}

FORCE_INLINE int entry_end_cb(header_translated_t *entry, int entry_index, FILE *fp_writer)
{
    if (fp_writer != NULL) {
        fclose(fp_writer);
        fp_writer = NULL;
    }
    return 0;
}

FORCE_INLINE int get_last_block_portion_size(int filesize)
{
    const int partial = filesize % TAR_BLOCK_SIZE;
    return (partial > 0 ? partial : TAR_BLOCK_SIZE);
}

FORCE_INLINE int extract_tar(FILE *fp)
{
    unsigned char buffer[TAR_BLOCK_SIZE + 1];
    int header_checked = 0;
    int i;

    header_t header;
    header_translated_t header_translated;

    int num_blocks;
    int current_data_size;
    int entry_index = 0;
    int empty_count = 0;

    buffer[TAR_BLOCK_SIZE] = 0;

    // The end of the file is represented by two empty entries (which we 
    // expediently identify by filename length).
    while (empty_count < 2) {
        if (read_block(fp, buffer) != 0)
            return -2;

        // If we haven't yet determined what format to support, read the 
        // header of the next entry, now. This should be done only at the 
        // top of the archive.

        if (parse_header(buffer, &header) != 0) {
            errln("Could not understand the header of the first entry in the TAR.");
            return -3;
        }

        if (strlen(header.filename) == 0) {
            empty_count++;
        } else {
            if (translate_header(&header, &header_translated) != 0) {
                errln("Could not translate header.");
                return -4;
            }

            FILE *fp_writer = NULL;
            if (entry_header_cb(&header_translated, entry_index, &fp_writer) != 0)
                return -5;
        
            i = 0;
            int received_bytes = 0;
            num_blocks = GET_NUM_BLOCKS(header_translated.filesize);
            while (i < num_blocks) {
                if (read_block(fp, buffer) != 0) {
                    errln("Could not read block. File too short.");
                    break;
                }

                if (i >= num_blocks - 1)
                    current_data_size = get_last_block_portion_size(header_translated.filesize);
                else
                    current_data_size = TAR_BLOCK_SIZE;

                buffer[current_data_size] = 0;

                if (entry_data_cb(&header_translated, entry_index, fp_writer, buffer, current_data_size) != 0)
                    break;

                i++;
                received_bytes += current_data_size;
            }

            entry_end_cb(&header_translated, entry_index, fp_writer);

            if (i < num_blocks)
                return -6;
        }

        entry_index++;
    }

    return 0;
}

FORCE_INLINE int gunzip(char *data, int size, int fd)
{
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    zs.next_in = (Bytef*)data;
    zs.avail_in = size;

    if (inflateInit2(&zs, (15 + 32)) != Z_OK) {
        errln("inflateInit failed while decompressing.");
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
        errln("Exception during zlib decompression");
        return -1;
    }

    return 0;
}

FORCE_INLINE int extract_tar_gz_from_mem(char *data, int size)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("Failed to create pipe");
        return -1;
    }
    FILE *fp = fdopen(pipe_fds[0], "rb");
    if (fp == NULL) {
        errln("Could not open pipe fd.");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }
    std::thread t([=] {
        gunzip(data, size, pipe_fds[1]);
        close(pipe_fds[1]);
    });
    int r = extract_tar(fp);
    fclose(fp);
    t.join();
    return r;
}
