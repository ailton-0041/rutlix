/*
 * IsoBurner - Detecção e validação de imagens ISO
 * Arquivo: src/iso_detect.h + iso_detect.c
 *
 * Suporte completo a:
 *   - ISO 9660 (Linux padrão)
 *   - El Torito (boot legacy)
 *   - UDF (DVDs, ISOs maiores)
 *   - Windows ISO (detecta presence de sources/install.wim / install.esd)
 *   - ISO híbrido (BIOS + UEFI)
 */

#ifndef ISO_DETECT_H
#define ISO_DETECT_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    ISO_UNKNOWN = 0,
    ISO_LINUX,
    ISO_WINDOWS
} IsoType;

typedef struct {
    IsoType  type;
    int      is_hybrid;          /* tem boot BIOS + UEFI */
    int      has_efi_image;      /* /EFI/BOOT/BOOTx64.EFI presente */
    int      has_install_wim;    /* Windows: install.wim ou install.esd */
    int      wim_needs_split;    /* install.wim > 4 GB */
    uint64_t total_size;
    char     label[33];          /* label da ISO 9660 */
    char     description[128];   /* descrição amigável */
} IsoInfo;

/*
 * Detecta o tipo da ISO (Windows / Linux / Desconhecido).
 * Leitura de até ~4 MB no início do arquivo para análise.
 */
IsoType iso_detect_type(const char *path);

/*
 * Análise completa da ISO — preenche IsoInfo.
 * Retorna 1 em sucesso, 0 em falha.
 */
int iso_analyze(const char *path, IsoInfo *info);

/*
 * Valida se o arquivo é uma imagem ISO utilizável.
 * Retorna 1 se válida, 0 se inválida (preenche err_msg).
 */
int disk_is_valid_iso(const char *path, char *err_msg, size_t err_sz);

#endif /* ISO_DETECT_H */
