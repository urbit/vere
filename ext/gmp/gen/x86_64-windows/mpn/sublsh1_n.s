




































































	.text
	.align	16, 0x90
	.globl	__gmpn_sublsh1_n
	
	.def	__gmpn_sublsh1_n
	.scl	2
	.type	32
	.endef
__gmpn_sublsh1_n:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	push	%rbx
	push	%rbp

	mov	(%rdx), %r8
	mov	%ecx, %eax
	lea	(%rdi,%rcx,8), %rdi
	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdx,%rcx,8), %rdx
	neg	%rcx
	xor	%ebp, %ebp
	and	$3, %eax
	je	Lb00
	cmp	$2, %eax
	jc	Lb01
	je	Lb10

Lb11:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	sbb	%eax, %eax	
	mov	(%rsi,%rcx,8), %rbp
	mov	8(%rsi,%rcx,8), %rbx
	sub	%r8, %rbp
	sbb	%r9, %rbx
	mov	%rbp, (%rdi,%rcx,8)
	mov	%rbx, 8(%rdi,%rcx,8)
	mov	16(%rsi,%rcx,8), %rbp
	sbb	%r10, %rbp
	mov	%rbp, 16(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$3, %rcx
	jmp	Lent

Lb10:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	sbb	%eax, %eax	
	mov	(%rsi,%rcx,8), %rbp
	mov	8(%rsi,%rcx,8), %rbx
	sub	%r8, %rbp
	sbb	%r9, %rbx
	mov	%rbp, (%rdi,%rcx,8)
	mov	%rbx, 8(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$2, %rcx
	jmp	Lent

Lb01:	add	%r8, %r8
	sbb	%eax, %eax	
	mov	(%rsi,%rcx,8), %rbp
	sub	%r8, %rbp
	mov	%rbp, (%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	inc	%rcx
Lent:	jns	Lend

	.align	16, 0x90
Ltop:	add	%eax, %eax	

	mov	(%rdx,%rcx,8), %r8
Lb00:	adc	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	mov	24(%rdx,%rcx,8), %r11
	adc	%r11, %r11

	sbb	%eax, %eax	
	add	%ebp, %ebp	

	mov	(%rsi,%rcx,8), %rbp
	mov	8(%rsi,%rcx,8), %rbx
	sbb	%r8, %rbp
	sbb	%r9, %rbx
	mov	%rbp, (%rdi,%rcx,8)
	mov	%rbx, 8(%rdi,%rcx,8)
	mov	16(%rsi,%rcx,8), %rbp
	mov	24(%rsi,%rcx,8), %rbx
	sbb	%r10, %rbp
	sbb	%r11, %rbx
	mov	%rbp, 16(%rdi,%rcx,8)
	mov	%rbx, 24(%rdi,%rcx,8)

	sbb	%ebp, %ebp	
	add	$4, %rcx
	js	Ltop

Lend:	add	%ebp, %eax
	neg	%eax

	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret
	
