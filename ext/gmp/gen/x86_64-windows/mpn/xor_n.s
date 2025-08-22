









































































  
  
  

















	.text
	.align	32, 0x90
	.globl	__gmpn_xor_n
	
	.def	__gmpn_xor_n
	.scl	2
	.type	32
	.endef
__gmpn_xor_n:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	(%rdx), %r8
	mov	%ecx, %eax
	lea	(%rdx,%rcx,8), %rdx
	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdi,%rcx,8), %rdi
	neg	%rcx
	and	$3, %eax
	je	Lb00
	cmp	$2, %eax
	jc	Lb01
	je	Lb10

Lb11:	xor	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	dec	%rcx
	jmp	Le11
Lb10:	add	$-2, %rcx
	jmp	Le10
Lb01:	xor	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	inc	%rcx
	jz	Lret

Ltop:	mov	(%rdx,%rcx,8), %r8
Lb00:	mov	8(%rdx,%rcx,8), %r9
	xor	(%rsi,%rcx,8), %r8
	xor	8(%rsi,%rcx,8), %r9
	nop				
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
Le11:	mov	16(%rdx,%rcx,8), %r8
Le10:	mov	24(%rdx,%rcx,8), %r9
	xor	16(%rsi,%rcx,8), %r8
	xor	24(%rsi,%rcx,8), %r9
	mov	%r8, 16(%rdi,%rcx,8)
	mov	%r9, 24(%rdi,%rcx,8)
	add	$4, %rcx
	jnc	Ltop

Lret:	pop	%rsi
	pop	%rdi
	ret
	





