






































































   
   
   
   





  
  
  









	.text
	.align	4, 0x90
	.globl	___gmpn_addmul_1
	
	
___gmpn_addmul_1:

	
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13

	mov	%rdx, %rbp
	mov	%rcx, %rdx

	test	$1, %bpl
	jnz	Lbx1

Lbx0:	shr	$2, %rbp
	jc	Lb10

Lb00:	.byte	0xc4,98,147,0xf6,38
	.byte	0xc4,226,227,0xf6,70,8
	add	%r12, %rbx
	adc	$0, %rax
	mov	(%rdi), %r12
	mov	8(%rdi), %rcx
	.byte	0xc4,98,179,0xf6,70,16
	lea	-16(%rdi), %rdi
	lea	16(%rsi), %rsi
	add	%r13, %r12
	jmp	Llo0

Lbx1:	shr	$2, %rbp
	jc	Lb11

Lb01:	.byte	0xc4,98,163,0xf6,22
	jnz	Lgt1
Ln1:	add	%r11, (%rdi)
	mov	$0, %eax
	adc	%r10, %rax
	jmp	Lret

Lgt1:	.byte	0xc4,98,147,0xf6,102,8
	.byte	0xc4,226,227,0xf6,70,16
	lea	24(%rsi), %rsi
	add	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	(%rdi), %r10
	mov	8(%rdi), %r12
	mov	16(%rdi), %rcx
	lea	-8(%rdi), %rdi
	add	%r11, %r10
	jmp	Llo1

Lb11:	.byte	0xc4,226,227,0xf6,6
	mov	(%rdi), %rcx
	.byte	0xc4,98,179,0xf6,70,8
	lea	8(%rsi), %rsi
	lea	-24(%rdi), %rdi
	inc	%rbp			
	add	%rbx, %rcx
	jmp	Llo3

Lb10:	.byte	0xc4,98,179,0xf6,6
	.byte	0xc4,98,163,0xf6,86,8
	lea	-32(%rdi), %rdi
	mov	$0, %eax
	clc				
	jz	Lend			

	.align	4, 0x90
Ltop:	adc	%rax, %r9
	lea	32(%rdi), %rdi
	adc	%r8, %r11
	.byte	0xc4,98,147,0xf6,102,16
	mov	(%rdi), %r8
	.byte	0xc4,226,227,0xf6,70,24
	lea	32(%rsi), %rsi
	adc	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	8(%rdi), %r10
	mov	16(%rdi), %r12
	add	%r9, %r8
	mov	24(%rdi), %rcx
	mov	%r8, (%rdi)
	adc	%r11, %r10
Llo1:	.byte	0xc4,98,179,0xf6,6
	mov	%r10, 8(%rdi)
	adc	%r13, %r12
Llo0:	mov	%r12, 16(%rdi)
	adc	%rbx, %rcx
Llo3:	.byte	0xc4,98,163,0xf6,86,8
	mov	%rcx, 24(%rdi)
	dec	%rbp
	jnz	Ltop

Lend:	adc	%rax, %r9
	adc	%r8, %r11
	mov	32(%rdi), %r8
	mov	%r10, %rax
	adc	$0, %rax
	mov	40(%rdi), %r10
	add	%r9, %r8
	mov	%r8, 32(%rdi)
	adc	%r11, %r10
	mov	%r10, 40(%rdi)
	adc	$0, %rax

Lret:	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	
