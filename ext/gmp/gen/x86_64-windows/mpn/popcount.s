




























































  
  
  
  
  
  
  
  
  










	.text
	.align	32, 0x90
	.globl	__gmpn_popcount
	
	.def	__gmpn_popcount
	.scl	2
	.type	32
	.endef
__gmpn_popcount:

 	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
		
 	push	%rbx
	mov	$0x5555555555555555, %r10
	push	%rbp
	mov	$0x3333333333333333, %r11
 	lea	(%rdi,%rsi,8), %rdi
	mov	$0x0f0f0f0f0f0f0f0f, %rcx
 	neg	%rsi
	mov	$0x0101010101010101, %rdx
	xor	%eax, %eax
	test	$1, %sil
	jz	Ltop

	mov	(%rdi,%rsi,8), %r8
 
	mov	%r8, %r9
	shr	%r8
	and	%r10, %r8
	sub	%r8, %r9

	mov	%r9, %r8
	shr	$2, %r9
	and	%r11, %r8
	and	%r11, %r9
	add	%r8, %r9		

	dec	%rsi
	jmp	Lmid

	.align	16, 0x90
Ltop:	mov	(%rdi,%rsi,8), %r8
	mov	8(%rdi,%rsi,8), %rbx
  
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

	imul	%rdx, %r9		
	shr	$56, %r9

	add	%r9, %rax		
	add	$2, %rsi
	jnc	Ltop

Lend:
 	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret
	
