





















































































	.text
	.align	64, 0x90
	.globl	__gmpn_gcd_22
	.type	__gmpn_gcd_22,@function
	
__gmpn_gcd_22:

	

	.align	16, 0x90
.Ltop:	mov	%rcx, %r10
	sub	%rsi, %r10
	jz	.Llowz		
	mov	%rdx, %r11
	sbb	%rdi, %r11

	rep;bsf	%r10, %rax		

	mov	%rsi, %r8
	sub	%rcx, %rsi
	mov	%rdi, %r9
	sbb	%rdx, %rdi

.Lbck:	cmovc	%r10, %rsi		
	cmovc	%r11, %rdi		
	cmovc	%r8, %rcx		
	cmovc	%r9, %rdx		

	xor	%r10d, %r10d
	sub	%rax, %r10
	.byte	0xc4,98,169,0xf7,207
	.byte	0xc4,226,251,0xf7,246
	.byte	0xc4,226,251,0xf7,255
	or	%r9, %rsi

	test	%rdx, %rdx
	jnz	.Ltop
	test	%rdi, %rdi
	jnz	.Ltop

.Lgcd_11:
	mov	%rcx, %rdi

	jmp	__gmpn_gcd_11@PLT


.Llowz:
	
	
	mov	%rdx, %r10
	sub	%rdi, %r10
	je	.Lend

	xor	%r11, %r11
	mov	%rsi, %r8
	mov	%rdi, %r9
	rep;bsf	%r10, %rax		
	mov	%rdi, %rsi
	xor	%rdi, %rdi
	sub	%rdx, %rsi
	jmp	.Lbck

.Lend:	mov	%rcx, %rax
	
.Lret:	
	ret
	.size	__gmpn_gcd_22,.-__gmpn_gcd_22
