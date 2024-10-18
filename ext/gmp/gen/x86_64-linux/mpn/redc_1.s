










































































   
   
   
   
    











	.text
	.align	32, 0x90
	.globl	__gmpn_redc_1
	.type	__gmpn_redc_1,@function
	
__gmpn_redc_1:

	

	push	%rbp
	mov	(%rsi), %rbp		
	push	%rbx
	imul	%r8, %rbp		
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	mov	%rcx, %r12
	neg	%r12
	lea	(%rdx,%rcx,8), %r13	
	lea	-16(%rsi,%rcx,8), %rsi		

	mov	%ecx, %eax
	and	$3, %eax
	lea	4(%rax), %r9
	cmp	$4, %ecx
	cmovg	%r9, %rax
	lea	.Ltab(%rip), %r9

	movslq	(%r9,%rax,4), %rax
	add	%r9, %rax
	jmp	*%rax


	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Ltab:	.long	.L0-.Ltab
	.long	.L1-.Ltab
	.long	.L2-.Ltab
	.long	.L3-.Ltab
	.long	.L0m4-.Ltab
	.long	.L1m4-.Ltab
	.long	.L2m4-.Ltab
	.long	.L3m4-.Ltab
	.text

	.align	16, 0x90
.L1:	mov	(%rdx), %rax
	mul	%rbp
	add	8(%rsi), %rax
	adc	16(%rsi), %rdx
	mov	%rdx, (%rdi)
	mov	$0, %eax
	adc	%eax, %eax
	jmp	.Lret


	.align	16, 0x90
.L2:	mov	(%rdx), %rax
	mul	%rbp
	xor	%r14d, %r14d
	mov	%rax, %r10
	mov	-8(%r13), %rax
	mov	%rdx, %r9
	mul	%rbp
	add	(%rsi), %r10
	adc	%rax, %r9
	adc	%rdx, %r14
	add	8(%rsi), %r9
	adc	$0, %r14
	mov	%r9, %rbp
	imul	%r8, %rbp
	mov	-16(%r13), %rax
	mul	%rbp
	xor	%ebx, %ebx
	mov	%rax, %r10
	mov	-8(%r13), %rax
	mov	%rdx, %r11
	mul	%rbp
	add	%r9, %r10
	adc	%rax, %r11
	adc	%rdx, %rbx
	add	16(%rsi), %r11
	adc	$0, %rbx
	xor	%eax, %eax
	add	%r11, %r14
	adc	24(%rsi), %rbx
	mov	%r14, (%rdi)
	mov	%rbx, 8(%rdi)
	adc	%eax, %eax
	jmp	.Lret


.L3:	mov	(%rdx), %rax
	mul	%rbp
	mov	%rax, %rbx
	mov	%rdx, %r10
	mov	-16(%r13), %rax
	mul	%rbp
	xor	%r9d, %r9d
	xor	%r14d, %r14d
	add	-8(%rsi), %rbx
	adc	%rax, %r10
	mov	-8(%r13), %rax
	adc	%rdx, %r9
	mul	%rbp
	add	(%rsi), %r10
	mov	%r10, (%rsi)
	adc	%rax, %r9
	adc	%rdx, %r14
	mov	%r10, %rbp
	imul	%r8, %rbp
	add	%r9, 8(%rsi)
	adc	$0, %r14
	mov	%r14, -8(%rsi)

	mov	-24(%r13), %rax
	mul	%rbp
	mov	%rax, %rbx
	mov	%rdx, %r10
	mov	-16(%r13), %rax
	mul	%rbp
	xor	%r9d, %r9d
	xor	%r14d, %r14d
	add	(%rsi), %rbx
	adc	%rax, %r10
	mov	-8(%r13), %rax
	adc	%rdx, %r9
	mul	%rbp
	add	8(%rsi), %r10
	mov	%r10, 8(%rsi)
	adc	%rax, %r9
	adc	%rdx, %r14
	mov	%r10, %rbp
	imul	%r8, %rbp
	add	%r9, 16(%rsi)
	adc	$0, %r14
	mov	%r14, (%rsi)

	mov	-24(%r13), %rax
	mul	%rbp
	mov	%rax, %rbx
	mov	%rdx, %r10
	mov	-16(%r13), %rax
	mul	%rbp
	xor	%r9d, %r9d
	xor	%r14d, %r14d
	add	8(%rsi), %rbx
	adc	%rax, %r10
	mov	-8(%r13), %rax
	adc	%rdx, %r9
	mul	%rbp
	add	16(%rsi), %r10
	adc	%rax, %r9
	adc	%rdx, %r14
	add	24(%rsi), %r9
	adc	$0, %r14

	xor	%eax, %eax
	add	-8(%rsi), %r10
	adc	(%rsi), %r9
	adc	32(%rsi), %r14
	mov	%r10, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r14, 16(%rdi)
	adc	%eax, %eax
	jmp	.Lret


	.align	16, 0x90
.L2m4:
.Llo2:	mov	(%r13,%r12,8), %rax
	mul	%rbp
	xor	%r14d, %r14d
	xor	%ebx, %ebx
	mov	%rax, %r10
	mov	8(%r13,%r12,8), %rax
	mov	24(%rsi,%r12,8), %r15
	mov	%rdx, %r9
	mul	%rbp
	add	16(%rsi,%r12,8), %r10
	adc	%rax, %r9
	mov	16(%r13,%r12,8), %rax
	adc	%rdx, %r14
	mul	%rbp
	mov	$0, %r10d		
	lea	2(%r12), %r11
	add	%r9, %r15
	imul	%r8, %r15
	jmp	 .Le2

	.align	16, 0x90
.Lli2:	add	%r10, (%rsi,%r11,8)
	adc	%rax, %r9
	mov	(%r13,%r11,8), %rax
	adc	%rdx, %r14
	xor	%r10d, %r10d
	mul	%rbp
.Le2:	add	%r9, 8(%rsi,%r11,8)
	adc	%rax, %r14
	adc	%rdx, %rbx
	mov	8(%r13,%r11,8), %rax
	mul	%rbp
	add	%r14, 16(%rsi,%r11,8)
	adc	%rax, %rbx
	adc	%rdx, %r10
	mov	16(%r13,%r11,8), %rax
	mul	%rbp
	add	%rbx, 24(%rsi,%r11,8)
	mov	$0, %r14d		
	mov	%r14, %rbx		
	adc	%rax, %r10
	mov	24(%r13,%r11,8), %rax
	mov	%r14, %r9		
	adc	%rdx, %r9
	mul	%rbp
	add	$4, %r11
	js	 .Lli2

.Lle2:	add	%r10, (%rsi)
	adc	%rax, %r9
	adc	%r14, %rdx
	add	%r9, 8(%rsi)
	adc	$0, %rdx
	mov	%rdx, 16(%rsi,%r12,8)	
	add	$8, %rsi
	mov	%r15, %rbp
	dec	%rcx
	jnz	.Llo2

	mov	%r12, %rcx
	sar	$2, %rcx
	lea	32(%rsi,%r12,8), %rsi
	lea	(%rsi,%r12,8), %rdx

	mov	-16(%rsi), %r8
	mov	-8(%rsi), %r9
	add	-16(%rdx), %r8
	adc	-8(%rdx), %r9
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	lea	16(%rdi), %rdi
	jmp	.Laddx


	.align	16, 0x90
.L1m4:
.Llo1:	mov	(%r13,%r12,8), %rax
	xor	%r9, %r9
	xor	%ebx, %ebx
	mul	%rbp
	mov	%rax, %r9
	mov	8(%r13,%r12,8), %rax
	mov	24(%rsi,%r12,8), %r15
	mov	%rdx, %r14
	mov	$0, %r10d		
	mul	%rbp
	add	16(%rsi,%r12,8), %r9
	adc	%rax, %r14
	adc	%rdx, %rbx
	mov	16(%r13,%r12,8), %rax
	mul	%rbp
	lea	1(%r12), %r11
	add	%r14, %r15
	imul	%r8, %r15
	jmp	 .Le1

	.align	16, 0x90
.Lli1:	add	%r10, (%rsi,%r11,8)
	adc	%rax, %r9
	mov	(%r13,%r11,8), %rax
	adc	%rdx, %r14
	xor	%r10d, %r10d
	mul	%rbp
	add	%r9, 8(%rsi,%r11,8)
	adc	%rax, %r14
	adc	%rdx, %rbx
	mov	8(%r13,%r11,8), %rax
	mul	%rbp
.Le1:	add	%r14, 16(%rsi,%r11,8)
	adc	%rax, %rbx
	adc	%rdx, %r10
	mov	16(%r13,%r11,8), %rax
	mul	%rbp
	add	%rbx, 24(%rsi,%r11,8)
	mov	$0, %r14d		
	mov	%r14, %rbx		
	adc	%rax, %r10
	mov	24(%r13,%r11,8), %rax
	mov	%r14, %r9		
	adc	%rdx, %r9
	mul	%rbp
	add	$4, %r11
	js	 .Lli1

.Lle1:	add	%r10, (%rsi)
	adc	%rax, %r9
	adc	%r14, %rdx
	add	%r9, 8(%rsi)
	adc	$0, %rdx
	mov	%rdx, 16(%rsi,%r12,8)	
	add	$8, %rsi
	mov	%r15, %rbp
	dec	%rcx
	jnz	.Llo1

	mov	%r12, %rcx
	sar	$2, %rcx
	lea	24(%rsi,%r12,8), %rsi
	lea	(%rsi,%r12,8), %rdx

	mov	-8(%rsi), %r8
	add	-8(%rdx), %r8
	mov	%r8, (%rdi)
	lea	8(%rdi), %rdi
	jmp	.Laddx


	.align	16, 0x90
.L0:
.L0m4:
.Llo0:	mov	(%r13,%r12,8), %rax
	mov	%r12, %r11
	mul	%rbp
	xor	%r10d, %r10d
	mov	%rax, %r14
	mov	%rdx, %rbx
	mov	8(%r13,%r12,8), %rax
	mov	24(%rsi,%r12,8), %r15
	mul	%rbp
	add	16(%rsi,%r12,8), %r14
	adc	%rax, %rbx
	adc	%rdx, %r10
	add	%rbx, %r15
	imul	%r8, %r15
	jmp	.Le0

	.align	16, 0x90
.Lli0:	add	%r10, (%rsi,%r11,8)
	adc	%rax, %r9
	mov	(%r13,%r11,8), %rax
	adc	%rdx, %r14
	xor	%r10d, %r10d
	mul	%rbp
	add	%r9, 8(%rsi,%r11,8)
	adc	%rax, %r14
	adc	%rdx, %rbx
	mov	8(%r13,%r11,8), %rax
	mul	%rbp
	add	%r14, 16(%rsi,%r11,8)
	adc	%rax, %rbx
	adc	%rdx, %r10
.Le0:	mov	16(%r13,%r11,8), %rax
	mul	%rbp
	add	%rbx, 24(%rsi,%r11,8)
	mov	$0, %r14d		
	mov	%r14, %rbx		
	adc	%rax, %r10
	mov	24(%r13,%r11,8), %rax
	mov	%r14, %r9		
	adc	%rdx, %r9
	mul	%rbp
	add	$4, %r11
	js	 .Lli0

.Lle0:	add	%r10, (%rsi)
	adc	%rax, %r9
	adc	%r14, %rdx
	add	%r9, 8(%rsi)
	adc	$0, %rdx
	mov	%rdx, 16(%rsi,%r12,8)	
	add	$8, %rsi
	mov	%r15, %rbp
	dec	%rcx
	jnz	.Llo0

	mov	%r12, %rcx
	sar	$2, %rcx
	clc
	lea	16(%rsi,%r12,8), %rsi
	lea	(%rsi,%r12,8), %rdx
	jmp	.Laddy


	.align	16, 0x90
.L3m4:
.Llo3:	mov	(%r13,%r12,8), %rax
	mul	%rbp
	mov	%rax, %rbx
	mov	%rdx, %r10
	mov	8(%r13,%r12,8), %rax
	mov	24(%rsi,%r12,8), %r15
	mul	%rbp
	add	16(%rsi,%r12,8), %rbx	
	mov	$0, %ebx		
	mov	%rbx, %r14		
	adc	%rax, %r10
	mov	16(%r13,%r12,8), %rax
	mov	%r14, %r9		
	adc	%rdx, %r9
	add	%r10, %r15
	mul	%rbp
	lea	3(%r12), %r11
	imul	%r8, %r15


	.align	16, 0x90
.Lli3:	add	%r10, (%rsi,%r11,8)
	adc	%rax, %r9
	mov	(%r13,%r11,8), %rax
	adc	%rdx, %r14
	xor	%r10d, %r10d
	mul	%rbp
	add	%r9, 8(%rsi,%r11,8)
	adc	%rax, %r14
	adc	%rdx, %rbx
	mov	8(%r13,%r11,8), %rax
	mul	%rbp
	add	%r14, 16(%rsi,%r11,8)
	adc	%rax, %rbx
	adc	%rdx, %r10
	mov	16(%r13,%r11,8), %rax
	mul	%rbp
	add	%rbx, 24(%rsi,%r11,8)
	mov	$0, %r14d		
	mov	%r14, %rbx		
	adc	%rax, %r10
	mov	24(%r13,%r11,8), %rax
	mov	%r14, %r9		
	adc	%rdx, %r9
	mul	%rbp
	add	$4, %r11
	js	 .Lli3

.Lle3:	add	%r10, (%rsi)
	adc	%rax, %r9
	adc	%r14, %rdx
	add	%r9, 8(%rsi)
	adc	$0, %rdx
	mov	%rdx, 16(%rsi,%r12,8)	
	mov	%r15, %rbp
	lea	8(%rsi), %rsi
	dec	%rcx
	jnz	.Llo3



	mov	%r12, %rcx
	sar	$2, %rcx
	lea	40(%rsi,%r12,8), %rsi
	lea	(%rsi,%r12,8), %rdx

	mov	-24(%rsi), %r8
	mov	-16(%rsi), %r9
	mov	-8(%rsi), %r10
	add	-24(%rdx), %r8
	adc	-16(%rdx), %r9
	adc	-8(%rdx), %r10
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	lea	24(%rdi), %rdi

.Laddx:inc	%rcx
	jz	.Lad3

.Laddy:mov	(%rsi), %r8
	mov	8(%rsi), %r9
	inc	%rcx
	jmp	.Lmid


.Lal3:	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	adc	24(%rdx), %r11
	mov	%r8, (%rdi)
	lea	32(%rsi), %rsi
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	inc	%rcx
	mov	%r11, 24(%rdi)
	lea	32(%rdx), %rdx
	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	lea	32(%rdi), %rdi
.Lmid:	mov	16(%rsi), %r10
	mov	24(%rsi), %r11
	jnz	.Lal3

.Lae3:	adc	(%rdx), %r8
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	adc	24(%rdx), %r11
	mov	%r8, (%rdi)
	mov	%r9, 8(%rdi)
	mov	%r10, 16(%rdi)
	mov	%r11, 24(%rdi)

.Lad3:	mov	%ecx, %eax	
	adc	%eax, %eax

.Lret:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	
	ret
	.size	__gmpn_redc_1,.-__gmpn_redc_1
