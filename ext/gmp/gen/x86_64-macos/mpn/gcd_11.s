




























































































	.text
	.align	6, 0x90
	.globl	___gmpn_gcd_11
	
	
___gmpn_gcd_11:

	
	jmp	Lodd

	.align	4, 0x90
Ltop:	cmovc	%rdx, %rdi		
	cmovc	%rax, %rsi		
	shr	%cl, %rdi
Lodd:	mov	%rsi, %rdx
	sub	%rdi, %rdx		
	bsf	%rdx, %rcx
	mov	%rdi, %rax
	sub	%rsi, %rdi			
	jnz	Ltop

Lend:	
	
	
	ret
	

