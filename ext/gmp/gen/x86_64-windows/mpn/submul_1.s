








































































				
		


	.text
	.align	16, 0x90
	.globl	__gmpn_submul_1
	.type	__gmpn_submul_1,@function
	
__gmpn_submul_1:

	mov	%rdx, %rax
	mov	%rcx, %rdx
	mov	%rax, %rcx
	test	$1, %cl
	jz	.Lbx0

.Lbx1:	.byte	0xc4,226,179,0xf6,6
	test	$2, %cl
	stc
	jz	.Lb01

.Lb11:	lea	1(%rcx), %rcx
	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi
	jmp	.Llo3

.Lb01:	lea	3(%rcx), %rcx
	jmp	.Llo1

.Lbx0:	.byte	0xc4,98,179,0xf6,6
	test	$2, %cl
	stc
	jz	.Lb00

.Lb10:	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	lea	2(%rcx), %rcx
	jmp	.Llo2

.Lb00:	lea	24(%rsi), %rsi
	lea	24(%rdi), %rdi
	jmp	.Llo0

.Ltop:	lea	32(%rsi), %rsi
	lea	32(%rdi), %rdi
	.byte	0xc4,98,179,0xf6,70,232
	.byte	0xf3,76,0x0f,0x38,0xf6,200
.Llo0:	not	%r9
	.byte	0x66,76,0x0f,0x38,0xf6,79,232
	mov	%r9, -24(%rdi)
	.byte	0xc4,226,179,0xf6,70,240
	.byte	0xf3,77,0x0f,0x38,0xf6,200
.Llo3:	not	%r9
	.byte	0x66,76,0x0f,0x38,0xf6,79,240
	mov	%r9, -16(%rdi)
	.byte	0xc4,98,179,0xf6,70,248
	.byte	0xf3,76,0x0f,0x38,0xf6,200
.Llo2:	not	%r9
	.byte	0x66,76,0x0f,0x38,0xf6,79,248
	mov	%r9, -8(%rdi)
	.byte	0xc4,226,179,0xf6,6
	.byte	0xf3,77,0x0f,0x38,0xf6,200
.Llo1:	not	%r9
	.byte	0x66,76,0x0f,0x38,0xf6,15
	mov	%r9, (%rdi)
	lea	-4(%rcx), %rcx
	jrcxz	.Lend
	jmp	.Ltop

.Lend:	.byte	0xf3,72,0x0f,0x38,0xf6,193
	sbb	$-1, %rax
	ret
	.size	__gmpn_submul_1,.-__gmpn_submul_1

