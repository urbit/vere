














































































	.text
	.align	5, 0x90
	.globl	___gmpn_modexact_1_odd
	
	
___gmpn_modexact_1_odd:

	
	mov	$0, %ecx


	.globl	___gmpn_modexact_1c_odd
	
	
___gmpn_modexact_1c_odd:

	
Lent:
	
	
	
	

	mov	%rdx, %r8		
	shr	%edx		

	lea	___gmp_binvert_limb_table(%rip), %r9



	and	$127, %edx
	mov	%rcx, %r10		

	movzbl	(%r9,%rdx), %edx	

	mov	(%rdi), %rax		
	lea	(%rdi,%rsi,8), %r11	
	mov	%r8, %rdi		

	lea	(%rdx,%rdx), %ecx	
	imul	%edx, %edx	

	neg	%rsi			

	imul	%edi, %edx	

	sub	%edx, %ecx	

	lea	(%rcx,%rcx), %edx	
	imul	%ecx, %ecx	

	imul	%edi, %ecx	

	sub	%ecx, %edx	
	xor	%ecx, %ecx	

	lea	(%rdx,%rdx), %r9	
	imul	%rdx, %rdx		

	imul	%r8, %rdx		

	sub	%rdx, %r9		
	mov	%r10, %rdx		

	

	inc	%rsi
	jz	Lone


	.align	4, 0x90
Ltop:
	
	
	
	
	
	
	
	

	sub	%rdx, %rax		

	adc	$0, %rcx		
	imul	%r9, %rax		

	mul	%r8			

	mov	(%r11,%rsi,8), %rax	
	sub	%rcx, %rax		
	setc	%cl		

	inc	%rsi
	jnz	Ltop


Lone:
	sub	%rdx, %rax		

	adc	$0, %rcx		
	imul	%r9, %rax		

	mul	%r8			

	lea	(%rcx,%rdx), %rax	
	
	ret

	
	
