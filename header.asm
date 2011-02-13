;;; chainlance asm source  -*- mode: nasm -*-

	BITS 64
	SECTION .text

;;; Symbolic names for fixed registers

%define rTapeSize  rdx
%define rTapeSized edx
%define rTapeSizeb dl
%define rTapeA     rbx
%define rTapeB     rcx
%define rTapeAb    bl
%define rTapeBb    cl
%define rRepAd     r8d
%define rRepBd     r9d
%define rRepSA     r10
%define rRepSB     r11
%define rSaveIP    r12
%define rCell2     r13b
%define rTapeBase  r14
%define rCycles    r15
%define rCyclesd   r15d

;;; Top-level logic for BF Joust matches

%define MAXCYCLES 100000

%define MINTAPE 10
%define MAXTAPE 30

global main
main:

	;; miscellaneous initialization

	mov rbp, rsp

	cld					; make stosb and such always advance
	xor esi, esi				; sum of wins for progA

	;; run through all tape sizes with progA vs progB

	xor rTapeSized, rTapeSized
	mov rTapeSizeb, MINTAPE
loop1:	lea rSaveIP, [rel progB]
	call run
	inc rTapeSizeb
	cmp rTapeSizeb, MAXTAPE
	jbe loop1

	;; run through all tape sizes with progA vs progB2 (reverse polarity)

	xor rTapeSized, rTapeSized
	mov rTapeSizeb, MINTAPE
loop2:	lea rSaveIP, [rel progB2]
	call run
	inc rTapeSizeb
	cmp rTapeSizeb, MAXTAPE
	jbe loop2

	;; all done, return the result (-42 .. 42 for tapes 10 .. 30)

	mov eax, esi
	ret

;;; Single-match execution and initialization

run:	;; runs a single match
	;; rSaveIP should be set to either progB or progB2
	;; rTapeSize should be set to the proper tape size

	;; initialize tape for tape of rTapeSize bytes
	;; also sets rTapeBase, rTapeA, rTapeB, rCell2

	xor eax, eax
	lea rdi, [rel tape]
	mov rTapeBase, rdi			; save start of tape
	mov ecx, rTapeSized
	rep stosb				; tape[0 .. size-1] = 0

	xor rTapeA, rTapeA			; tape pointer for A = 0 = left end
	lea rTapeB, [rTapeA+rTapeSize-1]	; tape pointer for B = size-1 = right end

	mov al, 128
	mov [rTapeBase], al			; tape[0] = flagA = 128
	mov [rTapeBase+rTapeB], al		; tape[size-1] = flagB = 128
	mov rCell2, al				; copy of tape[B] for the first cycle

	;; initialize rep counter stack

%if MAXREPS > 1
	lea rRepSA, [rel repStackA]
	lea rRepSB, [rel repStackB]
%endif

	;; start executing this round

	mov rCyclesd, MAXCYCLES			; initialize cycle counter

	xor edi, edi				; used for "death flags"
runit:	jmp progA

;;; Helper macros and snippets for the compiled code

;; context switch ProgA -> ProgB
%macro	CTX_ProgA 0

	lea rax, [rel %%next]
	xchg rax, rSaveIP
	jmp (rax)
  %%next:

%endmacro

;; context switch ProgB -> ProgA
%macro	CTX_ProgB 0
	lea rax, [rel %%next]
	jmp nextcycle
  %%next:
%endmacro

;; end-of-cycle code
nextcycle:
	cmp byte [rTapeBase], 0
	jz .flagA				; A's flag is zero
	btr edi, 1				; A's flag is nonzero, clear flag
.testB:	cmp byte [rTapeBase + rTapeSize - 1], 0
	jz .flagB				; B's flag is zero
	btr edi, 2				; B's flag is nonzero, clear flag
.cont:	bt edi, 0
	jc .winB				; A totally dead, B wins
	dec rCycles
	jz tie					; tie if out of cycles
	mov rCell2, [rTapeBase + rTapeB]	; save a copy for ProgB [ check
	xchg rax, rSaveIP
	jmp (rax)
.flagA:
	bts edi, 1				; ensure the zero gets flagged
	jnc .testB				; first offense, let live and test B
	bts edi, 0				; second offense, flag totally dead
	jmp .testB				; test B in case of ties
.flagB:
	bts edi, 2				; make sure it gets flagged
	jnc .cont				; first offense, let live
	bt edi, 0
	jc tie					; A also dead. so tie
	inc esi
	ret					; A wins (esi +1)
.winB:
	dec esi
	ret					; B wins (esi -1)

;; A fell of the tape (middle of cycle, must wait for tie)
fallA:
	bts edi, 0				; flag A as totally dead
	CTX_ProgA				; let B run to check for tie

;; B fell of the tape (end of cycle, can decide)
fallB:
	bt edi, 0
	jc tie					; A also totally dead, tie
	cmp byte [rTapeBase], 0
	jnz .notie				; A known to be alive, A wins
	bt edi, 1
	jc tie					; A dead by flag now, tie
.notie:	inc esi
	ret					; A wind (esi +1)

;; in case of a tie
tie:	ret					; tie (no change in esi)

;;; Statically allocated memory for tape and such

	SECTION .data

tape:	times 30 db 0

%if MAXREPS > 1
repStackA:
	times MAXREPS-1 dd 0
repStackB:
	times MAXREPS-1 dd 0
%endif

	SECTION .text

;;; Program code below
