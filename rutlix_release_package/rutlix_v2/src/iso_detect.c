/*
 * IsoBurner - Implementação da detecção de ISO
 * Arquivo: src/iso_detect.c
 */

#include "iso_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

/* =========================================================
 * Estruturas ISO 9660
 * ========================================================= */
#pragma pack(push, 1)

/* Primary Volume Descriptor (setor 16) */
typedef struct {
    uint8_t  type;           /* 1 = PVD */
    char     id[5];          /* "CD001" */
    uint8_t  version;
    uint8_t  unused1;
    char     sys_id[32];
    char     vol_id[32];
    uint8_t  unused2[8];
    uint32_t vol_space_le;
    uint32_t vol_space_be;
    /* ... demais campos omitidos */
} Iso9660PVD;

/* El Torito Boot Record */
typedef struct {
    uint8_t  type;           /* 0 = boot record */
    char     id[5];          /* "CD001" */
    uint8_t  version;
    char     boot_sys_id[32];/* "EL TORITO SPECIFICATION" */
    uint8_t  unused[32];
    uint32_t boot_catalog;
} ElToritoVD;

#pragma pack(pop)

/* =========================================================
 * Utilitários internos
 * ========================================================= */

static void str_trim(char *s, int len) {
    /* Remove espaços do fim */
    for (int i = len - 1; i >= 0; i--) {
        if (s[i] == ' ' || s[i] == '\0') s[i] = '\0';
        else break;
    }
}

/*
 * Lê um setor ISO (2048 bytes) em offset de setor.
 * Retorna 1 em sucesso.
 */
static int read_sector(int fd, uint32_t sector, uint8_t *buf) {
    off_t offset = (off_t)sector * 2048;
    if (lseek(fd, offset, SEEK_SET) < 0) return 0;
    ssize_t n = read(fd, buf, 2048);
    return (n == 2048) ? 1 : 0;
}

/*
 * Converte string ISO para lowercase para busca.
 */
static void str_lower(const char *src, char *dst, size_t sz) {
    size_t i;
    for (i = 0; i < sz - 1 && src[i]; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/*
 * Percorre entradas do diretório raiz da ISO 9660 procurando
 * arquivos/dirs que indicam Windows.
 *
 * Estrutura da entrada de diretório ISO 9660:
 * - byte 0: tamanho da entrada (0 = fim do setor)
 * - byte 1: tamanho dos atributos estendidos
 * - bytes 2-9: localização (LE + BE)
 * - bytes 10-17: tamanho (LE + BE)
 * - byte 32: flags (2 = diretório)
 * - byte 33: file unit size
 * - byte 34: interleave gap
 * - bytes 35-36: volume sequence number
 * - byte 37: tamanho do nome de arquivo
 * - byte 38+: nome do arquivo
 */

/* Indicadores de ISO Windows verificados via scan do diretório raiz */
#define WIN_INDICATOR_MAX 8

static int scan_root_dir_for_windows(int fd, uint32_t root_lba, uint32_t root_size) {
    uint8_t buf[2048];
    int found_sources = 0;
    int found_bootmgr = 0;

    uint32_t sectors = (root_size + 2047) / 2048;
    if (sectors > 64) sectors = 64;  /* limite de segurança */

    for (uint32_t s = 0; s < sectors; s++) {
        if (!read_sector(fd, root_lba + s, buf)) break;

        uint32_t pos = 0;
        while (pos < 2048) {
            uint8_t entry_len = buf[pos];
            if (entry_len == 0) break;   /* fim do bloco */

            if (pos + entry_len > 2048) break;

            uint8_t name_len = buf[pos + 37];
            if (name_len == 0) { pos += entry_len; continue; }

            char name[256] = {0};
            memcpy(name, &buf[pos + 38], name_len < 255 ? name_len : 255);

            /* Remove versão ";1" */
            char *semi = strchr(name, ';');
            if (semi) *semi = '\0';

            char lower[256];
            str_lower(name, lower, sizeof(lower));

            if (strcmp(lower, "sources") == 0) found_sources = 1;
            if (strcmp(lower, "bootmgr") == 0) found_bootmgr = 1;

            pos += entry_len;
        }
    }

    return (found_sources || found_bootmgr) ? 1 : 0;
}

/*
 * Verifica se existe install.wim ou install.esd dentro de /sources/
 * Retorna tamanho do arquivo em bytes (0 se não encontrado).
 */
static uint64_t find_install_wim(int fd, uint32_t root_lba, uint32_t root_size) {
    uint8_t buf[2048];
    uint32_t sources_lba  = 0;
    uint32_t sources_size = 0;

    /* Passo 1: encontra a pasta Sources no diretório raiz */
    uint32_t sectors = (root_size + 2047) / 2048;
    if (sectors > 64) sectors = 64;

    for (uint32_t s = 0; s < sectors && !sources_lba; s++) {
        if (!read_sector(fd, root_lba + s, buf)) break;

        uint32_t pos = 0;
        while (pos < 2048) {
            uint8_t entry_len = buf[pos];
            if (entry_len == 0) break;
            if (pos + entry_len > 2048) break;

            uint8_t flags    = buf[pos + 25];
            uint8_t name_len = buf[pos + 37];
            if (name_len == 0) { pos += entry_len; continue; }

            char name[256] = {0};
            memcpy(name, &buf[pos + 38], name_len < 255 ? name_len : 255);
            char *semi = strchr(name, ';');
            if (semi) *semi = '\0';

            char lower[256];
            str_lower(name, lower, sizeof(lower));

            if ((flags & 0x02) && strcmp(lower, "sources") == 0) {
                /* É diretório sources */
                memcpy(&sources_lba,  &buf[pos + 2], 4);  /* LE */
                memcpy(&sources_size, &buf[pos + 10], 4); /* LE */
            }

            pos += entry_len;
        }
    }

    if (!sources_lba) return 0;

    /* Passo 2: procura install.wim ou install.esd dentro de Sources */
    sectors = (sources_size + 2047) / 2048;
    if (sectors > 256) sectors = 256;

    for (uint32_t s = 0; s < sectors; s++) {
        if (!read_sector(fd, sources_lba + s, buf)) break;

        uint32_t pos = 0;
        while (pos < 2048) {
            uint8_t entry_len = buf[pos];
            if (entry_len == 0) break;
            if (pos + entry_len > 2048) break;

            uint8_t name_len = buf[pos + 37];
            if (name_len == 0) { pos += entry_len; continue; }

            char name[256] = {0};
            memcpy(name, &buf[pos + 38], name_len < 255 ? name_len : 255);
            char *semi = strchr(name, ';');
            if (semi) *semi = '\0';

            char lower[256];
            str_lower(name, lower, sizeof(lower));

            if (strcmp(lower, "install.wim") == 0 ||
                strcmp(lower, "install.esd") == 0) {
                /* Lê tamanho (LE, 4 bytes) */
                uint32_t sz_lo;
                memcpy(&sz_lo, &buf[pos + 10], 4);
                return (uint64_t)sz_lo * 2048ULL;  /* aproximado */
            }

            pos += entry_len;
        }
    }

    return 0;
}

/* =========================================================
 * API pública
 * ========================================================= */

int iso_analyze(const char *path, IsoInfo *info) {
    if (!path || !info) return 0;
    memset(info, 0, sizeof(*info));
    info->type = ISO_UNKNOWN;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    /* Tamanho do arquivo */
    struct stat st;
    if (fstat(fd, &st) == 0) info->total_size = (uint64_t)st.st_size;

    uint8_t buf[2048];

    /* Setor 16 = Primary Volume Descriptor da ISO 9660 */
    if (!read_sector(fd, 16, buf)) {
        close(fd);
        return 0;
    }

    /* Verifica assinatura "CD001" */
    if (memcmp(buf + 1, "CD001", 5) != 0) {
        close(fd);
        return 0;
    }

    /* Lê label do volume */
    memcpy(info->label, buf + 40, 32);
    info->label[32] = '\0';
    str_trim(info->label, 32);

    /* Diretório raiz: offset 156 no PVD */
    uint32_t root_lba, root_size;
    memcpy(&root_lba,  buf + 156 + 2, 4);  /* location LE */
    memcpy(&root_size, buf + 156 + 10, 4); /* size LE */

    /* Detecta Windows */
    int is_windows = scan_root_dir_for_windows(fd, root_lba, root_size);

    if (is_windows) {
        info->type = ISO_WINDOWS;

        /* Verifica install.wim */
        uint64_t wim_size = find_install_wim(fd, root_lba, root_size);
        if (wim_size > 0) {
            info->has_install_wim = 1;
            info->wim_needs_split = (wim_size > 4ULL * 1024 * 1024 * 1024) ? 1 : 0;
        }

        /* Verifica /efi/boot/bootx64.efi nos setores iniciais */
        for (uint32_t s = 17; s < 32; s++) {
            if (read_sector(fd, s, buf)) {
                if (buf[0] == 0 && buf[1] == 0xEF) {
                    info->has_efi_image = 1;
                    info->is_hybrid = 1;
                    break;
                }
            }
        }

        snprintf(info->description, sizeof(info->description),
                 "Windows Installation Media — %s", info->label);
    } else {
        info->type = ISO_LINUX;

        /* Verifica se é híbrido (El Torito + EFI) verificando setor 17 */
        if (read_sector(fd, 17, buf)) {
            if (buf[0] == 0 && memcmp(buf + 1, "CD001", 5) == 0) {
                ElToritoVD *et = (ElToritoVD *)buf;
                if (memcmp(et->boot_sys_id, "EL TORITO SPECIFICATION", 23) == 0) {
                    info->is_hybrid = 1;
                }
            }
        }

        snprintf(info->description, sizeof(info->description),
                 "Linux ISO — %s", info->label);
    }

    close(fd);
    return 1;
}

IsoType iso_detect_type(const char *path) {
    IsoInfo info;
    if (!iso_analyze(path, &info)) return ISO_UNKNOWN;
    return info.type;
}

int disk_is_valid_iso(const char *path, char *err_msg, size_t err_sz) {
    if (!path || !*path) {
        if (err_msg) snprintf(err_msg, err_sz, "Caminho vazio / Empty path");
        return 0;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        if (err_msg) snprintf(err_msg, err_sz,
                              "Arquivo não encontrado / File not found: %s", path);
        return 0;
    }

    if (st.st_size < 32768) {
        if (err_msg) snprintf(err_msg, err_sz,
                              "Arquivo muito pequeno para ser uma ISO válida / "
                              "File too small to be a valid ISO");
        return 0;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (err_msg) snprintf(err_msg, err_sz,
                              "Sem permissão para ler / Cannot read: %s", path);
        return 0;
    }

    uint8_t buf[2048];
    if (lseek(fd, 16 * 2048, SEEK_SET) < 0 || read(fd, buf, 2048) != 2048) {
        close(fd);
        if (err_msg) snprintf(err_msg, err_sz,
                              "Não foi possível ler o volume descriptor / "
                              "Cannot read volume descriptor");
        return 0;
    }
    close(fd);

    if (memcmp(buf + 1, "CD001", 5) != 0) {
        if (err_msg) snprintf(err_msg, err_sz,
                              "Assinatura ISO 9660 não encontrada. "
                              "Pode ser UDF ou formato diferente. / "
                              "ISO 9660 signature not found. May be UDF or different format.");
        return 1;  /* permite continuar */
    }

    return 1;
}
