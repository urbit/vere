













































































	.text
	.align	16, 0x90
	.globl	__gmpn_mullo_basecase
	.type	__gmpn_mullo_basecase,@function
	
__gmpn_mullo_basecase:

	
	cmp	$4, %rcx
	jge	.Lgen
	mov	(%rsi), %rax		
	mov	(%rdx), %r8		

	lea	.Ltab(%rip), %r9
	movslq	(%r9,%rcx,4), %r10
	add	%r10, %r9
	jmp	*%r9

	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Ltab:	.long	.Ltab-.Ltab			
	.long	.L1-.Ltab			
	.long	.L2-.Ltab			
	.long	.L3-.Ltab			
	.text

.L1:	imul	%r8, %rax
	mov	%rax, (%rdi)
	
	ret

.L2:	mov	8(%rdx), %r11
	imul	%rax, %r11		
	mul	%r8			
	mov	%rax, (%rdi)
	imul	8(%rsi), %r8		
	lea	(%r11, %rdx), %rax
	add	%r8, %rax
	mov	%rax, 8(%rdi)
	
	ret

.L3:	mov	8(%rdx), %r9	
	mov	16(%rdx), %r11
	mul	%r8			
	mov	%rax, (%rdi)		
	mov	(%rsi), %rax		
	mov	%rdx, %rcx		
	mul	%r9			
	imul	8(%rsi), %r9		
	mov	16(%rsi), %r10
	imul	%r8, %r10		
	add	%rax, %rcx
	adc	%rdx, %r9
	add	%r10, %r9
	mov	8(%rsi), %rax		
	mul	%r8			
	add	%rax, %rcx
	adc	%rdx, %r9
	mov	%r11, %rax
	imul	(%rsi), %rax		
	add	%rax, %r9
	mov	%rcx, 8(%rdi)
	mov	%r9, 16(%rdi)
	
	ret

.L0m4:
.L1m4:
.L2m4:
.L3m4:
.Lgen:	push	%rbx
	push	%rbp
	push	%r13
	push	%r14
	push	%r15

	mov	(%rsi), %rax
	mov	(%rdx), %r13
	mov	%rdx, %r11

	lea	(%rdi,%rcx,8), %rdi
	lea	(%rsi,%rcx,8), %rsi
	neg	%rcx

	mul	%r13

	test	$1, %cl
	jz	.Lmul_2

.Lmul_1:
	lea	-8(%rdi), %rdi
	lea	-8(%rsi), %rsi
	test	$2, %cl
	jnz	.Lmul_1_prologue_3

.Lmul_1_prologue_2:		
	lea	-1(%rcx), %r9
	lea	.Laddmul_outer_1(%rip), %r8
	mov	%rax, %rbx
	mov	%rdx, %r15
	xor	%ebp, %ebp
	xor	%r10d, %r10d
	mov	16(%rsi,%rcx,8), %rax
	jmp	.Lmul_1_entry_2

.Lmul_1_prologue_3:		
	lea	1(%rcx), %r9
	lea	.Laddmul_outer_3(%rip), %r8
	mov	%rax, %rbp
	mov	%rdx, %r10
	xor	%ebx, %ebx
	jmp	.Lmul_1_entry_0

	.align	16, 0x90
.Lmul_1_top:
	mov	%rbx, -16(%rdi,%r9,8)
	add	%rax, %r15
	mov	(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	xor	%ebx, %ebx
	mul	%r13
	mov	%r15, -8(%rdi,%r9,8)
	add	%rax, %rbp
	adc	%rdx, %r10
.Lmul_1_entry_0:
	mov	8(%rsi,%r9,8), %rax
	mul	%r13
	mov	%rbp, (%rdi,%r9,8)
	add	%rax, %r10
	adc	%rdx, %rbx
	mov	16(%rsi,%r9,8), %rax
	mul	%r13
	mov	%r10, 8(%rdi,%r9,8)
	xor	%ebp, %ebp	
	mov	%rbp, %r10			
	add	%rax, %rbx
	mov	24(%rsi,%r9,8), %rax
	mov	%rbp, %r15			
	adc	%rdx, %r15
.Lmul_1_entry_2:
	mul	%r13
	add	$4, %r9
	js	.Lmul_1_top

	mov	%rbx, -16(%rdi)
	add	%rax, %r15
	mov	%r15, -8(%rdi)
	adc	%rdx, %rbp

	imul	(%rsi), %r13
	add	%r13, %rbp
	mov	%rbp, (%rdi)

	add	$1, %rcx
	jz	.Lret

	mov	8(%r11), %r13
	mov	16(%r11), %r14

	lea	16(%rsi), %rsi
	lea	8(%r11), %r11
	lea	24(%rdi), %rdi

	jmp	*%r8


.Lmul_2:
	mov	8(%r11), %r14
	test	$2, %cl
	jz	.Lmul_2_prologue_3

	.align	16, 0x90
.Lmul_2_prologue_1:
	lea	0(%rcx), %r9
	mov	%rax, %r10
	mov	%rdx, %rbx
	xor	%r15d, %r15d
	mov	(%rsi,%rcx,8), %rax
	lea	.Laddmul_outer_3(%rip), %r8
	jmp	.Lmul_2_entry_1

	.align	16, 0x90
.Lmul_2_prologue_3:
	lea	2(%rcx), %r9
	mov	$0, %r10d
	mov	%rax, %r15
	mov	(%rsi,%rcx,8), %rax
	mov	%rdx, %rbp
	lea	.Laddmul_outer_1(%rip), %r8
	jmp	.Lmul_2_entry_3

	.align	16, 0x90
.Lmul_2_top:
	mov	-32(%rsi,%r9,8), %rax
	mul	%r14
	add	%rax, %rbx
	adc	%rdx, %r15
	mov	-24(%rsi,%r9,8), %rax
	xor	%ebp, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	-24(%rsi,%r9,8), %rax
	adc	%rdx, %r15
	adc	$0, %ebp
	mul	%r14
	add	%rax, %r15
	mov	%rbx, -24(%rdi,%r9,8)
	adc	%rdx, %rbp
	mov	-16(%rsi,%r9,8), %rax
	mul	%r13
	mov	$0, %r10d
	add	%rax, %r15
	adc	%rdx, %rbp
	mov	-16(%rsi,%r9,8), %rax
	adc	$0, %r10d
.Lmul_2_entry_3:
	mov	$0, %ebx
	mov	%r15, -16(%rdi,%r9,8)
	mul	%r14
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mov	$0, %r15d
	mul	%r13
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	adc	%r15d, %ebx
	mul	%r14
	add	%rax, %r10
	mov	%rbp, -8(%rdi,%r9,8)
	adc	%rdx, %rbx
	mov	(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	adc	%rdx, %rbx
	adc	$0, %r15d
.Lmul_2_entry_1:
	add	$4, %r9
	mov	%r10, -32(%rdi,%r9,8)
	js	.Lmul_2_top

	imul	-16(%rsi), %r14
	add	%r14, %rbx
	imul	-8(%rsi), %r13
	add	%r13, %rbx
	mov	%rbx, -8(%rdi)

	add	$2, %rcx
	jz	.Lret

	mov	16(%r11), %r13
	mov	24(%r11), %r14

	lea	16(%r11), %r11
	lea	16(%rdi), %rdi

	jmp	*%r8


.Laddmul_outer_1:
	lea	-2(%rcx), %r9
	mov	-16(%rsi,%rcx,8), %rax
	mul	%r13
	mov	%rax, %r10
	mov	-16(%rsi,%rcx,8), %rax
	mov	%rdx, %rbx
	xor	%r15d, %r15d
	lea	.Laddmul_outer_3(%rip), %r8
	jmp	.Laddmul_entry_1

.Laddmul_outer_3:
	lea	0(%rcx), %r9
	mov	-16(%rsi,%rcx,8), %rax
	xor	%r10d, %r10d
	mul	%r13
	mov	%rax, %r15
	mov	-16(%rsi,%rcx,8), %rax
	mov	%rdx, %rbp
	lea	.Laddmul_outer_1(%rip), %r8
	jmp	.Laddmul_entry_3

	.align	16, 0x90
.Laddmul_top:
	add	%r10, -32(%rdi,%r9,8)
	adc	%rax, %rbx
	mov	-24(%rsi,%r9,8), %rax
	adc	%rdx, %r15
	xor	%ebp, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	-24(%rsi,%r9,8), %rax
	adc	%rdx, %r15
	adc	%ebp, %ebp
	mul	%r14
	xor	%r10d, %r10d
	add	%rbx, -24(%rdi,%r9,8)
	adc	%rax, %r15
	mov	-16(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	mul	%r13
	add	%rax, %r15
	mov	-16(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	adc	$0, %r10d
.Laddmul_entry_3:
	mul	%r14
	add	%r15, -16(%rdi,%r9,8)
	adc	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mul	%r13
	xor	%ebx, %ebx
	add	%rax, %rbp
	adc	%rdx, %r10
	mov	$0, %r15d
	mov	-8(%rsi,%r9,8), %rax
	adc	%r15d, %ebx
	mul	%r14
	add	%rbp, -8(%rdi,%r9,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
	mov	(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	mov	(%rsi,%r9,8), %rax
	adc	%rdx, %rbx
	adc	$0, %r15d
.Laddmul_entry_1:
	mul	%r14
	add	$4, %r9
	js	.Laddmul_top

	add	%r10, -32(%rdi)
	adc	%rax, %rbx

	imul	-24(%rsi), %r13
	add	%r13, %rbx
	add	%rbx, -24(%rdi)

	add	$2, %rcx
	jns	.Lret

	lea	16(%r11), %r11

	mov	(%r11), %r13
	mov	8(%r11), %r14

	lea	-16(%rsi), %rsi

	jmp	*%r8

.Lret:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_mullo_basecase,.-__gmpn_mullo_basecase
