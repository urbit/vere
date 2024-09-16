






























































































	.text
	.align	16, 0x90
	.globl	__gmpn_mul_basecase
	.type	__gmpn_mul_basecase,@function
	
__gmpn_mul_basecase:

	


	cmp	$2, %rdx
	ja	.Lgen
	mov	(%rcx), %rdx
	.byte	0xc4,98,251,0xf6,14	
	je	.Ls2x

.Ls11:	mov	%rax, (%rdi)
	mov	%r9, 8(%rdi)
	
	ret

.Ls2x:	cmp	$2, %r8
	.byte	0xc4,98,187,0xf6,86,8	
	je	.Ls22

.Ls21:	add	%r8, %r9
	adc	$0, %r10
	mov	%rax, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	
	ret

.Ls22:	add	%r8, %r9		
	adc	$0, %r10		
	mov	8(%rcx), %rdx
	mov	%rax, (%rdi)
	.byte	0xc4,98,187,0xf6,30	
	.byte	0xc4,226,251,0xf6,86,8	
	add	%r11, %rax		
	adc	$0, %rdx		
	add	%r8, %r9		
	adc	%rax, %r10		
	adc	$0, %rdx		
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	mov	%rdx, 24(%rdi)
	
	ret

	.align	16, 0x90
.Lgen:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r14

	mov	%rcx, %r14
	lea	1(%rdx), %rbx
	mov	%rdx, %rbp
	mov	%edx, %eax
	and	$-8, %rbx
	shr	$3, %rbp		
	neg	%rbx
	and	$7, %eax		
					
	mov	%rbp, %rcx
	mov	(%r14), %rdx
	lea	8(%r14), %r14

	lea	.Lmtab(%rip), %r10
	movslq	(%r10,%rax,4), %r11
	lea	(%r11, %r10), %r10
	jmp	*%r10


.Lmf0:	.byte	0xc4,98,171,0xf6,30
	lea	56(%rsi), %rsi
	lea	-8(%rdi), %rdi
	jmp	.Lmb0

.Lmf3:	.byte	0xc4,98,155,0xf6,14
	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi
	inc	%rcx
	jmp	.Lmb3

.Lmf4:	.byte	0xc4,98,171,0xf6,30
	lea	24(%rsi), %rsi
	lea	24(%rdi), %rdi
	inc	%rcx
	jmp	.Lmb4

.Lmf5:	.byte	0xc4,98,155,0xf6,14
	lea	32(%rsi), %rsi
	lea	32(%rdi), %rdi
	inc	%rcx
	jmp	.Lmb5

.Lmf6:	.byte	0xc4,98,171,0xf6,30
	lea	40(%rsi), %rsi
	lea	40(%rdi), %rdi
	inc	%rcx
	jmp	.Lmb6

.Lmf7:	.byte	0xc4,98,155,0xf6,14
	lea	48(%rsi), %rsi
	lea	48(%rdi), %rdi
	inc	%rcx
	jmp	.Lmb7

.Lmf1:	.byte	0xc4,98,155,0xf6,14
	jmp	.Lmb1

.Lmf2:	.byte	0xc4,98,171,0xf6,30
	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	.byte	0xc4,98,155,0xf6,14

	.align	16, 0x90
.Lm1top:
	mov	%r10, -8(%rdi)
	adc	%r11, %r12
.Lmb1:	.byte	0xc4,98,171,0xf6,94,8
	adc	%r9, %r10
	lea	64(%rsi), %rsi
	mov	%r12, (%rdi)
.Lmb0:	mov	%r10, 8(%rdi)
	.byte	0xc4,98,155,0xf6,78,208
	lea	64(%rdi), %rdi
	adc	%r11, %r12
.Lmb7:	.byte	0xc4,98,171,0xf6,94,216
	mov	%r12, -48(%rdi)
	adc	%r9, %r10
.Lmb6:	mov	%r10, -40(%rdi)
	.byte	0xc4,98,155,0xf6,78,224
	adc	%r11, %r12
.Lmb5:	.byte	0xc4,98,171,0xf6,94,232
	mov	%r12, -32(%rdi)
	adc	%r9, %r10
.Lmb4:	.byte	0xc4,98,155,0xf6,78,240
	mov	%r10, -24(%rdi)
	adc	%r11, %r12
.Lmb3:	.byte	0xc4,98,171,0xf6,94,248
	adc	%r9, %r10
	mov	%r12, -16(%rdi)
	dec	%rcx
	.byte	0xc4,98,155,0xf6,14
	jnz	.Lm1top

.Lm1end:
	mov	%r10, -8(%rdi)
	adc	%r11, %r12
	mov	%r12, (%rdi)
	adc	%rcx, %r9		
	mov	%r9, 8(%rdi)

	dec	%r8
	jz	.Ldone

	lea	.Latab(%rip), %r10
	movslq	(%r10,%rax,4), %rax
	lea	(%rax, %r10), %rax


.Louter:
	lea	(%rsi,%rbx,8), %rsi
	mov	%rbp, %rcx
	mov	(%r14), %rdx
	lea	8(%r14), %r14
	jmp	*%rax

.Lf0:	.byte	0xc4,98,171,0xf6,94,8
	lea	8(%rdi,%rbx,8), %rdi
	lea	-1(%rcx), %rcx
	jmp	.Lb0

.Lf3:	.byte	0xc4,98,155,0xf6,78,240
	lea	-56(%rdi,%rbx,8), %rdi
	jmp	.Lb3

.Lf4:	.byte	0xc4,98,171,0xf6,94,232
	lea	-56(%rdi,%rbx,8), %rdi
	jmp	.Lb4

.Lf5:	.byte	0xc4,98,155,0xf6,78,224
	lea	-56(%rdi,%rbx,8), %rdi
	jmp	.Lb5

.Lf6:	.byte	0xc4,98,171,0xf6,94,216
	lea	-56(%rdi,%rbx,8), %rdi
	jmp	.Lb6

.Lf7:	.byte	0xc4,98,155,0xf6,78,16
	lea	8(%rdi,%rbx,8), %rdi
	jmp	.Lb7

.Lf1:	.byte	0xc4,98,155,0xf6,14
	lea	8(%rdi,%rbx,8), %rdi
	jmp	.Lb1

.Lam1end:
	.byte	0xf3,76,0x0f,0x38,0xf6,39
	.byte	0xf3,76,0x0f,0x38,0xf6,201		
	mov	%r12, (%rdi)
	adc	%rcx, %r9		
	mov	%r9, 8(%rdi)

	dec	%r8			
	jnz	.Louter
.Ldone:
	pop	%r14
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret

.Lf2:
	.byte	0xc4,98,171,0xf6,94,248
	lea	8(%rdi,%rbx,8), %rdi
	.byte	0xc4,98,155,0xf6,14

	.align	16, 0x90
.Lam1top:
	.byte	0xf3,76,0x0f,0x38,0xf6,87,248
	.byte	0x66,77,0x0f,0x38,0xf6,227
	mov	%r10, -8(%rdi)
	jrcxz	.Lam1end
.Lb1:	.byte	0xc4,98,171,0xf6,94,8
	.byte	0xf3,76,0x0f,0x38,0xf6,39
	lea	-1(%rcx), %rcx
	mov	%r12, (%rdi)
	.byte	0x66,77,0x0f,0x38,0xf6,209
.Lb0:	.byte	0xc4,98,155,0xf6,78,16
	.byte	0x66,77,0x0f,0x38,0xf6,227
	.byte	0xf3,76,0x0f,0x38,0xf6,87,8
	mov	%r10, 8(%rdi)
.Lb7:	.byte	0xc4,98,171,0xf6,94,24
	lea	64(%rsi), %rsi
	.byte	0x66,77,0x0f,0x38,0xf6,209
	.byte	0xf3,76,0x0f,0x38,0xf6,103,16
	mov	%r12, 16(%rdi)
.Lb6:	.byte	0xc4,98,155,0xf6,78,224
	.byte	0xf3,76,0x0f,0x38,0xf6,87,24
	.byte	0x66,77,0x0f,0x38,0xf6,227
	mov	%r10, 24(%rdi)
.Lb5:	.byte	0xc4,98,171,0xf6,94,232
	.byte	0x66,77,0x0f,0x38,0xf6,209
	.byte	0xf3,76,0x0f,0x38,0xf6,103,32
	mov	%r12, 32(%rdi)
.Lb4:	.byte	0xc4,98,155,0xf6,78,240
	.byte	0xf3,76,0x0f,0x38,0xf6,87,40
	.byte	0x66,77,0x0f,0x38,0xf6,227
	mov	%r10, 40(%rdi)
.Lb3:	.byte	0xf3,76,0x0f,0x38,0xf6,103,48
	.byte	0xc4,98,171,0xf6,94,248
	mov	%r12, 48(%rdi)
	lea	64(%rdi), %rdi
	.byte	0x66,77,0x0f,0x38,0xf6,209
	.byte	0xc4,98,155,0xf6,14
	jmp	.Lam1top

	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Lmtab:.long	.Lmf0-.Lmtab
	.long	.Lmf1-.Lmtab
	.long	.Lmf2-.Lmtab
	.long	.Lmf3-.Lmtab
	.long	.Lmf4-.Lmtab
	.long	.Lmf5-.Lmtab
	.long	.Lmf6-.Lmtab
	.long	.Lmf7-.Lmtab
.Latab:.long	.Lf0-.Latab
	.long	.Lf1-.Latab
	.long	.Lf2-.Latab
	.long	.Lf3-.Latab
	.long	.Lf4-.Latab
	.long	.Lf5-.Latab
	.long	.Lf6-.Latab
	.long	.Lf7-.Latab
	.text
	.size	__gmpn_mul_basecase,.-__gmpn_mul_basecase
