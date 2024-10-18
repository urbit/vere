













































































		.section	.rodata
	.align	64, 0x90
ctz_table:

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
	.globl	__gmpn_gcd_11
	.type	__gmpn_gcd_11,@function
	
__gmpn_gcd_11:

	
	mov	ctz_table@GOTPCREL(%rip), %r8


	jmp	.Lent

	.align	16, 0x90
.Ltop:	cmovc	%rdx, %rdi		
	cmovc	%rax, %rsi		
.Lmid:	and	$127, %edx
	movzbl	(%r8,%rdx), %ecx
	jz	.Lshift_alot
	shr	%cl, %rdi
.Lent:	mov	%rdi, %rax
	mov	%rsi, %rdx
	sub	%rdi, %rdx
	sub	%rsi, %rdi
	jnz	.Ltop

.Lend:	
	
	
	ret

.Lshift_alot:
	shr	$7, %rdi
	mov	%rdi, %rdx
	jmp	.Lmid
	.size	__gmpn_gcd_11,.-__gmpn_gcd_11
