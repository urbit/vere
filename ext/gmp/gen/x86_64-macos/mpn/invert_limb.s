


























































.private_extern __gmpn_invert_limb_table


	.text
	.align	4, 0x90
	.globl	___gmpn_invert_limb
	
	
___gmpn_invert_limb:
		
	
	mov	%rdi, %rax		
	shr	$55, %rax		

	lea	__gmpn_invert_limb_table(%rip), %r8
	add	$-512, %r8

	movzwl	(%r8,%rax,2), %ecx	

	
	mov	%rdi, %rsi		
	mov	%ecx, %eax	
	imul	%ecx, %ecx	
	shr	$24, %rsi		
	inc	%rsi			
	imul	%rsi, %rcx		
	shr	$40, %rcx		
	sal	$11, %eax		
	dec	%eax
	sub	%ecx, %eax	

	
	mov	$0x1000000000000000, %rcx
	imul	%rax, %rsi		
	sub	%rsi, %rcx
	imul	%rax, %rcx
	sal	$13, %rax
	shr	$47, %rcx
	add	%rax, %rcx		

	
	mov	%rdi, %rsi		
	shr	%rsi			
	sbb	%rax, %rax		
	sub	%rax, %rsi		
	imul	%rcx, %rsi		
	and	%rcx, %rax		
	shr	%rax			
	sub	%rsi, %rax		
	mul	%rcx
	sal	$31, %rcx
	shr	%rdx
	add	%rdx, %rcx		

	mov	%rdi, %rax
	mul	%rcx
	add	%rdi, %rax
	mov	%rcx, %rax
	adc	%rdi, %rdx
	sub	%rdx, %rax

	
	ret
	

