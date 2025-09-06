



































































  
  
  








	.text
	.align	16, 0x90
	.globl	__gmpn_rsblsh_n
	
	.def	__gmpn_rsblsh_n
	.scl	2
	.type	32
	.endef
__gmpn_rsblsh_n:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	56(%rsp), %r8d	
	push	%r12
	push	%rbp
	push	%rbx

	mov	(%rdx), %rax	

	mov	$0, %ebp
	sub	%rcx, %rbp

	lea	-16(%rsi,%rcx,8), %rsi
	lea	-16(%rdi,%rcx,8), %rdi
	lea	16(%rdx,%rcx,8), %r12

	mov	%rcx, %r9

	mov	%r8, %rcx
	mov	$1, %r8d
	shl	%cl, %r8

	mul	%r8			

	and	$3, %r9d
	jz	Lb0
	cmp	$2, %r9d
	jc	Lb1
	jz	Lb2

Lb3:	mov	%rax, %r11
	sub	16(%rsi,%rbp,8), %r11
	mov	-8(%r12,%rbp,8), %rax
	sbb	%ecx, %ecx
	mov	%rdx, %rbx
	mul	%r8
	or	%rax, %rbx
	mov	(%r12,%rbp,8), %rax
	mov	%rdx, %r9
	mul	%r8
	or	%rax, %r9
	add	$3, %rbp
	jnz	Llo3
	jmp	Lcj3

Lb2:	mov	%rax, %rbx
	mov	-8(%r12,%rbp,8), %rax
	mov	%rdx, %r9
	mul	%r8
	or	%rax, %r9
	add	$2, %rbp
	jz	Lcj2
	mov	%rdx, %r10
	mov	-16(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r10
	xor	%ecx, %ecx	
	jmp	Llo2

Lb1:	mov	%rax, %r9
	mov	%rdx, %r10
	add	$1, %rbp
	jnz	Lgt1
	sub	8(%rsi,%rbp,8), %r9
	jmp	Lcj1
Lgt1:	mov	-16(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r10
	mov	%rdx, %r11
	mov	-8(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r11
	sub	8(%rsi,%rbp,8), %r9
	sbb	16(%rsi,%rbp,8), %r10
	sbb	24(%rsi,%rbp,8), %r11
	mov	(%r12,%rbp,8), %rax
	sbb	%ecx, %ecx
	jmp	Llo1

Lb0:	mov	%rax, %r10
	mov	%rdx, %r11
	mov	-8(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r11
	sub	16(%rsi,%rbp,8), %r10
	sbb	24(%rsi,%rbp,8), %r11
	mov	(%r12,%rbp,8), %rax
	sbb	%ecx, %ecx
	mov	%rdx, %rbx
	mul	%r8
	or	%rax, %rbx
	mov	8(%r12,%rbp,8), %rax
	add	$4, %rbp
	jz	Lend

	.align	8, 0x90
Ltop:	mov	%rdx, %r9
	mul	%r8
	or	%rax, %r9
	mov	%r10, -16(%rdi,%rbp,8)
Llo3:	mov	%rdx, %r10
	mov	-16(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r10
	mov	%r11, -8(%rdi,%rbp,8)
Llo2:	mov	%rdx, %r11
	mov	-8(%r12,%rbp,8), %rax
	mul	%r8
	or	%rax, %r11
	add	%ecx, %ecx
	sbb	(%rsi,%rbp,8), %rbx
	sbb	8(%rsi,%rbp,8), %r9
	sbb	16(%rsi,%rbp,8), %r10
	sbb	24(%rsi,%rbp,8), %r11
	mov	(%r12,%rbp,8), %rax
	sbb	%ecx, %ecx
	mov	%rbx, (%rdi,%rbp,8)
Llo1:	mov	%rdx, %rbx
	mul	%r8
	or	%rax, %rbx
	mov	%r9, 8(%rdi,%rbp,8)
Llo0:	mov	8(%r12,%rbp,8), %rax
	add	$4, %rbp
	jnz	Ltop

Lend:	mov	%rdx, %r9
	mul	%r8
	or	%rax, %r9
	mov	%r10, -16(%rdi,%rbp,8)
Lcj3:	mov	%r11, -8(%rdi,%rbp,8)
Lcj2:	add	%ecx, %ecx
	sbb	(%rsi,%rbp,8), %rbx
	sbb	8(%rsi,%rbp,8), %r9
	mov	%rbx, (%rdi,%rbp,8)
Lcj1:	mov	%r9, 8(%rdi,%rbp,8)
	mov	%rdx, %rax
	sbb	$0, %rax
	pop	%rbx
	pop	%rbp
	pop	%r12
	pop	%rsi
	pop	%rdi
	ret
	
