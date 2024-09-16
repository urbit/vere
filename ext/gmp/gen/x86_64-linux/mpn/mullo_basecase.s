



































































	.text
	.align	32, 0x90
	.globl	__gmpn_mullo_basecase
	.type	__gmpn_mullo_basecase,@function
	
__gmpn_mullo_basecase:

	
	cmp	$4, %ecx
	jae	.Lbig

	mov	%rdx, %r11
	mov	(%rsi), %rdx

	cmp	$2, %ecx
	jae	.Lgt1
.Ln1:	imul	(%r11), %rdx
	mov	%rdx, (%rdi)
	
	ret
.Lgt1:	ja	.Lgt2
.Ln2:	mov	(%r11), %r9
	.byte	0xc4,194,251,0xf6,209
	mov	%rax, (%rdi)
	mov	8(%rsi), %rax
	imul	%r9, %rax
	add	%rax, %rdx
	mov	8(%r11), %r9
	mov	(%rsi), %rcx
	imul	%r9, %rcx
	add	%rcx, %rdx
	mov	%rdx, 8(%rdi)
	
	ret
.Lgt2:
.Ln3:	mov	(%r11), %r9
	.byte	0xc4,66,251,0xf6,209	
	mov	%rax, (%rdi)
	mov	8(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,209	
	imul	16(%rsi), %r9		
	add	%rax, %r10
	adc	%rdx, %r9
	mov	8(%r11), %r8
	mov	(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,208	
	add	%rax, %r10
	adc	%rdx, %r9
	imul	8(%rsi), %r8		
	add	%r8, %r9
	mov	%r10, 8(%rdi)
	mov	16(%r11), %r10
	mov	(%rsi), %rax
	imul	%rax, %r10		
	add	%r10, %r9
	mov	%r9, 16(%rdi)
	
	ret

	.align	16, 0x90
.Lbig:	push	%r14
	push	%r12
	push	%rbx
	push	%rbp
	mov	-8(%rdx,%rcx,8), %r14	
	imul	(%rsi), %r14		
	lea	-3(%rcx), %ebp
	lea	8(%rdx), %r11
	mov	(%rdx), %rdx

	mov	%ecx, %eax
	shr	$3, %ecx
	and	$7, %eax		
	lea	.Lmtab(%rip), %r10
	movslq	(%r10,%rax,4), %rax
	lea	(%rax, %r10), %r10
	jmp	*%r10


.Lmf0:	.byte	0xc4,98,171,0xf6,6
	lea	56(%rsi), %rsi
	lea	-8(%rdi), %rdi
	lea	.Lf7(%rip), %rbx
	jmp	.Lmb0

.Lmf3:	.byte	0xc4,226,179,0xf6,6
	lea	16(%rsi), %rsi
	lea	16(%rdi), %rdi
	jrcxz	.Lmc
	inc	%ecx
	lea	.Lf2(%rip), %rbx
	jmp	.Lmb3

.Lmc:	.byte	0xc4,98,171,0xf6,70,248
	add	%rax, %r10
	mov	%r9, -16(%rdi)
	.byte	0xc4,226,179,0xf6,6
	mov	%r10, -8(%rdi)
	adc	%r8, %r9
	mov	%r9, (%rdi)
	jmp	.Lc2

.Lmf4:	.byte	0xc4,98,171,0xf6,6
	lea	24(%rsi), %rsi
	lea	24(%rdi), %rdi
	inc	%ecx
	lea	.Lf3(%rip), %rbx
	jmp	.Lmb4

.Lmf5:	.byte	0xc4,226,179,0xf6,6
	lea	32(%rsi), %rsi
	lea	32(%rdi), %rdi
	inc	%ecx
	lea	.Lf4(%rip), %rbx
	jmp	.Lmb5

.Lmf6:	.byte	0xc4,98,171,0xf6,6
	lea	40(%rsi), %rsi
	lea	40(%rdi), %rdi
	inc	%ecx
	lea	.Lf5(%rip), %rbx
	jmp	.Lmb6

.Lmf7:	.byte	0xc4,226,179,0xf6,6
	lea	48(%rsi), %rsi
	lea	48(%rdi), %rdi
	lea	.Lf6(%rip), %rbx
	jmp	.Lmb7

.Lmf1:	.byte	0xc4,226,179,0xf6,6
	lea	.Lf0(%rip), %rbx
	jmp	.Lmb1

.Lmf2:	.byte	0xc4,98,171,0xf6,6
	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	lea	.Lf1(%rip), %rbx
	.byte	0xc4,226,179,0xf6,6


	.align	32, 0x90
.Lmtop:mov	%r10, -8(%rdi)
	adc	%r8, %r9
.Lmb1:	.byte	0xc4,98,171,0xf6,70,8
	adc	%rax, %r10
	lea	64(%rsi), %rsi
	mov	%r9, (%rdi)
.Lmb0:	mov	%r10, 8(%rdi)
	.byte	0xc4,226,179,0xf6,70,208
	lea	64(%rdi), %rdi
	adc	%r8, %r9
.Lmb7:	.byte	0xc4,98,171,0xf6,70,216
	mov	%r9, -48(%rdi)
	adc	%rax, %r10
.Lmb6:	mov	%r10, -40(%rdi)
	.byte	0xc4,226,179,0xf6,70,224
	adc	%r8, %r9
.Lmb5:	.byte	0xc4,98,171,0xf6,70,232
	mov	%r9, -32(%rdi)
	adc	%rax, %r10
.Lmb4:	.byte	0xc4,226,179,0xf6,70,240
	mov	%r10, -24(%rdi)
	adc	%r8, %r9
.Lmb3:	.byte	0xc4,98,171,0xf6,70,248
	adc	%rax, %r10
	mov	%r9, -16(%rdi)
	dec	%ecx
	.byte	0xc4,226,179,0xf6,6
	jnz	.Lmtop

.Lmend:mov	%r10, -8(%rdi)
	adc	%r8, %r9
	mov	%r9, (%rdi)
	adc	%rcx, %rax

	lea	8(,%rbp,8), %r12
	neg	%r12
	shr	$3, %ebp
	jmp	.Lent

.Lf0:	.byte	0xc4,98,171,0xf6,6
	lea	-8(%rsi), %rsi
	lea	-8(%rdi), %rdi
	lea	.Lf7(%rip), %rbx
	jmp	.Lb0

.Lf1:	.byte	0xc4,226,179,0xf6,6
	lea	-1(%rbp), %ebp
	lea	.Lf0(%rip), %rbx
	jmp	.Lb1

.Lend:	.byte	0xf3,76,0x0f,0x38,0xf6,15
	mov	%r9, (%rdi)
	.byte	0xf3,72,0x0f,0x38,0xf6,193		
	adc	%rcx, %rax		
	lea	8(%r12), %r12
.Lent:	.byte	0xc4,98,171,0xf6,70,8	
	add	%rax, %r14
	add	%r10, %r14		
	lea	(%rsi,%r12), %rsi		
	lea	8(%rdi,%r12), %rdi		
	mov	(%r11), %rdx
	lea	8(%r11), %r11
	or	%ebp, %ecx		
	jmp	*%rbx

.Lf7:	.byte	0xc4,226,179,0xf6,6
	lea	-16(%rsi), %rsi
	lea	-16(%rdi), %rdi
	lea	.Lf6(%rip), %rbx
	jmp	.Lb7

.Lf2:	.byte	0xc4,98,171,0xf6,6
	lea	8(%rsi), %rsi
	lea	8(%rdi), %rdi
	.byte	0xc4,226,179,0xf6,6
	lea	.Lf1(%rip), %rbx


	.align	32, 0x90
.Ltop:	.byte	0xf3,76,0x0f,0x38,0xf6,87,248
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, -8(%rdi)
	jrcxz	.Lend
.Lb1:	.byte	0xc4,98,171,0xf6,70,8
	.byte	0xf3,76,0x0f,0x38,0xf6,15
	lea	-1(%rcx), %ecx
	mov	%r9, (%rdi)
	.byte	0x66,76,0x0f,0x38,0xf6,208
.Lb0:	.byte	0xc4,226,179,0xf6,70,16
	.byte	0x66,77,0x0f,0x38,0xf6,200
	.byte	0xf3,76,0x0f,0x38,0xf6,87,8
	mov	%r10, 8(%rdi)
.Lb7:	.byte	0xc4,98,171,0xf6,70,24
	lea	64(%rsi), %rsi
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xf3,76,0x0f,0x38,0xf6,79,16
	mov	%r9, 16(%rdi)
.Lb6:	.byte	0xc4,226,179,0xf6,70,224
	.byte	0xf3,76,0x0f,0x38,0xf6,87,24
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, 24(%rdi)
.Lb5:	.byte	0xc4,98,171,0xf6,70,232
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xf3,76,0x0f,0x38,0xf6,79,32
	mov	%r9, 32(%rdi)
.Lb4:	.byte	0xc4,226,179,0xf6,70,240
	.byte	0xf3,76,0x0f,0x38,0xf6,87,40
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, 40(%rdi)
.Lb3:	.byte	0xf3,76,0x0f,0x38,0xf6,79,48
	.byte	0xc4,98,171,0xf6,70,248
	mov	%r9, 48(%rdi)
	lea	64(%rdi), %rdi
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xc4,226,179,0xf6,6
	jmp	.Ltop

.Lf6:	.byte	0xc4,98,171,0xf6,6
	lea	40(%rsi), %rsi
	lea	-24(%rdi), %rdi
	lea	.Lf5(%rip), %rbx
	jmp	.Lb6

.Lf5:	.byte	0xc4,226,179,0xf6,6
	lea	32(%rsi), %rsi
	lea	-32(%rdi), %rdi
	lea	.Lf4(%rip), %rbx
	jmp	.Lb5

.Lf4:	.byte	0xc4,98,171,0xf6,6
	lea	24(%rsi), %rsi
	lea	-40(%rdi), %rdi
	lea	.Lf3(%rip), %rbx
	jmp	.Lb4

.Lf3:	.byte	0xc4,226,179,0xf6,6
	lea	16(%rsi), %rsi
	lea	-48(%rdi), %rdi
	jrcxz	.Lcor
	lea	.Lf2(%rip), %rbx
	jmp	.Lb3

.Lcor:	.byte	0xf3,76,0x0f,0x38,0xf6,79,48
	.byte	0xc4,98,171,0xf6,70,248
	mov	%r9, 48(%rdi)
	lea	64(%rdi), %rdi
	.byte	0x66,76,0x0f,0x38,0xf6,208
	.byte	0xc4,226,179,0xf6,6
	.byte	0xf3,76,0x0f,0x38,0xf6,87,248
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, -8(%rdi)		
	.byte	0xf3,76,0x0f,0x38,0xf6,15
	mov	%r9, (%rdi)		
	.byte	0xf3,72,0x0f,0x38,0xf6,193
.Lc2:
	.byte	0xc4,98,171,0xf6,70,8
	adc	%rax, %r14
	add	%r10, %r14
	mov	(%r11), %rdx
	test	%ecx, %ecx
	.byte	0xc4,98,171,0xf6,70,240
	.byte	0xc4,226,179,0xf6,70,248
	.byte	0xf3,76,0x0f,0x38,0xf6,87,248
	.byte	0x66,77,0x0f,0x38,0xf6,200
	mov	%r10, -8(%rdi)
	.byte	0xf3,76,0x0f,0x38,0xf6,15
	.byte	0xf3,72,0x0f,0x38,0xf6,193
	adc	%rcx, %rax
	.byte	0xc4,98,171,0xf6,6
	add	%rax, %r14
	add	%r10, %r14
	mov	8(%r11), %rdx
	.byte	0xc4,226,243,0xf6,70,240
	add	%r9, %rcx
	mov	%rcx, (%rdi)
	adc	$0, %rax
	.byte	0xc4,98,171,0xf6,70,248
	add	%rax, %r14
	add	%r10, %r14
	mov	%r14, 8(%rdi)
	pop	%rbp
	pop	%rbx
	pop	%r12
	pop	%r14
	
	ret
	.size	__gmpn_mullo_basecase,.-__gmpn_mullo_basecase
	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Lmtab:.long	.Lmf7-.Lmtab
	.long	.Lmf0-.Lmtab
	.long	.Lmf1-.Lmtab
	.long	.Lmf2-.Lmtab
	.long	.Lmf3-.Lmtab
	.long	.Lmf4-.Lmtab
	.long	.Lmf5-.Lmtab
	.long	.Lmf6-.Lmtab
