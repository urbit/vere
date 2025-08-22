



































































	.text
	.align	64, 0x90
	.globl	__gmpn_copyd
	
	.def	__gmpn_copyd
	.scl	2
	.type	32
	.endef
__gmpn_copyd:

	lea	-8(%rdx,%r8,8), %rdx
	lea	(%rcx,%r8,8), %rcx
	sub	$4, %r8
	jc	Lend
	nop

Ltop:	mov	(%rdx), %rax
	mov	-8(%rdx), %r9
	lea	-32(%rcx), %rcx
	mov	-16(%rdx), %r10
	mov	-24(%rdx), %r11
	lea	-32(%rdx), %rdx
	mov	%rax, 24(%rcx)
	mov	%r9, 16(%rcx)
	sub	$4, %r8
	mov	%r10, 8(%rcx)
	mov	%r11, (%rcx)
	jnc	Ltop

Lend:	shr	%r8d
	jnc	1f
	mov	(%rdx), %rax
	mov	%rax, -8(%rcx)
	lea	-8(%rcx), %rcx
	lea	-8(%rdx), %rdx
1:	shr	%r8d
	jnc	1f
	mov	(%rdx), %rax
	mov	-8(%rdx), %r9
	mov	%rax, -8(%rcx)
	mov	%r9, -16(%rcx)
1:	ret
	
