



































































   
   
   
   








	.text
	.align	5, 0x90
	.globl	___gmpn_mul_1
	
	
___gmpn_mul_1:

	
	push	%rbx
	push	%rbp
	push	%r12

	mov	%rdx, %rbp
	shr	$2, %rbp

	test	$1, %dl
	jnz	Lbx1

Lbx0:	test	$2, %dl
	mov	%rcx, %rdx
	jnz	Lb10

Lb00:	.byte	0xc4,98,179,0xf6,6
	.byte	0xc4,98,163,0xf6,86,8
	.byte	0xc4,98,243,0xf6,102,16
	lea	-32(%rdi), %rdi
	jmp	Llo0

Lb10:	.byte	0xc4,98,243,0xf6,38
	.byte	0xc4,226,227,0xf6,70,8
	lea	-16(%rdi), %rdi
	test	%rbp, %rbp
	jz	Lcj2
	.byte	0xc4,98,179,0xf6,70,16
	lea	16(%rsi), %rsi
	jmp	Llo2

Lbx1:	test	$2, %dl
	mov	%rcx, %rdx
	jnz	Lb11

Lb01:	.byte	0xc4,226,227,0xf6,6
	lea	-24(%rdi), %rdi
	test	%rbp, %rbp
	jz	Lcj1
	.byte	0xc4,98,179,0xf6,70,8
	lea	8(%rsi), %rsi
	jmp	Llo1

Lb11:	.byte	0xc4,98,163,0xf6,22
	.byte	0xc4,98,243,0xf6,102,8
	.byte	0xc4,226,227,0xf6,70,16
	lea	-8(%rdi), %rdi
	test	%rbp, %rbp
	jz	Lcj3
	lea	24(%rsi), %rsi
	jmp	Llo3

	.align	5, 0x90
Ltop:	lea	32(%rdi), %rdi
	mov	%r9, (%rdi)
	adc	%r8, %r11
Llo3:	.byte	0xc4,98,179,0xf6,6
	mov	%r11, 8(%rdi)
	adc	%r10, %rcx
Llo2:	mov	%rcx, 16(%rdi)
	adc	%r12, %rbx
Llo1:	.byte	0xc4,98,163,0xf6,86,8
	adc	%rax, %r9
	.byte	0xc4,98,243,0xf6,102,16
	mov	%rbx, 24(%rdi)
Llo0:	.byte	0xc4,226,227,0xf6,70,24
	lea	32(%rsi), %rsi
	dec	%rbp
	jnz	Ltop

Lend:	lea	32(%rdi), %rdi
	mov	%r9, (%rdi)
	adc	%r8, %r11
Lcj3:	mov	%r11, 8(%rdi)
	adc	%r10, %rcx
Lcj2:	mov	%rcx, 16(%rdi)
	adc	%r12, %rbx
Lcj1:	mov	%rbx, 24(%rdi)
	adc	$0, %rax

	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	

