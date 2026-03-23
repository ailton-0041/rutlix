/*
 * IsoBurner - Funções Assembly / fallback C
 * src/asm_utils.h
 */
#ifndef ASM_UTILS_H
#define ASM_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NO_ASM
/* ── Implementações C puras (fallback quando sem NASM) ── */

static inline uint64_t asm_get_file_size(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? (uint64_t)st.st_size : 0ULL;
}
static inline int asm_str_ends_with_iso(const char *s) {
    if (!s) return 0;
    size_t l = strlen(s); if (l < 4) return 0;
    const char *e = s + l - 4;
    return (e[0]=='.' &&
            (e[1]=='i'||e[1]=='I') &&
            (e[2]=='s'||e[2]=='S') &&
            (e[3]=='o'||e[3]=='O')) ? 1 : 0;
}
static inline uint32_t asm_compute_checksum(const uint8_t *d, size_t n) {
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++) s += d[i];
    return s;
}
static inline void asm_memcpy_fast(void *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
}
static inline void asm_zero_memory(void *p, size_t n) {
    memset(p, 0, n);
}
static inline uint64_t asm_align_up(uint64_t v, uint64_t a) {
    return (v + a - 1) & ~(a - 1);
}

#else
/* ── Declarações externas — implementadas em asm/utils.asm ── */

uint64_t asm_get_file_size(const char *path);
int      asm_str_ends_with_iso(const char *str);
uint32_t asm_compute_checksum(const uint8_t *data, size_t len);
void     asm_memcpy_fast(void *dst, const void *src, size_t n);
void     asm_zero_memory(void *ptr, size_t n);
uint64_t asm_align_up(uint64_t value, uint64_t align);

#endif /* NO_ASM */

#ifdef __cplusplus
}
#endif

#endif /* ASM_UTILS_H */
