























































































	.text
	.align	4, 0x90
	.globl	___gmpn_mul_basecase
	
	
___gmpn_mul_basecase:

	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	mov	%rdx, %rbx		
	neg	%rbx

	mov	%rdx, %rbp		
	sar	$2, %rbp			

	test	$1, %r8b
	jz	Ldo_mul_2




	mov	(%rcx), %rdx

Ldo_mul_1:
	test	$1, %bl
	jnz	Lm1x1

Lm1x0:test	$2, %bl
	jnz	Lm110

Lm100:
	.byte	0xc4,98,139,0xf6,38
	.byte	0xc4,98,163,0xf6,110,8
	lea	-24(%rdi), %rdi
	jmp	Lm1l0

Lm110:
	.byte	0xc4,98,147,0xf6,14
	.byte	0xc4,98,163,0xf6,118,8
	lea	-8(%rdi), %rdi
	test	%rbp, %rbp
	jz	Lcj2
	.byte	0xc4,98,171,0xf6,102,16
	lea	16(%rsi), %rsi
	jmp	Lm1l2

Lm1x1:test	$2, %bl
	jz	Lm111

Lm101:
	.byte	0xc4,98,179,0xf6,54
	lea	-16(%rdi), %rdi
	test	%rbp, %rbp
	jz	Lcj1
	.byte	0xc4,98,171,0xf6,102,8
	lea	8(%rsi), %rsi
	jmp	Lm1l1

Lm111:
	.byte	0xc4,98,155,0xf6,46
	.byte	0xc4,98,171,0xf6,78,8
	.byte	0xc4,98,163,0xf6,118,16
	lea	24(%rsi), %rsi
	test	%rbp, %rbp
	jnz	Lgt3
	add	%r10, %r13
	jmp	Lcj3
Lgt3:	add	%r10, %r13
	jmp	Lm1l3

	.align	5, 0x90
Lm1tp:lea	32(%rdi), %rdi
Lm1l3:mov	%r12, (%rdi)
	.byte	0xc4,98,171,0xf6,38
Lm1l2:mov	%r13, 8(%rdi)
	adc	%r11, %r9
Lm1l1:adc	%r10, %r14
	mov	%r9, 16(%rdi)
	.byte	0xc4,98,163,0xf6,110,8
Lm1l0:mov	%r14, 24(%rdi)
	.byte	0xc4,98,171,0xf6,78,16
	adc	%r11, %r12
	.byte	0xc4,98,163,0xf6,118,24
	adc	%r10, %r13
	lea	32(%rsi), %rsi
	dec	%rbp
	jnz	Lm1tp

Lm1ed:lea	32(%rdi), %rdi
Lcj3:	mov	%r12, (%rdi)
Lcj2:	mov	%r13, 8(%rdi)
	adc	%r11, %r9
Lcj1:	mov	%r9, 16(%rdi)
	adc	$0, %r14
	mov	%r14, 24(%rdi)

	dec	%r8d
	jz	Lret5

	lea	8(%rcx), %rcx
	lea	32(%rdi), %rdi



	jmp	Ldo_addmul

Ldo_mul_2:





	mov	(%rcx), %r9
	mov	8(%rcx), %r14

	lea	(%rbx), %rbp
	sar	$2, %rbp

	test	$1, %bl
	jnz	Lm2x1

Lm2x0:xor	%r10, %r10
	test	$2, %bl
	mov	(%rsi), %rdx
	.byte	0xc4,66,155,0xf6,217
	jz	Lm2l0

Lm210:lea	-16(%rdi), %rdi
	lea	-16(%rsi), %rsi
	jmp	Lm2l2

Lm2x1:xor	%r12, %r12
	test	$2, %bl
	mov	(%rsi), %rdx
	.byte	0xc4,66,171,0xf6,233
	jz	Lm211

Lm201:lea	-24(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	Lm2l1

Lm211:lea	-8(%rdi), %rdi
	lea	-8(%rsi), %rsi
	jmp	Lm2l3

	.align	4, 0x90
Lm2tp:.byte	0xc4,66,251,0xf6,214
	add	%rax, %r12
	mov	(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
	add	%rax, %r12
	adc	$0, %r11
	add	%r13, %r12
Lm2l0:mov	%r12, (%rdi)
	adc	$0, %r11
	.byte	0xc4,66,251,0xf6,230
	add	%rax, %r10
	mov	8(%rsi), %rdx
	adc	$0, %r12
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r10
	adc	$0, %r13
	add	%r11, %r10
Lm2l3:mov	%r10, 8(%rdi)
	adc	$0, %r13
	.byte	0xc4,66,251,0xf6,214
	add	%rax, %r12
	mov	16(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
	add	%rax, %r12
	adc	$0, %r11
	add	%r13, %r12
Lm2l2:mov	%r12, 16(%rdi)
	adc	$0, %r11
	.byte	0xc4,66,251,0xf6,230
	add	%rax, %r10
	mov	24(%rsi), %rdx
	adc	$0, %r12
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r10
	adc	$0, %r13
	add	%r11, %r10
	lea	32(%rsi), %rsi
Lm2l1:mov	%r10, 24(%rdi)
	adc	$0, %r13
	inc	%rbp
	lea	32(%rdi), %rdi
	jnz	Lm2tp

Lm2ed:.byte	0xc4,194,235,0xf6,198
	add	%rdx, %r12
	adc	$0, %rax
	add	%r13, %r12
	mov	%r12, (%rdi)
	adc	$0, %rax
	mov	%rax, 8(%rdi)

	add	$-2, %r8d
	jz	Lret5
	lea	16(%rcx), %rcx
	lea	16(%rdi), %rdi


Ldo_addmul:
	push	%r15
	push	%r8			





	lea	(%rdi,%rbx,8), %rdi
	lea	(%rsi,%rbx,8), %rsi

Louter:
	mov	(%rcx), %r9
	mov	8(%rcx), %r8

	lea	2(%rbx), %rbp
	sar	$2, %rbp

	mov	(%rsi), %rdx
	test	$1, %bl
	jnz	Lbx1

Lbx0:	mov	(%rdi), %r14
	mov	8(%rdi), %r15
	.byte	0xc4,66,251,0xf6,217
	add	%rax, %r14
	.byte	0xc4,66,251,0xf6,224
	adc	$0, %r11
	mov	%r14, (%rdi)
	add	%rax, %r15
	adc	$0, %r12
	mov	8(%rsi), %rdx
	test	$2, %bl
	jnz	Lb10

Lb00:	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi
	jmp	Llo0

Lb10:	mov	16(%rdi), %r14
	lea	32(%rsi), %rsi
	.byte	0xc4,66,251,0xf6,233
	jmp	Llo2

Lbx1:	mov	(%rdi), %r15
	mov	8(%rdi), %r14
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r14
	adc	$0, %r10
	mov	8(%rsi), %rdx
	mov	%r15, (%rdi)
	.byte	0xc4,66,251,0xf6,217
	test	$2, %bl
	jz	Lb11

Lb01:	mov	16(%rdi), %r15
	lea	24(%rdi), %rdi
	lea	24(%rsi), %rsi
	jmp	Llo1

Lb11:	lea	8(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	Llo3

	.align	4, 0x90
Ltop:	.byte	0xc4,66,251,0xf6,233
	add	%r10, %r15
	adc	$0, %r12
Llo2:	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r14
	adc	$0, %r10
	lea	32(%rdi), %rdi
	add	%r11, %r15
	mov	-16(%rsi), %rdx
	mov	%r15, -24(%rdi)
	adc	$0, %r13
	add	%r12, %r14
	mov	-8(%rdi), %r15
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
Llo1:	add	%rax, %r14
	.byte	0xc4,66,251,0xf6,224
	adc	$0, %r11
	add	%r13, %r14
	mov	%r14, -16(%rdi)
	adc	$0, %r11
	add	%rax, %r15
	adc	$0, %r12
	add	%r10, %r15
	mov	-8(%rsi), %rdx
	adc	$0, %r12
Llo0:	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r15
	adc	$0, %r13
	mov	(%rdi), %r14
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r14
	adc	$0, %r10
	add	%r11, %r15
	mov	%r15, -8(%rdi)
	adc	$0, %r13
	mov	(%rsi), %rdx
	add	%r12, %r14
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
Llo3:	add	%rax, %r14
	adc	$0, %r11
	.byte	0xc4,66,251,0xf6,224
	add	%r13, %r14
	mov	8(%rdi), %r15
	mov	%r14, (%rdi)
	mov	16(%rdi), %r14
	adc	$0, %r11
	add	%rax, %r15
	adc	$0, %r12
	mov	8(%rsi), %rdx
	lea	32(%rsi), %rsi
	inc	%rbp
	jnz	Ltop

Lend:	.byte	0xc4,66,251,0xf6,233
	add	%r10, %r15
	adc	$0, %r12
	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,194,235,0xf6,192
	add	%r11, %r15
	mov	%r15, 8(%rdi)
	adc	$0, %r13
	add	%r12, %rdx
	adc	$0, %rax
	add	%r13, %rdx
	mov	%rdx, 16(%rdi)
	adc	$0, %rax
	mov	%rax, 24(%rdi)

	addl	$-2, (%rsp)
	lea	16(%rcx), %rcx
	lea	-16(%rsi,%rbx,8), %rsi
	lea	32(%rdi,%rbx,8), %rdi
	jnz	Louter

	pop	%rax		
	pop	%r15
Lret5:pop	%r14
Lret4:pop	%r13
Lret3:pop	%r12
Lret2:pop	%rbp
	pop	%rbx
	
	ret
	
