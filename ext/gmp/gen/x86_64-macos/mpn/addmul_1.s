











































































   
   
   
   








	.text
	.align	5, 0x90
	.globl	___gmpn_addmul_1
	
	
___gmpn_addmul_1:

	

	mov	%rcx, %r10
	mov	%rdx, %rcx
	mov	%edx, %r8d
	shr	$3, %rcx
	and	$7, %r8d		
	mov	%r10, %rdx
	lea	Ltab(%rip), %r10
	movslq	(%r10,%r8,4), %r8
	lea	(%r8, %r10), %r10
	jmp	*%r10

	.text
	.align	3, 0x90
Ltab:	.set	Lf0_tmp, Lf0-Ltab
	.long	Lf0_tmp

	.set	Lf1_tmp, Lf1-Ltab
	.long	Lf1_tmp

	.set	Lf2_tmp, Lf2-Ltab
	.long	Lf2_tmp

	.set	Lf3_tmp, Lf3-Ltab
	.long	Lf3_tmp

	.set	Lf4_tmp, Lf4-Ltab
	.long	Lf4_tmp

	.set	Lf5_tmp, Lf5-Ltab
	.long	Lf5_tmp

	.set	Lf6_tmp, Lf6-Ltab
	.long	Lf6_tmp

	.set	Lf7_tmp, Lf7-Ltab
	.long	Lf7_tmp

	.text

Lf0:	.byte	0xc4,98,171,0xf6,6
	lea	-8(%rsi), %rsi
	lea	-8(%rdi), %rdi
	lea	-1(%rcx), %rcx
	jmp	Lb0

Lf3:	.byte	0xc4,226,179,0xf6,6
	lea	16(%rsi), %rsi
	lea	-48(%rdi), %rdi
	jmp	Lb3

Lf4:	.byte	0xc4,98,171,0xf6,6
	lea	24(%rsi), %rsi
	lea	-40(%rdi), %rdi
	jmp	Lb4

Lf5:	.byte	0xc4,226,179,0xf6,6
	lea	32(%rsi), %rsi
	lea	-32(%rdi), %rdi
	jmp	Lb5

Lf6:	.byte	0xc4,98,171,0xf6,6
	lea	40(%rsi), %rsi
	lea	-24(%rdi), %rdi
	jmp	Lb6

Lf1:	.byte	0xc4,226,179,0xf6,6
	jrcxz	L1
	jmp	Lb1
L1:	add	(%rdi), %r9
	mov	%r9, (%rdi)
	adc	%rcx, %rax		
	
	ret

Lend:	.byte	0xf3,76,0x0f,0x38,0xf6,15
	mov	%r9, (%rdi)
	.byte	0xf3,72,0x0f,0x38,0xf6,193		
	adc	%rcx, %rax		
	
	ret

	nop;nop;nop;nop

Lf2:	.byte	0xc4,98,171,0xf6,6
	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	.byte	0xc4,226,179,0xf6,6

	.align	5, 0x90
Ltop:	.byte	0xf3,76,0x0f,0x38,0xf6,87,248
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, -8(%rdi)
	jrcxz	Lend
Lb1:	.byte	0xc4,98,171,0xf6,70,8
	.byte	0xf3,76,0x0f,0x38,0xf6,15
	lea	-1(%rcx), %rcx
	mov	%r9, (%rdi)
	.byte	0x66,76,0x0f,0x38,0xf6,208
Lb0:	.byte	0xc4,226,179,0xf6,70,16
	.byte	0x66,77,0x0f,0x38,0xf6,200
	.byte	0xf3,76,0x0f,0x38,0xf6,87,8
	mov	%r10, 8(%rdi)
Lb7:	.byte	0xc4,98,171,0xf6,70,24
	lea	64(%rsi), %rsi
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xf3,76,0x0f,0x38,0xf6,79,16
	mov	%r9, 16(%rdi)
Lb6:	.byte	0xc4,226,179,0xf6,70,224
	.byte	0xf3,76,0x0f,0x38,0xf6,87,24
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, 24(%rdi)
Lb5:	.byte	0xc4,98,171,0xf6,70,232
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xf3,76,0x0f,0x38,0xf6,79,32
	mov	%r9, 32(%rdi)
Lb4:	.byte	0xc4,226,179,0xf6,70,240
	.byte	0xf3,76,0x0f,0x38,0xf6,87,40
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, 40(%rdi)
Lb3:	.byte	0xf3,76,0x0f,0x38,0xf6,79,48
	.byte	0xc4,98,171,0xf6,70,248
	mov	%r9, 48(%rdi)
	lea	64(%rdi), %rdi
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xc4,226,179,0xf6,6
	jmp	Ltop

Lf7:	.byte	0xc4,226,179,0xf6,6
	lea	-16(%rsi), %rsi
	lea	-16(%rdi), %rdi
	jmp	Lb7
	

