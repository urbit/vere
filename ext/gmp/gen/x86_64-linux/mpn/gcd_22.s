












































































		.section	.rodata
	.align	64, 0x90
ctz_table:

	.byte	8
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	5
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	6
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	5
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	7
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	5
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	6
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	5
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	2
	.byte	0
	.byte	1
	.byte	0

	.size	ctz_table,.-ctz_table

















	.text
	.align	64, 0x90
	.globl	__gmpn_gcd_22
	.type	__gmpn_gcd_22,@function
	
__gmpn_gcd_22:

	
	mov	%rcx, %rax

	mov	ctz_table@GOTPCREL(%rip), %r10



	.align	16, 0x90
.Ltop:	mov	%rax, %rcx
	sub	%rsi, %rcx
	jz	.Llowz		
	mov	%rdx, %r11
	sbb	%rdi, %r11

	mov	%rsi, %r8
	mov	%rdi, %r9

	sub	%rax, %rsi
	sbb	%rdx, %rdi

.Lbck:	cmovc	%rcx, %rsi		
	cmovc	%r11, %rdi		
	cmovc	%r8, %rax		
	cmovc	%r9, %rdx		

	and	$255, %ecx
	movzbl	(%r10,%rcx), %ecx
	jz	.Lcount_better

.Lshr:	shr	%cl, %rsi
	mov	%rdi, %r11
	shr	%cl, %rdi
	neg	%rcx
	shl	%cl, %r11
	or	%r11, %rsi

	test	%rdx, %rdx
	jnz	.Ltop
	test	%rdi, %rdi
	jnz	.Ltop

.Lgcd_11:
	mov	%rax, %rdi

	jmp	__gmpn_gcd_11@PLT


.Lcount_better:
	rep;bsf	%rsi, %rcx		
	jmp	.Lshr

.Llowz:
	
	
	mov	%rdx, %rcx
	sub	%rdi, %rcx
	je	.Lend

	xor	%r11, %r11
	mov	%rsi, %r8
	mov	%rdi, %r9
	mov	%rdi, %rsi
	xor	%rdi, %rdi
	sub	%rdx, %rsi
	jmp	.Lbck

.Lend:	
	
	
	ret
	.size	__gmpn_gcd_22,.-__gmpn_gcd_22
