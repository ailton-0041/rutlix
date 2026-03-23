/*
 * IsoBurner v2.0 - Operações de disco
 * src/disk_ops.c
 */

#define _GNU_SOURCE
#include "disk_ops.h"
#include "iso_detect.h"
#include "asm_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>

/* =========================================================
 * Progresso global (thread-safe)
 * ========================================================= */
static WriteProgress   g_progress;
static pthread_mutex_t g_pmutex = PTHREAD_MUTEX_INITIALIZER;

static void set_progress(double frac, uint64_t written, uint64_t total,
                         double speed, int done, int error,
                         const char *fmt, ...) {
    char msg[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&g_pmutex);
    g_progress.fraction      = frac;
    g_progress.bytes_written = written;
    g_progress.total_bytes   = total;
    g_progress.speed_mb_s    = speed;
    g_progress.done          = done;
    g_progress.error         = error;
    /* safe copy — deixa null terminator garantido */
    memset(g_progress.message, 0, sizeof(g_progress.message));
    memcpy(g_progress.message, msg,
           strlen(msg) < sizeof(g_progress.message) - 1
               ? strlen(msg) : sizeof(g_progress.message) - 1);
    pthread_mutex_unlock(&g_pmutex);
}

WriteProgress disk_get_progress(void) {
    WriteProgress p;
    pthread_mutex_lock(&g_pmutex);
    p = g_progress;
    pthread_mutex_unlock(&g_pmutex);
    return p;
}

/* =========================================================
 * Utilitários sysfs
 * ========================================================= */
/* buffers generosos para evitar truncation nos paths */
#define SYSBUF 512

static int sysfs_read(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *r = fgets(buf, (int)sz, f);
    fclose(f);
    if (!r) return 0;
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    return 1;
}

void disk_format_size(uint64_t bytes, char *out, size_t sz) {
    if (!bytes)                 snprintf(out, sz, "0 B");
    else if (bytes >= 1ULL<<40) snprintf(out, sz, "%.2f TB", bytes/(double)(1ULL<<40));
    else if (bytes >= 1ULL<<30) snprintf(out, sz, "%.1f GB", bytes/(double)(1ULL<<30));
    else if (bytes >= 1ULL<<20) snprintf(out, sz, "%.1f MB", bytes/(double)(1ULL<<20));
    else                        snprintf(out, sz, "%llu KB",(unsigned long long)(bytes>>10));
}

int disk_scan_removable(DriveInfo *drives, int max) {
    DIR *dir = opendir("/sys/block");
    if (!dir) return 0;
    int count = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) && count < max) {
        const char *n = ent->d_name;
        if (n[0] == '.') continue;
        /* só sdX e mmcblkX */
        if (strncmp(n,"sd",2)!=0 && strncmp(n,"mmcblk",6)!=0) continue;

        /* buffers amplos para evitar truncation */
        char p[SYSBUF]; char buf[64];

        /* removable? */
        snprintf(p, sizeof(p), "/sys/block/%.200s/removable", n);
        if (!sysfs_read(p, buf, sizeof(buf)) || buf[0] != '1') continue;

        DriveInfo *d = &drives[count];
        memset(d, 0, sizeof(*d));

        /* name, path */
        snprintf(d->name, sizeof(d->name), "%.60s", n);
        snprintf(d->path, sizeof(d->path), "/dev/%.120s", n);
        d->removable = 1;

        /* size */
        snprintf(p, sizeof(p), "/sys/block/%.200s/size", n);
        unsigned long long sectors = 0;
        if (sysfs_read(p, buf, sizeof(buf))) sscanf(buf, "%llu", &sectors);
        d->size_bytes = sectors * 512ULL;
        disk_format_size(d->size_bytes, d->size_str, sizeof(d->size_str));

        /* vendor / model */
        char vendor[64]={0}, model[64]={0};
        snprintf(p, sizeof(p), "/sys/block/%.200s/device/vendor", n);
        sysfs_read(p, vendor, sizeof(vendor));
        snprintf(p, sizeof(p), "/sys/block/%.200s/device/model", n);
        sysfs_read(p, model, sizeof(model));

        if (vendor[0] && model[0])
            snprintf(d->label, sizeof(d->label), "%.60s %.60s", vendor, model);
        else if (model[0])
            snprintf(d->label, sizeof(d->label), "%.120s", model);
        else
            snprintf(d->label, sizeof(d->label), "Pendrive USB");

        count++;
    }
    closedir(dir);
    return count;
}

/* =========================================================
 * Executa comando externo sem dependência de shell
 * ========================================================= */
static int run_cmd(const char **argv) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull,1); dup2(devnull,2); close(devnull); }
        execvp(argv[0], (char *const*)argv);
        _exit(127);
    }
    int st = 0; waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* =========================================================
 * Copia um fd aberto para um caminho, reportando progresso
 * ========================================================= */
static int copy_fd_to_path(int src_fd, const char *dst_path,
                            uint64_t *wtotal, uint64_t total,
                            WriteArgs *wa, struct timespec *t0) {
    int dst = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) return 0;

    const size_t BSZ = 4 * 1024 * 1024;
    uint8_t *buf = (uint8_t*)malloc(BSZ);
    if (!buf) { close(dst); return 0; }

    ssize_t n; int ok = 1;
    while (!wa->cancel && (n = read(src_fd, buf, BSZ)) > 0) {
        if (write(dst, buf, (size_t)n) != n) { ok = 0; break; }
        *wtotal += (uint64_t)n;

        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double e = (now.tv_sec-t0->tv_sec)+(now.tv_nsec-t0->tv_nsec)*1e-9;
        double sp = e>0 ? (*wtotal/e/(1024.0*1024.0)) : 0;
        double fr = total>0 ? (double)*wtotal/(double)total : 0;
        set_progress(fr, *wtotal, total, sp, 0, 0,
                     "Gravando... %.1f MB/s  (%.0f%%)", sp, fr*100);
        if (wa->callback) {
            WriteProgress p = disk_get_progress();
            wa->callback(&p, wa->user_data);
        }
    }
    free(buf); fsync(dst); close(dst);
    return ok;
}

/* =========================================================
 * Split WIM — C puro + ferramentas opcionais
 * ========================================================= */
#define WIM_MAGIC_STR  "MSWIM\0\0\0"
#define WIM_HDR_SZ     208
#define WIM_FLAG_SPANNED 0x00000020U
#define WIM_PART_BYTES  (3800ULL * 1024 * 1024)

static int split_wim_pure(const char *src_wim, const char *dst_dir,
                           uint64_t *wtotal, uint64_t total,
                           WriteArgs *wa, struct timespec *t0);

static int split_wim(const char *src_wim, const char *dst_dir,
                     uint64_t *wtotal, uint64_t total,
                     WriteArgs *wa, struct timespec *t0) {
    /* Tamanho máximo para dst_dir + "/install.swm" = 12 chars + null */
    char swm1[MAX_PATH_LEN];
    snprintf(swm1, sizeof(swm1), "%.*s/install.swm",
             (int)(sizeof(swm1) - 14), dst_dir);

    /* Tenta ferramentas externas primeiro */
    struct { const char *bin; const char *argv[7]; } tools[] = {
        { "wimsplit",
          {"wimsplit", src_wim, swm1, "3800", NULL} },
        { "wimlib-imagex",
          {"wimlib-imagex", "split", src_wim, swm1, "3800", NULL} },
        { NULL, {NULL} }
    };

    for (int t = 0; tools[t].bin; t++) {
        /* which via access em PATH */
        char try_path[512];
        snprintf(try_path, sizeof(try_path),
                 "which %.400s >/dev/null 2>&1", tools[t].bin);
        if (system(try_path) == 0) {        /* NOLINT */
            set_progress((double)*wtotal/(double)(total?total:1),
                         *wtotal, total, 0,0,0,
                         "Split WIM com %s...", tools[t].bin);
            if (wa->callback) {
                WriteProgress p = disk_get_progress();
                wa->callback(&p, wa->user_data);
            }
            if (run_cmd((const char**)tools[t].argv) == 0) {
                /* acumula bytes das partes geradas */
                for (int i = 1; i <= 99; i++) {
                    char sp[MAX_PATH_LEN];
                    if (i == 1)
                        snprintf(sp, sizeof(sp), "%.*s/install.swm",
                                 (int)(sizeof(sp)-14), dst_dir);
                    else
                        snprintf(sp, sizeof(sp), "%.*s/install%d.swm",
                                 (int)(sizeof(sp)-16), dst_dir, i);
                    struct stat st;
                    if (stat(sp, &st) != 0) break;
                    *wtotal += (uint64_t)st.st_size;
                }
                return 1;
            }
        }
    }

    /* Implementação C pura */
    return split_wim_pure(src_wim, dst_dir, wtotal, total, wa, t0);
}

static int split_wim_pure(const char *src_wim, const char *dst_dir,
                           uint64_t *wtotal, uint64_t total,
                           WriteArgs *wa, struct timespec *t0) {
    set_progress((double)*wtotal/(double)(total?total:1),
                 *wtotal, total, 0,0,0, "Dividindo install.wim (C puro)...");
    if (wa->callback) {
        WriteProgress p = disk_get_progress(); wa->callback(&p, wa->user_data);
    }

    int src = open(src_wim, O_RDONLY);
    if (src < 0) goto fallback;

    uint8_t hdr[WIM_HDR_SZ];
    if (read(src, hdr, WIM_HDR_SZ) != WIM_HDR_SZ ||
        memcmp(hdr, WIM_MAGIC_STR, 8) != 0) {
        close(src); goto fallback;
    }

    struct stat st; fstat(src, &st);
    uint64_t wim_sz = (uint64_t)st.st_size;
    uint16_t nparts = (uint16_t)((wim_sz + WIM_PART_BYTES - 1) / WIM_PART_BYTES);
    if (nparts < 2) nparts = 2;
    if (nparts > 99) nparts = 99;

    lseek(src, 0, SEEK_SET);

    const size_t BSZ = 4 * 1024 * 1024;
    uint8_t *buf = (uint8_t*)malloc(BSZ);
    if (!buf) { close(src); goto fallback; }

    uint64_t remaining = wim_sz;
    int ok = 1;

    for (uint16_t part = 1; part <= nparts && !wa->cancel; part++) {
        char ppath[MAX_PATH_LEN];
        if (part == 1)
            snprintf(ppath, sizeof(ppath), "%.*s/install.swm",
                     (int)(sizeof(ppath)-14), dst_dir);
        else
            snprintf(ppath, sizeof(ppath), "%.*s/install%d.swm",
                     (int)(sizeof(ppath)-16), dst_dir, (int)part);

        int dst = open(ppath, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (dst < 0) { ok = 0; break; }

        uint64_t part_sz = remaining > WIM_PART_BYTES ? WIM_PART_BYTES : remaining;
        uint64_t pw = 0;

        while (pw < part_sz && !wa->cancel) {
            size_t want = BSZ;
            if ((uint64_t)want > part_sz - pw) want = (size_t)(part_sz - pw);
            ssize_t n = read(src, buf, want);
            if (n <= 0) break;

            /* Patch cabeçalho na primeira leitura de cada parte */
            if (pw == 0 && (size_t)n >= WIM_HDR_SZ) {
                memcpy(buf, hdr, WIM_HDR_SZ);          /* restaura header original */
                uint32_t flags;
                memcpy(&flags, buf+16, 4);
                flags |= WIM_FLAG_SPANNED;
                memcpy(buf+16, &flags, 4);
                uint16_t pn = part, tp = nparts;
                memcpy(buf+40, &pn, 2);
                memcpy(buf+42, &tp, 2);
            }

            if (write(dst, buf, (size_t)n) != n) { ok = 0; break; }
            pw      += (uint64_t)n;
            *wtotal += (uint64_t)n;

            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            double e  = (now.tv_sec-t0->tv_sec)+(now.tv_nsec-t0->tv_nsec)*1e-9;
            double sp = e>0 ? (*wtotal/e/(1024.0*1024.0)) : 0;
            double fr = total>0 ? (double)*wtotal/(double)total : 0;
            set_progress(fr,*wtotal,total,sp,0,0,
                "Split WIM %d/%d — %.1f MB/s  (%.0f%%)",
                (int)part,(int)nparts,sp,fr*100.0);
            if (wa->callback) {
                WriteProgress p = disk_get_progress(); wa->callback(&p,wa->user_data);
            }
        }
        fsync(dst); close(dst);
        remaining -= part_sz;
        if (remaining == 0) break;
    }
    free(buf); close(src);
    return ok;

fallback:
    /* NTFS suporta >4 GB: copia diretamente */
    {
        char dst_wim[MAX_PATH_LEN];
        snprintf(dst_wim, sizeof(dst_wim), "%.*s/install.wim",
                 (int)(sizeof(dst_wim)-14), dst_dir);
        int s = open(src_wim, O_RDONLY);
        if (s < 0) return 0;
        int r = copy_fd_to_path(s, dst_wim, wtotal, total, wa, t0);
        close(s);
        return r;
    }
}

/* =========================================================
 * Cópia recursiva (ISO montada → pendrive montado)
 * ========================================================= */
#define IS_WIM(n) (strcasecmp((n),"install.wim")==0 || \
                   strcasecmp((n),"install.esd")==0)
#define GB4 (4ULL*1024*1024*1024)

static void copy_recursive(const char *src_dir, const char *dst_dir,
                            uint64_t *wtotal, uint64_t total,
                            WriteArgs *wa, struct timespec *t0,
                            int do_split);

static void copy_one_file(const char *src_path, const char *dst_path,
                           const char *fname,
                           uint64_t *wtotal, uint64_t total,
                           WriteArgs *wa, struct timespec *t0,
                           int do_split) {
    struct stat st;
    if (stat(src_path, &st) != 0) return;

    if (do_split && IS_WIM(fname) && (uint64_t)st.st_size >= GB4) {
        /* extrai diretório destino do path completo */
        char dst_dir_buf[MAX_PATH_LEN];
        snprintf(dst_dir_buf, sizeof(dst_dir_buf), "%.*s",
                 (int)(sizeof(dst_dir_buf)-1), dst_path);
        char *slash = strrchr(dst_dir_buf, '/');
        if (slash) *slash = '\0';
        else snprintf(dst_dir_buf, sizeof(dst_dir_buf), ".");
        split_wim(src_path, dst_dir_buf, wtotal, total, wa, t0);
        return;
    }

    int s = open(src_path, O_RDONLY);
    if (s < 0) return;
    copy_fd_to_path(s, dst_path, wtotal, total, wa, t0);
    close(s);
}

static void copy_recursive(const char *src_dir, const char *dst_dir,
                            uint64_t *wtotal, uint64_t total,
                            WriteArgs *wa, struct timespec *t0,
                            int do_split) {
    mkdir(dst_dir, 0755);

    DIR *d = opendir(src_dir);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) && !wa->cancel) {
        if (ent->d_name[0] == '.') continue;

        char sp[MAX_PATH_LEN], dp[MAX_PATH_LEN];
        snprintf(sp, sizeof(sp), "%.*s/%.*s",
                 (int)(sizeof(sp)/2-2), src_dir,
                 (int)(sizeof(sp)/2-2), ent->d_name);
        snprintf(dp, sizeof(dp), "%.*s/%.*s",
                 (int)(sizeof(dp)/2-2), dst_dir,
                 (int)(sizeof(dp)/2-2), ent->d_name);

        struct stat st;
        if (lstat(sp, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            copy_recursive(sp, dp, wtotal, total, wa, t0, do_split);
        } else if (S_ISREG(st.st_mode)) {
            char msg[300];
            snprintf(msg, sizeof(msg), "Copiando: %.250s", ent->d_name);
            set_progress((double)*wtotal/(double)(total?total:1),
                         *wtotal, total, 0,0,0, "%s", msg);
            if (wa->callback) {
                WriteProgress p = disk_get_progress(); wa->callback(&p,wa->user_data);
            }
            copy_one_file(sp, dp, ent->d_name, wtotal, total, wa, t0, do_split);
        }
    }
    closedir(d);
}

/* =========================================================
 * Gravação Linux ISO — dd raw direto
 * ========================================================= */
static void write_linux_iso(WriteArgs *wa) {
    const size_t BSZ = 4 * 1024 * 1024;

    int src = open(wa->iso_path, O_RDONLY);
    if (src < 0) {
        set_progress(0,0,0,0,0,1,"Erro ao abrir ISO: %.200s", strerror(errno));
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
        return;
    }

    int dst = open(wa->device_path, O_WRONLY|O_SYNC);
    if (dst < 0) {
        set_progress(0,0,0,0,0,1,
            "Erro ao abrir %.100s: %.100s\nExecute com sudo.",
            wa->device_path, strerror(errno));
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
        close(src); return;
    }

    uint64_t total   = asm_get_file_size(wa->iso_path);
    uint64_t written = 0;
    uint8_t *buf = (uint8_t*)malloc(BSZ);
    if (!buf) {
        set_progress(0,0,0,0,0,1,"Sem memória para buffer");
        close(src); close(dst); return;
    }

    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    ssize_t n;

    while (!wa->cancel && (n = read(src, buf, BSZ)) > 0) {
        if (write(dst, buf, (size_t)n) != n) {
            set_progress(0,written,total,0,0,1,
                         "Erro de escrita: %.200s", strerror(errno));
            if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
            free(buf); close(src); close(dst); return;
        }
        written += (uint64_t)n;
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double e  = (now.tv_sec-t0.tv_sec)+(now.tv_nsec-t0.tv_nsec)*1e-9;
        double sp = e>0 ? written/e/(1024.0*1024.0) : 0;
        double fr = total>0 ? (double)written/(double)total : 0;
        set_progress(fr,written,total,sp,0,0,
                     "Gravando... %.1f MB/s  (%.0f%%)",sp,fr*100.0);
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
    }

    fsync(dst); free(buf); close(src); close(dst);

    if (!wa->cancel)
        set_progress(1.0,written,total,0,1,0,"✓ Gravação concluída com sucesso!");
    else
        set_progress(0,written,total,0,0,1,"Gravação cancelada.");
    if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
}

/* =========================================================
 * Gravação Windows ISO — arquivo-a-arquivo com split WIM
 * ========================================================= */
static void write_windows_iso(WriteArgs *wa) {
    char tmp_iso[64] = "/tmp/isoburner_iso_XXXXXX";
    char tmp_usb[64] = "/tmp/isoburner_usb_XXXXXX";
    /* partição com buffer generoso */
    char dev_part[256] = {0};

    if (!mkdtemp(tmp_iso) || !mkdtemp(tmp_usb)) {
        set_progress(0,0,0,0,0,1,"Erro ao criar diretório temporário: %.200s",
                     strerror(errno));
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
        return;
    }

    set_progress(0.01,0,0,0,0,0,"Preparando dispositivo...");
    if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }

    /* Desmonta partições existentes */
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "umount %.200s* 2>/dev/null; sync", wa->device_path);
        if (system(cmd) != 0) { /* ignora erros de umount */ }
    }

    /* Nome da primeira partição */
    const char *dev = strrchr(wa->device_path, '/');
    dev = dev ? dev+1 : wa->device_path;
    if (strncmp(dev, "mmcblk", 6) == 0)
        snprintf(dev_part, sizeof(dev_part), "%.240sp1", wa->device_path);
    else
        snprintf(dev_part, sizeof(dev_part), "%.240s1", wa->device_path);

    /* Tabela de partição */
    {
        const char *scheme = (wa->partition_scheme == 1) ? "gpt" : "msdos";
        const char *fstype = (wa->filesystem == 1) ? "ntfs" : "fat32";
        const char *parted[] = {
            "parted","-s", wa->device_path,
            "mklabel", scheme,
            "mkpart","primary", fstype, "1MiB","100%",
            "set","1",
            (wa->partition_scheme==1) ? "esp" : "boot",
            "on", NULL
        };
        if (run_cmd(parted) != 0) {
            /* fdisk fallback */
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                "printf 'o\\nn\\np\\n1\\n\\n\\na\\nw\\n' | "
                "fdisk %.200s >/dev/null 2>&1", wa->device_path);
            if (system(cmd) != 0) { /* continua mesmo assim */ }
        }
        sleep(1);
        const char *pp[] = {"partprobe", wa->device_path, NULL};
        run_cmd(pp);
        sleep(1);
    }

    /* Formata partição */
    set_progress(0.03,0,0,0,0,0,"Formatando partição...");
    if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }

    {
        const char *vol = wa->volume_label[0] ? wa->volume_label : "WINDOWS";
        if (wa->filesystem == 1) {
            const char *mkntfs[] = {"mkfs.ntfs","-Q","-L",vol,dev_part,NULL};
            if (run_cmd(mkntfs) != 0) {
                const char *mkntfs2[] = {"mkntfs","-Q","-L","WINDOWS",dev_part,NULL};
                run_cmd(mkntfs2);
            }
        } else {
            const char *mkfat[] = {"mkfs.vfat","-F","32","-n",vol,dev_part,NULL};
            run_cmd(mkfat);
        }
        sleep(1);
    }

    /* Monta ISO */
    set_progress(0.05,0,0,0,0,0,"Montando imagem ISO...");
    if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }

    {
        const char *mnt[] = {"mount","-o","loop,ro",wa->iso_path,tmp_iso,NULL};
        if (run_cmd(mnt) != 0) {
            set_progress(0,0,0,0,0,1,
                "Erro ao montar ISO.\nExecute com sudo e verifique o arquivo.");
            if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
            goto cleanup;
        }
    }

    /* Monta pendrive */
    {
        const char *mnt[] = {"mount", dev_part, tmp_usb, NULL};
        if (run_cmd(mnt) != 0) {
            set_progress(0,0,0,0,0,1,"Erro ao montar pendrive: %.200s", dev_part);
            if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
            const char *u[] = {"umount", tmp_iso, NULL}; run_cmd(u);
            goto cleanup;
        }
    }

    /* Copia arquivos */
    {
        uint64_t total   = asm_get_file_size(wa->iso_path);
        uint64_t written = 0;
        struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
        int do_split = (wa->filesystem == 0);   /* FAT32 precisa de split */

        set_progress(0.07,0,total,0,0,0,"Copiando arquivos...");
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }

        copy_recursive(tmp_iso, tmp_usb, &written, total, wa, &t0, do_split);
    }

    if (!wa->cancel) {
        set_progress(0.98,0,0,0,0,0,"Sincronizando...");
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
        sync();
    }

    /* Desmonta */
    { const char *u[] = {"umount", tmp_usb, NULL}; run_cmd(u); }
    { const char *u[] = {"umount", tmp_iso, NULL}; run_cmd(u); }

    if (!wa->cancel)
        set_progress(1.0,0,0,0,1,0,
            "✓ Windows ISO gravada com sucesso! Pendrive pronto para boot.");
    else
        set_progress(0,0,0,0,0,1,"Gravação cancelada.");
    if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }

cleanup:
    rmdir(tmp_iso);
    rmdir(tmp_usb);
}

/* =========================================================
 * Verificação de blocos defeituosos (leitura direta)
 * ========================================================= */
static void run_bad_block_check(WriteArgs *wa);  /* forward decl */

/* =========================================================
 * Thread principal
 * ========================================================= */
void *disk_write_thread(void *arg) {
    WriteArgs *wa = (WriteArgs*)arg;

    if (wa->bad_blocks && wa->iso_path[0] == '\0') {
        /* Modo apenas verificação de blocos */
        run_bad_block_check(wa);
    } else {
        /* Grava ISO */
        if (wa->iso_type == ISO_WINDOWS) write_windows_iso(wa);
        else                             write_linux_iso(wa);

        /* Pós-gravação: verifica blocos se solicitado */
        if (wa->bad_blocks && !wa->cancel) {
            set_progress(0, 0, 0, 0, 0, 0,
                "Verificando blocos defeituosos (%d passada(s))...",
                wa->bad_block_passes > 0 ? wa->bad_block_passes : 1);
            if (wa->callback) {
                WriteProgress p = disk_get_progress();
                wa->callback(&p, wa->user_data);
            }
            run_bad_block_check(wa);
        }
    }

    free(wa);
    return NULL;
}

static void run_bad_block_check(WriteArgs *wa) {
    const char   *device   = wa->device_path;
    int           passes   = wa->bad_block_passes > 0 ? wa->bad_block_passes : 1;
    const size_t  BLOCK_SZ = 512;
    const size_t  READ_SZ  = 64 * 1024;

    /* Tamanho total via sysfs */
    const char *devname = strrchr(device, '/');
    devname = devname ? devname+1 : device;
    uint64_t total_bytes = 0;
    {
        char szpath[SYSBUF];
        snprintf(szpath, sizeof(szpath), "/sys/block/%.400s/size", devname);
        FILE *f = fopen(szpath, "r");
        if (f) {
            unsigned long long sec = 0;
            if (fscanf(f, "%llu", &sec) == 1) total_bytes = sec * 512ULL;
            fclose(f);
        }
    }

    /* Abre dispositivo com O_DIRECT (fallback sem) */
    int fd = open(device, O_RDONLY | O_DIRECT);
    if (fd < 0) fd = open(device, O_RDONLY);
    if (fd < 0) {
        set_progress(0, 0, 0, 0, 0, 1,
                     "Erro ao abrir %s: %s", device, strerror(errno));
        if (wa->callback) { WriteProgress p=disk_get_progress(); wa->callback(&p,wa->user_data); }
        return;
    }

    /* Buffer alinhado a 512 bytes para O_DIRECT */
    void *buf = NULL;
    if (posix_memalign(&buf, 512, READ_SZ) != 0) buf = malloc(READ_SZ);
    if (!buf) { close(fd); return; }

    uint64_t bad_count  = 0;
    uint64_t total_read = 0;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int pass = 1; pass <= passes && !wa->cancel; pass++) {
        lseek(fd, 0, SEEK_SET);
        uint64_t pos = 0;

        while (!wa->cancel) {
            ssize_t n = read(fd, buf, READ_SZ);
            if (n == 0) break;

            if (n < 0) {
                /* Bloco com erro: vai setor a setor para contar */
                for (size_t s = 0; s < READ_SZ; s += BLOCK_SZ) {
                    char tmp[512]; /* leitura pequena sem O_DIRECT */
                    lseek(fd, (off_t)(pos + s), SEEK_SET);
                    if (read(fd, tmp, BLOCK_SZ) < 0) bad_count++;
                }
                pos += READ_SZ;
                lseek(fd, (off_t)pos, SEEK_SET);
                continue;
            }

            pos        += (uint64_t)n;
            total_read += (uint64_t)n;

            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            double e  = (now.tv_sec-t0.tv_sec)+(now.tv_nsec-t0.tv_nsec)*1e-9;
            double sp = e > 0 ? total_read/e/(1024.0*1024.0) : 0;
            uint64_t grand_total = (uint64_t)passes * total_bytes;
            uint64_t grand_done  = (uint64_t)(pass-1)*total_bytes + pos;
            double fr = grand_total > 0
                        ? (double)grand_done / (double)grand_total : 0;

            set_progress(fr, total_read, grand_total, sp, 0, 0,
                "Passada %d/%d — %.1f MB/s — %llu bloco(s) ruim(s) (%.0f%%)",
                pass, passes, sp, (unsigned long long)bad_count, fr*100.0);
            if (wa->callback) {
                WriteProgress p = disk_get_progress();
                wa->callback(&p, wa->user_data);
            }
        }
    }

    free(buf); close(fd);

    uint64_t grand_total = (uint64_t)passes * total_bytes;
    if (bad_count == 0)
        set_progress(1.0, total_read, grand_total, 0, 1, 0,
            "✓ Nenhum bloco defeituoso em %d passada(s)!", passes);
    else
        set_progress(1.0, total_read, grand_total, 0, 0, 1,
            "⚠ %llu bloco(s) defeituoso(s) em %d passada(s).",
            (unsigned long long)bad_count, passes);

    if (wa->callback) {
        WriteProgress p = disk_get_progress();
        wa->callback(&p, wa->user_data);
    }
}
