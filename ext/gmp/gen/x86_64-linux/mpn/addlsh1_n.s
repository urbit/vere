





































































  
  
  








	.text
	.align	16, 0x90
	.globl	__gmpn_addlsh1_n
	.type	__gmpn_addlsh1_n,@function
	
__gmpn_addlsh1_n:

	
	push	%rbp

	mov	(%rdx), %r8
	mov	%ecx, %eax
	lea	(%rdi,%rcx,8), %rdi
	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdx,%rcx,8), %rdx
	neg	%rcx
	xor	%ebp, %ebp
	and	$3, %eax
	je	.Lb00
	cmp	$2, %eax
	jc	.Lb01
	je	.Lb10

.Lb11:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	sbb	%eax, %eax	
	add	(%rsi,%rcx,8), %r8
	adc	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	adc	16(%rsi,%rcx,8), %r10
	mov	%r10, 16(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$3, %rcx
	jmp	.Lent

.Lb10:	add	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	sbb	%eax, %eax	
	add	(%rsi,%rcx,8), %r8
	adc	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	add	$2, %rcx
	jmp	.Lent

.Lb01:	add	%r8, %r8
	sbb	%eax, %eax	
	add	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	sbb	%ebp, %ebp	
	inc	%rcx
.Lent:	jns	.Lend

	.align	16, 0x90
.Ltop:	add	%eax, %eax	

	mov	(%rdx,%rcx,8), %r8
.Lb00:	adc	%r8, %r8
	mov	8(%rdx,%rcx,8), %r9
	adc	%r9, %r9
	mov	16(%rdx,%rcx,8), %r10
	adc	%r10, %r10
	mov	24(%rdx,%rcx,8), %r11
	adc	%r11, %r11

	sbb	%eax, %eax	
	add	%ebp, %ebp	

	adc	(%rsi,%rcx,8), %r8
	nop				
	adc	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
	adc	16(%rsi,%rcx,8), %r10
	adc	24(%rsi,%rcx,8), %r11
	mov	%r10, 16(%rdi,%rcx,8)
	mov	%r11, 24(%rdi,%rcx,8)

	sbb	%ebp, %ebp	
	add	$4, %rcx
	js	.Ltop

.Lend:

	add	%ebp, %eax
	neg	%eax


	pop	%rbp
	
	ret
	.size	__gmpn_addlsh1_n,.-__gmpn_addlsh1_n
