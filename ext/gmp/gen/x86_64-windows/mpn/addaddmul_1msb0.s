





































































	.text
	.align	16, 0x90
	.globl	__gmpn_addaddmul_1msb0
	
	.def	__gmpn_addaddmul_1msb0
	.scl	2
	.type	32
	.endef
__gmpn_addaddmul_1msb0:

        push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	56(%rsp), %r8	
	mov	64(%rsp), %r9	
	push	%rbp

	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdx,%rcx,8), %rbp
	lea	(%rdi,%rcx,8), %rdi
	neg	%rcx

	mov	(%rsi,%rcx,8), %rax
	mul	%r8
	mov	%rax, %r11
	mov	(%rbp,%rcx,8), %rax
	mov	%rdx, %r10
	add	$3, %rcx
	jns	Lend

	push	%r13

	.align	16, 0x90
Ltop:	mul	%r9
	add	%rax, %r11
	mov	-16(%rsi,%rcx,8), %rax
	adc	%rdx, %r10
	mov	%r11, -24(%rdi,%rcx,8)
	mul	%r8
	add	%rax, %r10
	mov	-16(%rbp,%rcx,8), %rax
	mov	$0, %r13d
	adc	%rdx, %r13
	mul	%r9
	add	%rax, %r10
	mov	-8(%rsi,%rcx,8), %rax
	adc	%rdx, %r13
	mov	%r10, -16(%rdi,%rcx,8)
	mul	%r8
	add	%rax, %r13
	mov	-8(%rbp,%rcx,8), %rax
	mov	$0, %r11d
	adc	%rdx, %r11
	mul	%r9
	add	%rax, %r13
	adc	%rdx, %r11
	mov	(%rsi,%rcx,8), %rax
	mul	%r8
	add	%rax, %r11
	mov	%r13, -8(%rdi,%rcx,8)
	mov	(%rbp,%rcx,8), %rax
	mov	$0, %r10d
	adc	%rdx, %r10
	add	$3, %rcx
	js	Ltop

	pop	%r13

Lend:	mul	%r9
	add	%rax, %r11
	adc	%rdx, %r10
	cmp	$1, %ecx
	ja	Ltwo
	mov	-16(%rsi,%rcx,8), %rax
	mov	%r11, -24(%rdi,%rcx,8)
	mov	%r10, %r11
	jz	Lone

Lnul:	mul	%r8
	add	%rax, %r10
	mov	-16(%rbp), %rax
	mov	$0, %r11d
	adc	%rdx, %r11
	mul	%r9
	add	%rax, %r10
	mov	-8(%rsi), %rax
	adc	%rdx, %r11
	mov	%r10, -16(%rdi)
Lone:	mul	%r8
	add	%rax, %r11
	mov	-8(%rbp), %rax
	mov	$0, %r10d
	adc	%rdx, %r10
	mul	%r9
	add	%rax, %r11
	adc	%rdx, %r10

Ltwo:	mov	%r11, -8(%rdi)
	mov	%r10, %rax
Lret:	pop	%rbp
	pop	%rsi
	pop	%rdi
	ret
	
