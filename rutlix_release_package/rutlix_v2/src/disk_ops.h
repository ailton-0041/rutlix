/*
 * Rutlix Record — Operações de disco (header)
 * src/disk_ops.h
 */
#ifndef DISK_OPS_H
#define DISK_OPS_H

#include <stdint.h>
#include <stddef.h>
#include "iso_detect.h"

#define MAX_PATH_LEN 4096

typedef struct {
    char     name[64];
    char     path[128];
    char     label[128];
    char     size_str[32];
    uint64_t size_bytes;
    int      removable;
} DriveInfo;

typedef struct {
    double   fraction;
    uint64_t bytes_written;
    uint64_t total_bytes;
    double   speed_mb_s;
    int      done;
    int      error;
    char     message[512];
} WriteProgress;

typedef void (*WriteCallback)(const WriteProgress *p, void *user_data);

typedef struct {
    char          iso_path[MAX_PATH_LEN];
    char          device_path[128];
    char          volume_label[32];
    IsoType       iso_type;
    int           partition_scheme; /* 0=MBR 1=GPT */
    int           filesystem;       /* 0=FAT32 1=NTFS 2=exFAT 3=ext4 */
    int           quick_format;
    int           bad_blocks;
    int           bad_block_passes;
    volatile int  cancel;
    WriteCallback callback;
    void         *user_data;
} WriteArgs;

int           disk_scan_removable(DriveInfo *drives, int max);
WriteProgress disk_get_progress(void);
void          disk_format_size(uint64_t bytes, char *out, size_t sz);
void         *disk_write_thread(void *arg);
int           disk_is_valid_iso(const char *path, char *err, size_t err_sz);

#endif /* DISK_OPS_H */
