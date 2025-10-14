



























































































	.text
	.align	5, 0x90
	.globl	___gmpn_mullo_basecase
	
	
___gmpn_mullo_basecase:

	

	mov	%rdx, %r8
	mov	(%rsi), %rdx

	cmp	$4, %rcx
	jb	Lsmall

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13

	mov	(%r8), %r9
	mov	8(%r8), %rbx

	lea	2(%rcx), %rbp
	shr	$2, %rbp
	neg	%rcx
	add	$2, %rcx

	push	%rsi			

	test	$1, %cl
	jnz	Lm2x1

Lm2x0:.byte	0xc4,66,171,0xf6,233
	xor	%r12d, %r12d
	test	$2, %cl
	jz	Lm2b2

Lm2b0:lea	-8(%rdi), %rdi
	lea	-8(%rsi), %rsi
	jmp	Lm2e0

Lm2b2:lea	-24(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	Lm2e2

Lm2x1:.byte	0xc4,66,155,0xf6,217
	xor	%r10d, %r10d
	test	$2, %cl
	jnz	Lm2b3

Lm2b1:jmp	Lm2e1

Lm2b3:lea	-16(%rdi), %rdi
	lea	-16(%rsi), %rsi
	jmp	Lm2e3

	.align	4, 0x90
Lm2tp:.byte	0xc4,98,251,0xf6,211
	add	%rax, %r12
	mov	(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
	add	%rax, %r12
	adc	$0, %r11
	add	%r13, %r12
Lm2e1:mov	%r12, (%rdi)
	adc	$0, %r11
	.byte	0xc4,98,251,0xf6,227
	add	%rax, %r10
	mov	8(%rsi), %rdx
	adc	$0, %r12
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r10
	adc	$0, %r13
	add	%r11, %r10
Lm2e0:mov	%r10, 8(%rdi)
	adc	$0, %r13
	.byte	0xc4,98,251,0xf6,211
	add	%rax, %r12
	mov	16(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
	add	%rax, %r12
	adc	$0, %r11
	add	%r13, %r12
Lm2e3:mov	%r12, 16(%rdi)
	adc	$0, %r11
	.byte	0xc4,98,251,0xf6,227
	add	%rax, %r10
	mov	24(%rsi), %rdx
	adc	$0, %r12
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r10
	adc	$0, %r13
	add	%r11, %r10
	lea	32(%rsi), %rsi
Lm2e2:mov	%r10, 24(%rdi)
	adc	$0, %r13
	dec	%rbp
	lea	32(%rdi), %rdi
	jnz	Lm2tp

Lm2ed:.byte	0xc4,98,251,0xf6,211
	add	%rax, %r12
	mov	(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,217
	add	%r12, %rax
	add	%r13, %rax
	mov	%rax, (%rdi)

	mov	(%rsp), %rsi		
	lea	16(%r8), %r8
	lea	8(%rdi,%rcx,8), %rdi		
	add	$2, %rcx
	jge	Lcor1

	push	%r14
	push	%r15

Louter:
	mov	(%r8), %r9
	mov	8(%r8), %rbx

	lea	(%rcx), %rbp
	sar	$2, %rbp

	mov	(%rsi), %rdx
	test	$1, %cl
	jnz	Lbx1

Lbx0:	mov	(%rdi), %r15
	mov	8(%rdi), %r14
	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,98,251,0xf6,211
	add	%rax, %r14
	adc	$0, %r10
	mov	8(%rsi), %rdx
	mov	%r15, (%rdi)
	.byte	0xc4,66,251,0xf6,217
	test	$2, %cl
	jz	Lb2

Lb0:	lea	8(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	Llo0

Lb2:	mov	16(%rdi), %r15
	lea	24(%rdi), %rdi
	lea	24(%rsi), %rsi
	jmp	Llo2

Lbx1:	mov	(%rdi), %r14
	mov	8(%rdi), %r15
	.byte	0xc4,66,251,0xf6,217
	add	%rax, %r14
	.byte	0xc4,98,251,0xf6,227
	adc	$0, %r11
	mov	%r14, (%rdi)
	add	%rax, %r15
	adc	$0, %r12
	mov	8(%rsi), %rdx
	test	$2, %cl
	jnz	Lb3

Lb1:	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi
	jmp	Llo1

Lb3:	mov	16(%rdi), %r14
	lea	32(%rsi), %rsi
	.byte	0xc4,66,251,0xf6,233
	inc	%rbp
	jz	Lcj3
	jmp	Llo3

	.align	4, 0x90
Ltop:	.byte	0xc4,66,251,0xf6,233
	add	%r10, %r15
	adc	$0, %r12
Llo3:	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,98,251,0xf6,211
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
Llo2:	add	%rax, %r14
	.byte	0xc4,98,251,0xf6,227
	adc	$0, %r11
	add	%r13, %r14
	mov	%r14, -16(%rdi)
	adc	$0, %r11
	add	%rax, %r15
	adc	$0, %r12
	add	%r10, %r15
	mov	-8(%rsi), %rdx
	adc	$0, %r12
Llo1:	.byte	0xc4,66,251,0xf6,233
	add	%rax, %r15
	adc	$0, %r13
	mov	(%rdi), %r14
	.byte	0xc4,98,251,0xf6,211
	add	%rax, %r14
	adc	$0, %r10
	add	%r11, %r15
	mov	%r15, -8(%rdi)
	adc	$0, %r13
	mov	(%rsi), %rdx
	add	%r12, %r14
	.byte	0xc4,66,251,0xf6,217
	adc	$0, %r10
Llo0:	add	%rax, %r14
	adc	$0, %r11
	.byte	0xc4,98,251,0xf6,227
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
Lcj3:	add	%rax, %r15
	adc	$0, %r13
	.byte	0xc4,98,251,0xf6,211
	add	%rax, %r14
	add	%r11, %r15
	mov	-16(%rsi), %rdx
	mov	%r15, 8(%rdi)
	adc	$0, %r13
	add	%r12, %r14
	.byte	0xc4,66,251,0xf6,217
	add	%r14, %rax
	add	%r13, %rax
	mov	%rax, 16(%rdi)

	mov	16(%rsp), %rsi		
	lea	16(%r8), %r8
	lea	24(%rdi,%rcx,8), %rdi		
	add	$2, %rcx
	jl	Louter

	pop	%r15
	pop	%r14

	jnz	Lcor0

Lcor1:mov	(%r8), %r9
	mov	8(%r8), %rbx
	mov	(%rsi), %rdx
	.byte	0xc4,194,155,0xf6,233		
	add	(%rdi), %r12		
	adc	%rax, %rbp
	mov	8(%rsi), %r10
	imul	%r9, %r10
	imul	%rbx, %rdx
	mov	%r12, (%rdi)
	add	%r10, %rdx
	add	%rbp, %rdx
	mov	%rdx, 8(%rdi)
	pop	%rax			
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret

Lcor0:mov	(%r8), %r11
	imul	(%rsi), %r11
	add	%rax, %r11
	mov	%r11, (%rdi)
	pop	%rax			
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret

	.align	4, 0x90
Lsmall:
	cmp	$2, %rcx
	jae	Lgt1
Ln1:	imul	(%r8), %rdx
	mov	%rdx, (%rdi)
	
	ret
Lgt1:	ja	Lgt2
Ln2:	mov	(%r8), %r9
	.byte	0xc4,194,251,0xf6,209
	mov	%rax, (%rdi)
	mov	8(%rsi), %rax
	imul	%r9, %rax
	add	%rax, %rdx
	mov	8(%r8), %r9
	mov	(%rsi), %rcx
	imul	%r9, %rcx
	add	%rcx, %rdx
	mov	%rdx, 8(%rdi)
	
	ret
Lgt2:
Ln3:	mov	(%r8), %r9
	.byte	0xc4,66,251,0xf6,209	
	mov	%rax, (%rdi)
	mov	8(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,209	
	imul	16(%rsi), %r9		
	add	%rax, %r10
	adc	%rdx, %r9
	mov	8(%r8), %r11
	mov	(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,211	
	add	%rax, %r10
	adc	%rdx, %r9
	imul	8(%rsi), %r11		
	add	%r11, %r9
	mov	%r10, 8(%rdi)
	mov	16(%r8), %r10
	mov	(%rsi), %rax
	imul	%rax, %r10		
	add	%r10, %r9
	mov	%r9, 16(%rdi)
	
	ret
	
