; =====================================================================
; strops.asm  -  NASM x86-64 string / data processing routines (Win64 ABI)
;
; Build :  nasm -f win64 assembly/strops.asm -o build/obj/strops.obj
; Used by:
;   asm_contains_ci -> backend/ComplaintService.cpp  (case-insensitive complaint search)
;   asm_hash_djb2   -> dsa/HashTable.cpp             (bucket index for the complaint hash table)
;   asm_strlen / asm_to_lower -> backend/AsmEngine.cpp, tests
;
; Registers: RCX/RDX = arguments, RAX = return, R8-R11 = scratch (volatile).
; No callee-saved register is modified, so no PUSH/POP is required.
;
; [CO-EVIDENCE] COA-CO3: string-processing requirement implemented in 64-bit Assembly
;               (byte loads, ASCII range comparisons, case folding, nested scan loops).
; =====================================================================

default rel
bits 64

section .text
    global asm_strlen
    global asm_to_lower
    global asm_contains_ci
    global asm_hash_djb2

; ---------------------------------------------------------------------
; uint64 asm_strlen(const char* s /*RCX*/)
; ---------------------------------------------------------------------
asm_strlen:
    xor     rax, rax
.loop:
    cmp     byte [rcx + rax], 0
    je      .done
    inc     rax
    jmp     .loop
.done:
    ret

; ---------------------------------------------------------------------
; uint64 asm_to_lower(char* s /*RCX*/)   in-place ASCII lower-case, returns length
; ---------------------------------------------------------------------
asm_to_lower:
    xor     rax, rax
.loop:
    movzx   edx, byte [rcx + rax]
    test    dl, dl
    jz      .done
    cmp     dl, 'A'
    jb      .next
    cmp     dl, 'Z'
    ja      .next
    or      dl, 0x20            ; 'A'(0x41) | 0x20 = 'a'(0x61)
    mov     [rcx + rax], dl
.next:
    inc     rax
    jmp     .loop
.done:
    ret

; ---------------------------------------------------------------------
; int64 asm_contains_ci(const char* haystack /*RCX*/, const char* needle /*RDX*/)
; returns 1 if needle occurs in haystack ignoring ASCII case, else 0.
; Empty needle matches. Naive O(n*m) scan:
;   for each start position R8 in haystack:
;       R9 = R8, R10 = needle; compare folded bytes until needle ends (match) or mismatch
; ---------------------------------------------------------------------
asm_contains_ci:
    cmp     byte [rdx], 0
    je      .found              ; empty needle
    mov     r8, rcx             ; R8 = current start in haystack
.outer:
    cmp     byte [r8], 0
    je      .not_found          ; ran out of haystack
    mov     r9, r8              ; R9  = haystack cursor
    mov     r10, rdx            ; R10 = needle cursor
.inner:
    movzx   eax, byte [r10]
    test    al, al
    jz      .found              ; reached end of needle -> match
    movzx   r11d, byte [r9]
    test    r11b, r11b
    jz      .not_found          ; haystack ended before needle
    ; fold AL to lower case
    cmp     al, 'A'
    jb      .a_ok
    cmp     al, 'Z'
    ja      .a_ok
    or      al, 0x20
.a_ok:
    ; fold R11B to lower case
    cmp     r11b, 'A'
    jb      .b_ok
    cmp     r11b, 'Z'
    ja      .b_ok
    or      r11b, 0x20
.b_ok:
    cmp     al, r11b
    jne     .advance
    inc     r9
    inc     r10
    jmp     .inner
.advance:
    inc     r8
    jmp     .outer
.found:
    mov     rax, 1
    ret
.not_found:
    xor     rax, rax
    ret

; ---------------------------------------------------------------------
; uint64 asm_hash_djb2(const char* key /*RCX*/, uint64 buckets /*RDX*/)
; Case-insensitive djb2:  h = 5381; for c in key: h = h*33 + lower(c)
; h*33 computed as (h << 5) + h ; result = h mod buckets (DIV leaves remainder in RDX)
; ---------------------------------------------------------------------
asm_hash_djb2:
    mov     r8, rdx             ; R8 = bucket count (DIV needs RDX:RAX free)
    mov     rax, 5381
.loop:
    movzx   r9d, byte [rcx]
    test    r9b, r9b
    jz      .reduce
    cmp     r9b, 'A'
    jb      .fold_ok
    cmp     r9b, 'Z'
    ja      .fold_ok
    or      r9b, 0x20
.fold_ok:
    mov     r10, rax
    shl     rax, 5              ; h * 32
    add     rax, r10            ; + h  -> h * 33
    add     rax, r9             ; + c
    inc     rcx
    jmp     .loop
.reduce:
    test    r8, r8
    jz      .no_mod
    xor     edx, edx            ; RDX:RAX / R8
    div     r8
    mov     rax, rdx            ; remainder = bucket index
.no_mod:
    ret
