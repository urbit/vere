























































































	.text
	.align	16, 0x90
	.globl	__gmpn_addmul_2
	
	.def	__gmpn_addmul_2
	.scl	2
	.type	32
	.endef
__gmpn_addmul_2:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	%rdx, %r11
	push	%rbx
	push	%rbp

	mov	0(%rcx), %r8
	mov	8(%rcx), %r9

	mov	%edx, %ebx
	mov	(%rsi), %rax
	lea	-8(%rsi,%rdx,8), %rsi
	lea	-8(%rdi,%rdx,8), %rdi
	mul	%r8
	neg	%r11
	and	$3, %ebx
	jz	Lb0
	cmp	$2, %ebx
	jc	Lb1
	jz	Lb2

Lb3:	mov	%rax, %rcx
	mov	%rdx, %rbp
	xor	%r10d, %r10d
	mov	8(%rsi,%r11,8), %rax
	dec	%r11
	jmp	Llo3

Lb2:	mov	%rax, %rbp
	mov	8(%rsi,%r11,8), %rax
	mov	%rdx, %r10
	xor	%ebx, %ebx
	add	$-2, %r11
	jmp	Llo2

Lb1:	mov	%rax, %r10
	mov	8(%rsi,%r11,8), %rax
	mov	%rdx, %rbx
	xor	%ecx, %ecx
	inc	%r11
	jmp	Llo1

Lb0:	mov	$0, %r10d
	mov	%rax, %rbx
	mov	8(%rsi,%r11,8), %rax
	mov	%rdx, %rcx
	xor	%ebp, %ebp
	jmp	Llo0

	.align	32, 0x90
Ltop:	mov	$0, %ecx
	mul	%r8
	add	%rax, %r10
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %rbx
	adc	$0, %ecx
Llo1:	mul	%r9
	add	%r10, (%rdi,%r11,8)
	mov	$0, %r10d
	adc	%rax, %rbx
	mov	$0, %ebp
	mov	8(%rsi,%r11,8), %rax
	adc	%rdx, %rcx
	mul	%r8
	add	%rax, %rbx
	mov	8(%rsi,%r11,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
Llo0:	mul	%r9
	add	%rbx, 8(%rdi,%r11,8)
	adc	%rax, %rcx
	adc	%rdx, %rbp
	mov	16(%rsi,%r11,8), %rax
	mul	%r8
	add	%rax, %rcx
	adc	%rdx, %rbp
	adc	$0, %r10d
	mov	16(%rsi,%r11,8), %rax
Llo3:	mul	%r9
	add	%rcx, 16(%rdi,%r11,8)
	adc	%rax, %rbp
	adc	%rdx, %r10
	xor	%ebx, %ebx
	mov	24(%rsi,%r11,8), %rax
	mul	%r8
	add	%rax, %rbp
	mov	24(%rsi,%r11,8), %rax
	adc	%rdx, %r10
	adc	$0, %ebx
Llo2:	mul	%r9
	add	%rbp, 24(%rdi,%r11,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
	mov	32(%rsi,%r11,8), %rax
	add	$4, %r11
	js	Ltop

Lend:	xor	%ecx, %ecx
	mul	%r8
	add	%rax, %r10
	mov	(%rsi), %rax
	adc	%rdx, %rbx
	adc	%ecx, %ecx
	mul	%r9
	add	%r10, (%rdi)
	adc	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, 8(%rdi)
	mov	%rcx, %rax

	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret
	
