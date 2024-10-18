






























































	
	
	
	








	.text
	.align	16, 0x90
	.globl	__gmpn_rsh1add_nc
	.type	__gmpn_rsh1add_nc,@function
	
__gmpn_rsh1add_nc:

	

	push	%rbx

	xor	%eax, %eax
	neg	%r8			
	mov	(%rsi), %rbx
	adc	(%rdx), %rbx
	jmp	.Lent
	.size	__gmpn_rsh1add_nc,.-__gmpn_rsh1add_nc

	.align	16, 0x90
	.globl	__gmpn_rsh1add_n
	.type	__gmpn_rsh1add_n,@function
	
__gmpn_rsh1add_n:

	
	push	%rbx

	xor	%eax, %eax
	mov	(%rsi), %rbx
	add	(%rdx), %rbx
.Lent:
	rcr	%rbx			
	adc	%eax, %eax	

	mov	%ecx, %r11d
	and	$3, %r11d

	cmp	$1, %r11d
	je	.Ldo			

.Ln1:	cmp	$2, %r11d
	jne	.Ln2			
	add	%rbx, %rbx		
	mov	8(%rsi), %r10
	adc	8(%rdx), %r10
	lea	8(%rsi), %rsi
	lea	8(%rdx), %rdx
	lea	8(%rdi), %rdi
	rcr	%r10
	rcr	%rbx
	mov	%rbx, -8(%rdi)
	jmp	.Lcj1

.Ln2:	cmp	$3, %r11d
	jne	.Ln3			
	add	%rbx, %rbx		
	mov	8(%rsi), %r9
	mov	16(%rsi), %r10
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	lea	16(%rsi), %rsi
	lea	16(%rdx), %rdx
	lea	16(%rdi), %rdi
	rcr	%r10
	rcr	%r9
	rcr	%rbx
	mov	%rbx, -16(%rdi)
	jmp	.Lcj2

.Ln3:	dec	  %rcx			
	add	%rbx, %rbx		
	mov	8(%rsi), %r8
	mov	16(%rsi), %r9
	adc	8(%rdx), %r8
	adc	16(%rdx), %r9
	mov	24(%rsi), %r10
	adc	24(%rdx), %r10
	lea	24(%rsi), %rsi
	lea	24(%rdx), %rdx
	lea	24(%rdi), %rdi
	rcr	%r10
	rcr	%r9
	rcr	%r8
	rcr	%rbx
	mov	%rbx, -24(%rdi)
	mov	%r8, -16(%rdi)
.Lcj2:	mov	%r9, -8(%rdi)
.Lcj1:	mov	%r10, %rbx

.Ldo:
	shr	$2,   %rcx			
	je	.Lend			
	.align	16, 0x90
.Ltop:	add	%rbx, %rbx		

	mov	8(%rsi), %r8
	mov	16(%rsi), %r9
	adc	8(%rdx), %r8
	adc	16(%rdx), %r9
	mov	24(%rsi), %r10
	mov	32(%rsi), %r11
	adc	24(%rdx), %r10
	adc	32(%rdx), %r11

	lea	32(%rsi), %rsi
	lea	32(%rdx), %rdx

	rcr	%r11			
	rcr	%r10
	rcr	%r9
	rcr	%r8

	rcr	%rbx
	mov	%rbx, (%rdi)
	mov	%r8, 8(%rdi)
	mov	%r9, 16(%rdi)
	mov	%r10, 24(%rdi)
	mov	%r11, %rbx

	lea	32(%rdi), %rdi
	dec	  %rcx
	jne	.Ltop

.Lend:	mov	%rbx, (%rdi)
	pop	%rbx
	
	ret
	.size	__gmpn_rsh1add_n,.-__gmpn_rsh1add_n
