















































































	.text
	.align	16, 0x90
	.globl	__gmpn_sec_tabselect
	
	.def	__gmpn_sec_tabselect
	.scl	2
	.type	32
	.endef
__gmpn_sec_tabselect:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	56(%rsp), %r8d	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	mov	%rdx, %r9
	add	$-4, %r9
	js	Louter_end

Louter_top:
	mov	%rcx, %rbp
	push	%rsi
	xor	%r12d, %r12d
	xor	%r13d, %r13d
	xor	%r14d, %r14d
	xor	%r15d, %r15d
	mov	%r8, %rbx

	.align	16, 0x90
Ltop:	sub	$1, %rbx
	sbb	%rax, %rax
	mov	0(%rsi), %r10
	mov	8(%rsi), %r11
	and	%rax, %r10
	and	%rax, %r11
	or	%r10, %r12
	or	%r11, %r13
	mov	16(%rsi), %r10
	mov	24(%rsi), %r11
	and	%rax, %r10
	and	%rax, %r11
	or	%r10, %r14
	or	%r11, %r15
	lea	(%rsi,%rdx,8), %rsi
	add	$-1, %rbp
	jne	Ltop

	mov	%r12, 0(%rdi)
	mov	%r13, 8(%rdi)
	mov	%r14, 16(%rdi)
	mov	%r15, 24(%rdi)
	pop	%rsi
	lea	32(%rsi), %rsi
	lea	32(%rdi), %rdi
	add	$-4, %r9
	jns	Louter_top
Louter_end:

	test	$2, %dl
	jz	Lb0x
Lb1x:	mov	%rcx, %rbp
	push	%rsi
	xor	%r12d, %r12d
	xor	%r13d, %r13d
	mov	%r8, %rbx
	.align	16, 0x90
Ltp2:	sub	$1, %rbx
	sbb	%rax, %rax
	mov	0(%rsi), %r10
	mov	8(%rsi), %r11
	and	%rax, %r10
	and	%rax, %r11
	or	%r10, %r12
	or	%r11, %r13
	lea	(%rsi,%rdx,8), %rsi
	add	$-1, %rbp
	jne	Ltp2
	mov	%r12, 0(%rdi)
	mov	%r13, 8(%rdi)
	pop	%rsi
	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi

Lb0x:	test	$1, %dl
	jz	Lb00
Lb01:	mov	%rcx, %rbp
	xor	%r12d, %r12d
	mov	%r8, %rbx
	.align	16, 0x90
Ltp1:	sub	$1, %rbx
	sbb	%rax, %rax
	mov	0(%rsi), %r10
	and	%rax, %r10
	or	%r10, %r12
	lea	(%rsi,%rdx,8), %rsi
	add	$-1, %rbp
	jne	Ltp1
	mov	%r12, 0(%rdi)

Lb00:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret
	
