; =====================================================================
; severity.asm  -  NASM x86-64, Microsoft x64 calling convention (Win64)
;
; Build :  nasm -f win64 assembly/severity.asm -o build/obj/severity.obj
; Called:  backend/AsmEngine.cpp  (extern "C" declarations in backend/AsmEngine.h)
;
; Win64 ABI used by every routine in this file
;   arguments  : RCX = 1st, RDX = 2nd, R8 = 3rd, R9 = 4th   (integers / pointers)
;   return     : RAX
;   volatile   : RAX RCX RDX R8 R9 R10 R11  (we only use these -> no saves needed)
;   preserved  : RBX RBP RDI RSI R12-R15    (never touched here)
;   all routines are leaf functions: no calls, so no shadow space / stack frame needed.
;
; [CO-EVIDENCE] COA-CO2: 64-bit numerical computations with registers, ALU
;               instructions (ADD/SHL/SHR/IMUL-free multiply), CMP + conditional branching.
; [CO-EVIDENCE] COA-CO3: multiplication implemented with the add-and-shift algorithm and
;               with successive addition (compare both in tests/test_main.cpp).
; =====================================================================

default rel
bits 64

section .data
    ; per-category base weight (memory/data representation: 8 x 64-bit little-endian words)
    ; index = categories.asm_code in PostgreSQL
    ;              TRAFFIC GARBAGE ELECTRICITY WATER ROADS DRAINAGE  (6) (7=unknown)
    weight_table dq   14,     10,      22,      18,   12,    16,    10,  10

section .text
    global asm_multiply_shift_add
    global asm_multiply_successive
    global calculateSeverityAsm
    global classifySeverityAsm

; ---------------------------------------------------------------------
; uint64 asm_multiply_shift_add(uint64 multiplicand /*RCX*/, uint64 multiplier /*RDX*/)
; Classic binary add-and-shift multiplication (what a simple ALU does in hardware):
;   product = 0
;   while multiplier != 0:
;       if (multiplier & 1) product += multiplicand
;       multiplicand <<= 1 ; multiplier >>= 1
; ---------------------------------------------------------------------
asm_multiply_shift_add:
    xor     rax, rax            ; RAX = product accumulator = 0
    test    rdx, rdx
    jz      .done               ; multiplier == 0 -> product 0
.loop:
    test    rdx, 1              ; LSB of multiplier set?
    jz      .skip_add
    add     rax, rcx            ; product += multiplicand
.skip_add:
    shl     rcx, 1              ; multiplicand *= 2
    shr     rdx, 1              ; multiplier  /= 2  (sets ZF when it reaches 0)
    jnz     .loop
.done:
    ret

; ---------------------------------------------------------------------
; uint64 asm_multiply_successive(uint64 a /*RCX*/, uint64 b /*RDX*/)
; Multiplication by successive addition: add a to the accumulator b times.
; ---------------------------------------------------------------------
asm_multiply_successive:
    xor     rax, rax
.loop:
    test    rdx, rdx
    jz      .done
    add     rax, rcx
    dec     rdx
    jmp     .loop
.done:
    ret

; ---------------------------------------------------------------------
; int64 calculateSeverityAsm(int64 categoryCode /*RCX*/, int64 impact /*RDX*/, int64 urgency /*R8*/)
;
;   impact, urgency are clamped to 1..5 (CMP + JL/JG)
;   base     = weight_table[categoryCode]          (memory operand, scaled index *8)
;   product  = impact * urgency                    (add-and-shift, inline)
;   score    = base + 3*product                    (3p = (p << 1) + p)
;   score    = min(score, 100)
; returns RAX = severity score 0..100
; ---------------------------------------------------------------------
calculateSeverityAsm:
    ; -- category index bounds check: 0..7, otherwise use slot 7 (unknown)
    cmp     rcx, 0
    jl      .bad_cat
    cmp     rcx, 7
    jg      .bad_cat
    jmp     .cat_ok
.bad_cat:
    mov     rcx, 7
.cat_ok:
    lea     r9, [weight_table]
    mov     r10, [r9 + rcx*8]   ; R10 = base weight (64-bit load from data section)

    ; -- clamp impact (RDX) to 1..5
    cmp     rdx, 1
    jge     .imp_lo_ok
    mov     rdx, 1
.imp_lo_ok:
    cmp     rdx, 5
    jle     .imp_hi_ok
    mov     rdx, 5
.imp_hi_ok:
    ; -- clamp urgency (R8) to 1..5
    cmp     r8, 1
    jge     .urg_lo_ok
    mov     r8, 1
.urg_lo_ok:
    cmp     r8, 5
    jle     .urg_hi_ok
    mov     r8, 5
.urg_hi_ok:

    ; -- R11 = impact * urgency using add-and-shift  (RDX multiplicand, R8 multiplier)
    xor     r11, r11
.mul_loop:
    test    r8, 1
    jz      .mul_skip
    add     r11, rdx
.mul_skip:
    shl     rdx, 1
    shr     r8, 1
    jnz     .mul_loop

    ; -- RAX = 3 * product = (product << 1) + product
    mov     rax, r11
    shl     rax, 1
    add     rax, r11
    add     rax, r10            ; + category base weight

    ; -- saturate at 100
    cmp     rax, 100
    jle     .ret
    mov     rax, 100
.ret:
    ret

; ---------------------------------------------------------------------
; int64 classifySeverityAsm(int64 score /*RCX*/)
;   >= 80 -> 4 CRITICAL, >= 55 -> 3 HIGH, >= 30 -> 2 MEDIUM, else 1 LOW
; ---------------------------------------------------------------------
classifySeverityAsm:
    mov     rax, 4
    cmp     rcx, 80
    jge     .out
    mov     rax, 3
    cmp     rcx, 55
    jge     .out
    mov     rax, 2
    cmp     rcx, 30
    jge     .out
    mov     rax, 1
.out:
    ret
