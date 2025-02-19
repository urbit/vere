



































































	.text
	.align	64, 0x90
	.byte	0,0,0,0,0,0
	.globl	__gmpn_copyi
	.type	__gmpn_copyi,@function
	
__gmpn_copyi:

	lea	-8(%rdi), %rdi
	sub	$4, %rdx
	jc	.Lend

.Ltop:	mov	(%rsi), %rax
	mov	8(%rsi), %r9
	lea	32(%rdi), %rdi
	mov	16(%rsi), %r10
	mov	24(%rsi), %r11
	lea	32(%rsi), %rsi
	mov	%rax, -24(%rdi)
	mov	%r9, -16(%rdi)
	sub	$4, %rdx
	mov	%r10, -8(%rdi)
	mov	%r11, (%rdi)
	jnc	.Ltop

.Lend:	shr	%edx
	jnc	1f
	mov	(%rsi), %rax
	mov	%rax, 8(%rdi)
	lea	8(%rdi), %rdi
	lea	8(%rsi), %rsi
1:	shr	%edx
	jnc	1f
	mov	(%rsi), %rax
	mov	8(%rsi), %r9
	mov	%rax, 8(%rdi)
	mov	%r9, 16(%rdi)
1:	ret
	.size	__gmpn_copyi,.-__gmpn_copyi
