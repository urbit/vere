





















































































	.text
	.align	16, 0x90
	.globl	__gmpn_mul_basecase
	
	.def	__gmpn_mul_basecase
	.scl	2
	.type	32
	.endef
__gmpn_mul_basecase:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	mov	56(%rsp), %r8d	
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	xor	%r13d, %r13d
	mov	(%rsi), %rax
	mov	(%rcx), %r12

	sub	%rdx, %r13		
	mov	%r13, %r11
	mov	%edx, %ebx

	lea	(%rdi,%rdx,8), %rdi
	lea	(%rsi,%rdx,8), %rsi

	mul	%r12

	test	$1, %r8b
	jz	Lmul_2




Lmul_1:
	and	$3, %ebx
	jz	Lmul_1_prologue_0
	cmp	$2, %ebx
	jc	Lmul_1_prologue_1
	jz	Lmul_1_prologue_2

Lmul_1_prologue_3:
	add	$-1, %r11
	lea	Laddmul_outer_3(%rip), %r14
	mov	%rax, %r10
	mov	%rdx, %rbx
	jmp	Lmul_1_entry_3

Lmul_1_prologue_0:
	mov	%rax, %rbp
	mov	%rdx, %r10		
	lea	Laddmul_outer_0(%rip), %r14
	jmp	Lmul_1_entry_0

Lmul_1_prologue_1:
	cmp	$-1, %r13
	jne	2f
	mov	%rax, -8(%rdi)
	mov	%rdx, (%rdi)
	jmp	Lret
2:	add	$1, %r11
	lea	Laddmul_outer_1(%rip), %r14
	mov	%rax, %r15
	mov	%rdx, %rbp
	xor	%r10d, %r10d
	mov	(%rsi,%r11,8), %rax
	jmp	Lmul_1_entry_1

Lmul_1_prologue_2:
	add	$-2, %r11
	lea	Laddmul_outer_2(%rip), %r14
	mov	%rax, %rbx
	mov	%rdx, %r15
	mov	24(%rsi,%r11,8), %rax
	xor	%ebp, %ebp
	xor	%r10d, %r10d
	jmp	Lmul_1_entry_2


	

	.align	16, 0x90
Lmul_1_top:
	mov	%rbx, -16(%rdi,%r11,8)
	add	%rax, %r15
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %rbp
Lmul_1_entry_1:
	xor	%ebx, %ebx
	mul	%r12
	mov	%r15, -8(%rdi,%r11,8)
	add	%rax, %rbp
	adc	%rdx, %r10
Lmul_1_entry_0:
	mov	8(%rsi,%r11,8), %rax
	mul	%r12
	mov	%rbp, (%rdi,%r11,8)
	add	%rax, %r10
	adc	%rdx, %rbx
Lmul_1_entry_3:
	mov	16(%rsi,%r11,8), %rax
	mul	%r12
	mov	%r10, 8(%rdi,%r11,8)
	xor	%ebp, %ebp	
	mov	%rbp, %r10			
	add	%rax, %rbx
	mov	24(%rsi,%r11,8), %rax
	mov	%rbp, %r15			
	adc	%rdx, %r15
Lmul_1_entry_2:
	mul	%r12
	add	$4, %r11
	js	Lmul_1_top

	mov	%rbx, -16(%rdi)
	add	%rax, %r15
	mov	%r15, -8(%rdi)
	adc	%rdx, %rbp
	mov	%rbp, (%rdi)

	add	$-1, %r8			
	jz	Lret

	mov	8(%rcx), %r12
	mov	16(%rcx), %r9

	lea	8(%rcx), %rcx		
	lea	8(%rdi), %rdi		

	jmp	*%r14




	.align	16, 0x90
Lmul_2:
	mov	8(%rcx), %r9

	and	$3, %ebx
	jz	Lmul_2_prologue_0
	cmp	$2, %ebx
	jz	Lmul_2_prologue_2
	jc	Lmul_2_prologue_1

Lmul_2_prologue_3:
	lea	Laddmul_outer_3(%rip), %r14
	add	$2, %r11
	mov	%rax, -16(%rdi,%r11,8)
	mov	%rdx, %rbp
	xor	%r10d, %r10d
	xor	%ebx, %ebx
	mov	-16(%rsi,%r11,8), %rax
	jmp	Lmul_2_entry_3

	.align	16, 0x90
Lmul_2_prologue_0:
	add	$3, %r11
	mov	%rax, %rbx
	mov	%rdx, %r15
	xor	%ebp, %ebp
	mov	-24(%rsi,%r11,8), %rax
	lea	Laddmul_outer_0(%rip), %r14
	jmp	Lmul_2_entry_0

	.align	16, 0x90
Lmul_2_prologue_1:
	mov	%rax, %r10
	mov	%rdx, %rbx
	xor	%r15d, %r15d
	lea	Laddmul_outer_1(%rip), %r14
	jmp	Lmul_2_entry_1

	.align	16, 0x90
Lmul_2_prologue_2:
	add	$1, %r11
	lea	Laddmul_outer_2(%rip), %r14
	mov	$0, %ebx
	mov	$0, %r15d
	mov	%rax, %rbp
	mov	-8(%rsi,%r11,8), %rax
	mov	%rdx, %r10
	jmp	Lmul_2_entry_2

	

	.align	16, 0x90
Lmul_2_top:
	mov	-32(%rsi,%r11,8), %rax
	mul	%r9
	add	%rax, %rbx
	adc	%rdx, %r15
	mov	-24(%rsi,%r11,8), %rax
	xor	%ebp, %ebp
	mul	%r12
	add	%rax, %rbx
	mov	-24(%rsi,%r11,8), %rax
	adc	%rdx, %r15
	adc	$0, %ebp
Lmul_2_entry_0:
	mul	%r9
	add	%rax, %r15
	mov	%rbx, -24(%rdi,%r11,8)
	adc	%rdx, %rbp
	mov	-16(%rsi,%r11,8), %rax
	mul	%r12
	mov	$0, %r10d
	add	%rax, %r15
	adc	%rdx, %rbp
	mov	-16(%rsi,%r11,8), %rax
	adc	$0, %r10d
	mov	$0, %ebx
	mov	%r15, -16(%rdi,%r11,8)
Lmul_2_entry_3:
	mul	%r9
	add	%rax, %rbp
	mov	-8(%rsi,%r11,8), %rax
	adc	%rdx, %r10
	mov	$0, %r15d
	mul	%r12
	add	%rax, %rbp
	mov	-8(%rsi,%r11,8), %rax
	adc	%rdx, %r10
	adc	%r15d, %ebx	
Lmul_2_entry_2:
	mul	%r9
	add	%rax, %r10
	mov	%rbp, -8(%rdi,%r11,8)
	adc	%rdx, %rbx
	mov	(%rsi,%r11,8), %rax
	mul	%r12
	add	%rax, %r10
	adc	%rdx, %rbx
	adc	$0, %r15d
Lmul_2_entry_1:
	add	$4, %r11
	mov	%r10, -32(%rdi,%r11,8)
	js	Lmul_2_top

	mov	-32(%rsi,%r11,8), %rax	
	mul	%r9
	add	%rax, %rbx
	mov	%rbx, (%rdi)
	adc	%rdx, %r15
	mov	%r15, 8(%rdi)

	add	$-2, %r8			
	jz	Lret

	mov	16(%rcx), %r12
	mov	24(%rcx), %r9

	lea	16(%rcx), %rcx		
	lea	16(%rdi), %rdi		

	jmp	*%r14





	
	

Laddmul_outer_0:
	add	$3, %r13
	lea	0(%rip), %r14

	mov	%r13, %r11
	mov	-24(%rsi,%r13,8), %rax
	mul	%r12
	mov	%rax, %rbx
	mov	-24(%rsi,%r13,8), %rax
	mov	%rdx, %r15
	xor	%ebp, %ebp
	jmp	Laddmul_entry_0

Laddmul_outer_1:
	mov	%r13, %r11
	mov	(%rsi,%r13,8), %rax
	mul	%r12
	mov	%rax, %r10
	mov	(%rsi,%r13,8), %rax
	mov	%rdx, %rbx
	xor	%r15d, %r15d
	jmp	Laddmul_entry_1

Laddmul_outer_2:
	add	$1, %r13
	lea	0(%rip), %r14

	mov	%r13, %r11
	mov	-8(%rsi,%r13,8), %rax
	mul	%r12
	xor	%ebx, %ebx
	mov	%rax, %rbp
	xor	%r15d, %r15d
	mov	%rdx, %r10
	mov	-8(%rsi,%r13,8), %rax
	jmp	Laddmul_entry_2

Laddmul_outer_3:
	add	$2, %r13
	lea	0(%rip), %r14

	mov	%r13, %r11
	mov	-16(%rsi,%r13,8), %rax
	xor	%r10d, %r10d
	mul	%r12
	mov	%rax, %r15
	mov	-16(%rsi,%r13,8), %rax
	mov	%rdx, %rbp
	jmp	Laddmul_entry_3

	

	.align	16, 0x90
Laddmul_top:
	add	%r10, -32(%rdi,%r11,8)
	adc	%rax, %rbx
	mov	-24(%rsi,%r11,8), %rax
	adc	%rdx, %r15
	xor	%ebp, %ebp
	mul	%r12
	add	%rax, %rbx
	mov	-24(%rsi,%r11,8), %rax
	adc	%rdx, %r15
	adc	%ebp, %ebp	
Laddmul_entry_0:
	mul	%r9
	xor	%r10d, %r10d
	add	%rbx, -24(%rdi,%r11,8)
	adc	%rax, %r15
	mov	-16(%rsi,%r11,8), %rax
	adc	%rdx, %rbp
	mul	%r12
	add	%rax, %r15
	mov	-16(%rsi,%r11,8), %rax
	adc	%rdx, %rbp
	adc	$0, %r10d
Laddmul_entry_3:
	mul	%r9
	add	%r15, -16(%rdi,%r11,8)
	adc	%rax, %rbp
	mov	-8(%rsi,%r11,8), %rax
	adc	%rdx, %r10
	mul	%r12
	xor	%ebx, %ebx
	add	%rax, %rbp
	adc	%rdx, %r10
	mov	$0, %r15d
	mov	-8(%rsi,%r11,8), %rax
	adc	%r15d, %ebx	
Laddmul_entry_2:
	mul	%r9
	add	%rbp, -8(%rdi,%r11,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
	mov	(%rsi,%r11,8), %rax
	mul	%r12
	add	%rax, %r10
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %rbx
	adc	$0, %r15d
Laddmul_entry_1:
	mul	%r9
	add	$4, %r11
	js	Laddmul_top

	add	%r10, -8(%rdi)
	adc	%rax, %rbx
	mov	%rbx, (%rdi)
	adc	%rdx, %r15
	mov	%r15, 8(%rdi)

	add	$-2, %r8			
	jz	Lret

	lea	16(%rdi), %rdi		
	lea	16(%rcx), %rcx		

	mov	(%rcx), %r12
	mov	8(%rcx), %r9

	jmp	*%r14

	.align	16, 0x90
Lret:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rsi
	pop	%rdi
	ret

	
