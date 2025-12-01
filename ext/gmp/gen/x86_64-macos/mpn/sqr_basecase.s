
























































































	.text
	.align	5, 0x90
	.globl	___gmpn_sqr_basecase
	
	
___gmpn_sqr_basecase:

	

	cmp	$2, %rdx
	jae	Lgt1

	mov	(%rsi), %rdx
	.byte	0xc4,226,251,0xf6,210
	mov	%rax, (%rdi)
	mov	%rdx, 8(%rdi)
	
	ret

Lgt1:	jne	Lgt2

	mov	(%rsi), %rdx
	mov	8(%rsi), %rcx
	.byte	0xc4,98,179,0xf6,209	
	.byte	0xc4,98,251,0xf6,194	
	mov	%rcx, %rdx
	.byte	0xc4,226,163,0xf6,210	
	add	%r9, %r9		
	adc	%r10, %r10		
	adc	$0, %rdx		
	add	%r9, %r8		
	adc	%r11, %r10		
	adc	$0, %rdx		
	mov	%rax, (%rdi)
	mov	%r8, 8(%rdi)
	mov	%r10, 16(%rdi)
	mov	%rdx, 24(%rdi)
	
	ret

Lgt2:	cmp	$4, %rdx
	jae	Lgt3





	mov	(%rsi), %r8
	mov	8(%rsi), %rdx
	mov	%rdx, %r9
	.byte	0xc4,194,163,0xf6,192
	mov	16(%rsi), %rdx
	.byte	0xc4,194,171,0xf6,200
	mov	%r11, %r8
	add	%rax, %r10
	adc	$0, %rcx
	.byte	0xc4,194,235,0xf6,193
	add	%rcx, %rdx
	mov	%rdx, 24(%rdi)
	adc	$0, %rax
	mov	%rax, 32(%rdi)
	xor	%ecx, %ecx
	mov	(%rsi), %rdx
	.byte	0xc4,98,251,0xf6,218
	mov	%rax, (%rdi)
	add	%r8, %r8
	adc	%r10, %r10
	setc	%cl
	mov	8(%rsi), %rdx
	.byte	0xc4,226,251,0xf6,210
	add	%r11, %r8
	adc	%rax, %r10
	mov	%r8, 8(%rdi)
	mov	%r10, 16(%rdi)
	mov	24(%rdi), %r8
	mov	32(%rdi), %r10
	lea	(%rdx,%rcx), %r11
	adc	%r8, %r8
	adc	%r10, %r10
	setc	%cl
	mov	16(%rsi), %rdx
	.byte	0xc4,226,251,0xf6,210
	add	%r11, %r8
	adc	%rax, %r10
	mov	%r8, 24(%rdi)
	mov	%r10, 32(%rdi)
	adc	%rcx, %rdx
	mov	%rdx, 40(%rdi)
	
	ret

Lgt3:













Ldo_mul_2:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	mov	$0, %r12d
	sub	%rdx, %r12		
	push	%r12
	mov	(%rsi), %r8
	mov	8(%rsi), %rdx
	lea	2(%r12), %rcx
	sar	$2, %rcx			
	inc	%r12			
	mov	%rdx, %r9

	test	$1, %r12b
	jnz	Lmx1

Lmx0:	.byte	0xc4,66,227,0xf6,216
	mov	16(%rsi), %rdx
	mov	%rbx, 8(%rdi)
	xor	%rbx, %rbx
	.byte	0xc4,194,171,0xf6,232
	test	$2, %r12b
	jz	Lm00

Lm10:	lea	-8(%rdi), %rdi
	lea	-8(%rsi), %rsi
	jmp	Lmlo2

Lm00:	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	jmp	Lmlo0

Lmx1:	.byte	0xc4,194,171,0xf6,232
	mov	16(%rsi), %rdx
	mov	%r10, 8(%rdi)
	xor	%r10, %r10
	.byte	0xc4,66,227,0xf6,216
	test	$2, %r12b
	jz	Lmlo3

Lm01:	lea	16(%rdi), %rdi
	lea	16(%rsi), %rsi
	jmp	Lmlo1

	.align	5, 0x90
Lmtop:.byte	0xc4,66,251,0xf6,209
	add	%rax, %rbx		
	mov	(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,216
	adc	$0, %r10			
	add	%rax, %rbx		
Lmlo1:adc	$0, %r11			
	add	%rbp, %rbx			
	mov	%rbx, (%rdi)		
	adc	$0, %r11			
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r10		
	mov	8(%rsi), %rdx
	adc	$0, %rbx			
	.byte	0xc4,194,251,0xf6,232
	add	%rax, %r10		
	adc	$0, %rbp			
Lmlo0:add	%r11, %r10			
	mov	%r10, 8(%rdi)		
	adc	$0, %rbp			
	.byte	0xc4,66,251,0xf6,209
	add	%rax, %rbx		
	mov	16(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,216
	adc	$0, %r10			
	add	%rax, %rbx		
	adc	$0, %r11			
Lmlo3:add	%rbp, %rbx			
	mov	%rbx, 16(%rdi)		
	adc	$0, %r11			
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r10		
	mov	24(%rsi), %rdx
	adc	$0, %rbx			
	.byte	0xc4,194,251,0xf6,232
	add	%rax, %r10		
	adc	$0, %rbp			
Lmlo2:add	%r11, %r10			
	lea	32(%rsi), %rsi
	mov	%r10, 24(%rdi)		
	adc	$0, %rbp			
	inc	%rcx
	lea	32(%rdi), %rdi
	jnz	Lmtop

Lmend:.byte	0xc4,194,235,0xf6,193
	add	%rdx, %rbx
	adc	$0, %rax
	add	%rbp, %rbx
	mov	%rbx, (%rdi)
	adc	$0, %rax
	mov	%rax, 8(%rdi)

	lea	16(%rsi), %rsi
	lea	-16(%rdi), %rdi

Ldo_addmul_2:
Louter:
	lea	(%rsi,%r12,8), %rsi		
	lea	48(%rdi,%r12,8), %rdi		

	mov	-8(%rsi), %r8		

	add	$2, %r12			
	cmp	$-2, %r12
	jge	Lcorner

	mov	(%rsi), %r9

	lea	1(%r12), %rcx
	sar	$2, %rcx			

	mov	%r9, %rdx
	test	$1, %r12b
	jnz	Lbx1

Lbx0:	mov	(%rdi), %r13
	mov	8(%rdi), %r14
	.byte	0xc4,66,251,0xf6,216
	add	%rax, %r13
	adc	$0, %r11
	mov	%r13, (%rdi)
	xor	%rbx, %rbx
	test	$2, %r12b
	jnz	Lb10

Lb00:	mov	8(%rsi), %rdx
	lea	16(%rdi), %rdi
	lea	16(%rsi), %rsi
	jmp	Llo0

Lb10:	mov	8(%rsi), %rdx
	mov	16(%rdi), %r13
	lea	32(%rsi), %rsi
	inc	%rcx
	.byte	0xc4,194,251,0xf6,232
	jz	Lex
	jmp	Llo2

Lbx1:	mov	(%rdi), %r14
	mov	8(%rdi), %r13
	.byte	0xc4,194,251,0xf6,232
	mov	8(%rsi), %rdx
	add	%rax, %r14
	adc	$0, %rbp
	xor	%r10, %r10
	mov	%r14, (%rdi)
	.byte	0xc4,66,251,0xf6,216
	test	$2, %r12b
	jz	Lb11

Lb01:	mov	16(%rdi), %r14
	lea	24(%rdi), %rdi
	lea	24(%rsi), %rsi
	jmp	Llo1

Lb11:	lea	8(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	Llo3

	.align	5, 0x90
Ltop:	.byte	0xc4,194,251,0xf6,232
	add	%r10, %r14
	adc	$0, %rbx
Llo2:	add	%rax, %r14
	adc	$0, %rbp
	.byte	0xc4,66,251,0xf6,209
	add	%rax, %r13
	adc	$0, %r10
	lea	32(%rdi), %rdi
	add	%r11, %r14
	mov	-16(%rsi), %rdx
	mov	%r14, -24(%rdi)
	adc	$0, %rbp
	add	%rbx, %r13
	mov	-8(%rdi), %r14
	.byte	0xc4,66,251,0xf6,216
	adc	$0, %r10
Llo1:	add	%rax, %r13
	.byte	0xc4,194,251,0xf6,217
	adc	$0, %r11
	add	%rbp, %r13
	mov	%r13, -16(%rdi)
	adc	$0, %r11
	add	%rax, %r14
	adc	$0, %rbx
	add	%r10, %r14
	mov	-8(%rsi), %rdx
	adc	$0, %rbx
Llo0:	.byte	0xc4,194,251,0xf6,232
	add	%rax, %r14
	adc	$0, %rbp
	mov	(%rdi), %r13
	.byte	0xc4,66,251,0xf6,209
	add	%rax, %r13
	adc	$0, %r10
	add	%r11, %r14
	mov	%r14, -8(%rdi)
	adc	$0, %rbp
	mov	(%rsi), %rdx
	add	%rbx, %r13
	.byte	0xc4,66,251,0xf6,216
	adc	$0, %r10
Llo3:	add	%rax, %r13
	adc	$0, %r11
	.byte	0xc4,194,251,0xf6,217
	add	%rbp, %r13
	mov	8(%rdi), %r14
	mov	%r13, (%rdi)
	mov	16(%rdi), %r13
	adc	$0, %r11
	add	%rax, %r14
	adc	$0, %rbx
	mov	8(%rsi), %rdx
	lea	32(%rsi), %rsi
	inc	%rcx
	jnz	Ltop

Lend:	.byte	0xc4,194,251,0xf6,232
	add	%r10, %r14
	adc	$0, %rbx
Lex:	add	%rax, %r14
	adc	$0, %rbp
	.byte	0xc4,194,235,0xf6,193
	add	%r11, %r14
	mov	%r14, 8(%rdi)
	adc	$0, %rbp
	add	%rbx, %rdx
	adc	$0, %rax
	add	%rdx, %rbp
	mov	%rbp, 16(%rdi)
	adc	$0, %rax
	mov	%rax, 24(%rdi)

	jmp	Louter		

Lcorner:
	pop	%r12
	mov	(%rsi), %rdx
	jg	Lsmall_corner

	mov	%rdx, %r9
	mov	(%rdi), %r13
	mov	%rax, %r14		
	.byte	0xc4,66,251,0xf6,216
	add	%rax, %r13
	adc	$0, %r11
	mov	%r13, (%rdi)
	mov	8(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,232
	add	%rax, %r14
	adc	$0, %rbp
	.byte	0xc4,194,235,0xf6,193
	add	%r11, %r14
	mov	%r14, 8(%rdi)
	adc	$0, %rbp
	add	%rbp, %rdx
	mov	%rdx, 16(%rdi)
	adc	$0, %rax
	mov	%rax, 24(%rdi)
	lea	32(%rdi), %rdi
	lea	16(%rsi), %rsi
	jmp	Lcom

Lsmall_corner:
	.byte	0xc4,194,139,0xf6,232
	add	%rax, %r14		
	adc	$0, %rbp
	mov	%r14, (%rdi)
	mov	%rbp, 8(%rdi)
	lea	16(%rdi), %rdi
	lea	8(%rsi), %rsi

Lcom:

Lsqr_diag_addlsh1:
	lea	8(%rsi,%r12,8), %rsi		
	lea	(%rdi,%r12,8), %rdi
	lea	(%rdi,%r12,8), %rdi		
	inc	%r12

	mov	-8(%rsi), %rdx
	xor	%ebx, %ebx	
	.byte	0xc4,98,251,0xf6,210
	mov	%rax, 8(%rdi)
	mov	16(%rdi), %r8
	mov	24(%rdi), %r9
	jmp	Ldm

	.align	4, 0x90
Ldtop:mov	32(%rdi), %r8
	mov	40(%rdi), %r9
	lea	16(%rdi), %rdi
	lea	(%rdx,%rbx), %r10
Ldm:	adc	%r8, %r8
	adc	%r9, %r9
	setc	%bl
	mov	(%rsi), %rdx
	lea	8(%rsi), %rsi
	.byte	0xc4,226,251,0xf6,210
	add	%r10, %r8
	adc	%rax, %r9
	mov	%r8, 16(%rdi)
	mov	%r9, 24(%rdi)
	inc	%r12
	jnz	Ldtop

Ldend:adc	%rbx, %rdx
	mov	%rdx, 32(%rdi)

	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	
