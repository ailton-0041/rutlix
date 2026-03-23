; ============================================================
; IsoBurner v2.0 - Utilitários Assembly x86-64 (NASM)
; Arquivo: asm/utils.asm
;
; Compilar:
;   nasm -f elf64 -o build/utils.o asm/utils.asm
;
; ABI: System V AMD64
;   Argumentos: rdi, rsi, rdx, rcx, r8, r9
;   Retorno:    rax
;   Preservar:  rbx, rbp, r12-r15
; ============================================================

section .data
    align 16
    zero_xmm   dq 0, 0   ; constante zero para XMM

section .text

; ============================================================
; uint64_t asm_get_file_size(const char *path)
;
;   Usa syscall sys_stat (número 4 no Linux x86-64)
;   para obter st_size sem passar pelo libc.
;
;   struct stat layout (Linux x86-64):
;     offset  0: st_dev     (8 bytes)
;     offset  8: st_ino     (8 bytes)
;     offset 16: st_nlink   (8 bytes)
;     offset 24: st_mode    (4 bytes)
;     offset 28: st_uid     (4 bytes)
;     offset 32: st_gid     (4 bytes)
;     offset 36: __pad0     (4 bytes)
;     offset 40: st_rdev    (8 bytes)
;     offset 48: st_size    (8 bytes)  ← queremos isso
;     ...total: 144 bytes
; ============================================================
global asm_get_file_size
asm_get_file_size:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 144            ; aloca struct stat na stack

    ; rdi = path (já está correto como 1º argumento)
    lea     rsi, [rsp]          ; rsi = &stat_buf
    mov     eax, 4              ; sys_stat
    syscall

    test    rax, rax
    js      .err                ; rax < 0 → erro

    mov     rax, [rsp + 48]     ; st_size
    jmp     .done

.err:
    xor     eax, eax

.done:
    leave
    ret


; ============================================================
; int asm_str_ends_with_iso(const char *str)
;
;   Verifica se a string termina com ".iso" ou ".ISO"
;   de forma case-insensitive usando OR 0x20 (lowercase trick).
;   Retorna 1 se sim, 0 se não.
; ============================================================
global asm_str_ends_with_iso
asm_str_ends_with_iso:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12

    test    rdi, rdi
    jz      .no

    mov     r12, rdi            ; salva ponteiro original

    ; strlen em Assembly (loop simples)
    xor     ecx, ecx
    mov     rbx, rdi
.len_loop:
    cmp     byte [rbx], 0
    je      .len_done
    inc     ecx
    inc     rbx
    jmp     .len_loop
.len_done:
    ; ecx = comprimento

    cmp     ecx, 4
    jl      .no

    ; ponteiro para os últimos 4 caracteres
    lea     rbx, [r12 + rcx - 4]

    ; byte 0: deve ser '.'  (ASCII 46 = 0x2E)
    cmp     byte [rbx], '.'
    jne     .no

    ; byte 1: 'i' ou 'I' → OR 0x20 → 'i'
    mov     al, byte [rbx + 1]
    or      al, 0x20
    cmp     al, 'i'
    jne     .no

    ; byte 2: 's' ou 'S' → OR 0x20 → 's'
    mov     al, byte [rbx + 2]
    or      al, 0x20
    cmp     al, 's'
    jne     .no

    ; byte 3: 'o' ou 'O' → OR 0x20 → 'o'
    mov     al, byte [rbx + 3]
    or      al, 0x20
    cmp     al, 'o'
    jne     .no

    mov     eax, 1
    jmp     .done_iso
.no:
    xor     eax, eax
.done_iso:
    pop     r12
    pop     rbx
    leave
    ret


; ============================================================
; uint32_t asm_compute_checksum(const uint8_t *data, size_t len)
;
;   Soma acumulada de bytes.
;   Processa 8 bytes por iteração com unrolling parcial.
; ============================================================
global asm_compute_checksum
asm_compute_checksum:
    push    rbp
    mov     rbp, rsp

    xor     eax, eax            ; acumulador
    test    rsi, rsi
    jz      .cs_done

    mov     rcx, rsi
    mov     rbx, rdi

    ; Loop principal: 8 bytes por vez (unrolled)
    cmp     rcx, 8
    jl      .cs_byte

.cs_8loop:
    movzx   edx, byte [rbx]
    add     eax, edx
    movzx   edx, byte [rbx + 1]
    add     eax, edx
    movzx   edx, byte [rbx + 2]
    add     eax, edx
    movzx   edx, byte [rbx + 3]
    add     eax, edx
    movzx   edx, byte [rbx + 4]
    add     eax, edx
    movzx   edx, byte [rbx + 5]
    add     eax, edx
    movzx   edx, byte [rbx + 6]
    add     eax, edx
    movzx   edx, byte [rbx + 7]
    add     eax, edx
    add     rbx, 8
    sub     rcx, 8
    cmp     rcx, 8
    jge     .cs_8loop

.cs_byte:
    test    rcx, rcx
    jz      .cs_done
    movzx   edx, byte [rbx]
    add     eax, edx
    inc     rbx
    dec     rcx
    jnz     .cs_byte

.cs_done:
    leave
    ret


; ============================================================
; void asm_memcpy_fast(void *dst, const void *src, size_t n)
;
;   Cópia otimizada usando REP MOVSB.
;   Em CPUs modernas (Intel Haswell+, AMD Zen+), o microcode
;   expande REP MOVSB em operações SIMD de 16-32+ bytes/ciclo.
; ============================================================
global asm_memcpy_fast
asm_memcpy_fast:
    push    rbp
    mov     rbp, rsp

    mov     rcx, rdx            ; contagem de bytes
    cld                         ; direção: crescente
    rep     movsb               ; copia [rsi] → [rdi], RCX vezes

    leave
    ret


; ============================================================
; void asm_zero_memory(void *ptr, size_t n)
;
;   Zera memória usando REP STOSB.
; ============================================================
global asm_zero_memory
asm_zero_memory:
    push    rbp
    mov     rbp, rsp

    mov     rcx, rsi
    xor     eax, eax
    cld
    rep     stosb

    leave
    ret


; ============================================================
; uint64_t asm_align_up(uint64_t value, uint64_t align)
;
;   Alinha 'value' para cima até o próximo múltiplo de 'align'.
;   'align' deve ser potência de 2.
;
;   Fórmula: (value + align - 1) & ~(align - 1)
; ============================================================
global asm_align_up
asm_align_up:
    push    rbp
    mov     rbp, rsp

    mov     rax, rdi            ; rax = value
    lea     rdx, [rsi - 1]      ; rdx = align - 1
    add     rax, rdx            ; rax = value + align - 1
    not     rdx                 ; rdx = ~(align - 1)
    and     rax, rdx            ; rax = alinhado

    leave
    ret
