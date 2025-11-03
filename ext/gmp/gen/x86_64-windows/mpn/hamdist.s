





























































  
  
  
  
  
  
  
  
  
  









	.text
	.align	32, 0x90
	.globl	__gmpn_hamdist
	
	.def	__gmpn_hamdist
	.scl	2
	.type	32
	.endef
__gmpn_hamdist:

  	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
		
	push	%rbx
	mov	$0x5555555555555555, %r10
	push	%rbp
	mov	$0x3333333333333333, %r11
 	push	%r12		
	lea	(%rdi,%rdx,8), %rdi
	mov	$0x0f0f0f0f0f0f0f0f, %rcx
 	lea	(%rsi,%rdx,8), %rsi	
	neg	%rdx
	mov	$0x0101010101010101, %r12
	xor	%eax, %eax
	test	$1, %dl
	jz	Ltop

	mov	(%rdi,%rdx,8), %r8
 	xor	(%rsi,%rdx,8), %r8	

	mov	%r8, %r9
	shr	%r8
	and	%r10, %r8
	sub	%r8, %r9

	mov	%r9, %r8
	shr	$2, %r9
	and	%r11, %r8
	and	%r11, %r9
	add	%r8, %r9		

	dec	%rdx
	jmp	Lmid

	.align	16, 0x90
Ltop:	mov	(%rdi,%rdx,8), %r8
	mov	8(%rdi,%rdx,8), %rbx
 	xor	(%rsi,%rdx,8), %r8	
 	xor	8(%rsi,%rdx,8), %rbx	

	mov	%r8, %r9
	mov	%rbx, %rbp
	shr	%r8
	shr	%rbx
	and	%r10, %r8
	and	%r10, %rbx
	sub	%r8, %r9
	sub	%rbx, %rbp

	mov	%r9, %r8
	mov	%rbp, %rbx
	shr	$2, %r9
	shr	$2, %rbp
	and	%r11, %r8
	and	%r11, %r9
	and	%r11, %rbx
	and	%r11, %rbp
	add	%r8, %r9		
	add	%rbx, %rbp		

	add	%rbp, %r9		
Lmid:	mov	%r9, %r8
	shr	$4, %r9
	and	%rcx, %r8
	and	%rcx, %r9
	add	%r8, %r9		

	imul	%r12, %r9		
	shr	$56, %r9

	add	%r9, %rax		
	add	$2, %rdx
	jnc	Ltop

Lend:
 	pop	%r12		
	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret
	
