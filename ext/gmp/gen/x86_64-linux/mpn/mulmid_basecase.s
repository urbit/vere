












































































	.text
	.align	16, 0x90
	.globl	__gmpn_mulmid_basecase
	.type	__gmpn_mulmid_basecase,@function
	
__gmpn_mulmid_basecase:

	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	mov	%rcx, %r15

	
	lea	1(%rdx), %r13
	sub	%r8, %r13

	lea	(%rdi,%r13,8), %rdi

	cmp	$4, %r13		
	jc	.Ldiagonal

	lea	(%rsi,%rdx,8), %rsi

	test	$1, %r8
	jz	.Lmul_2




.Lmul_1:
	mov	%r13d, %ebx

	neg	%r13
	mov	(%rsi,%r13,8), %rax
	mov	(%r15), %r12
	mul	%r12

	and	$-4, %r13		
	mov	%r13, %r11

	and	$3, %ebx
	jz	.Lmul_1_prologue_0
	cmp	$2, %ebx
	jc	.Lmul_1_prologue_1
	jz	.Lmul_1_prologue_2

.Lmul_1_prologue_3:
	mov	%rax, %r10
	mov	%rdx, %rbx
	lea	.Laddmul_prologue_3(%rip), %r14
	jmp	.Lmul_1_entry_3

	.align	16, 0x90
.Lmul_1_prologue_0:
	mov	%rax, %rbp
	mov	%rdx, %r10		
	lea	.Laddmul_prologue_0(%rip), %r14
	jmp	.Lmul_1_entry_0

	.align	16, 0x90
.Lmul_1_prologue_1:
	add	$4, %r11
	mov	%rax, %rcx
	mov	%rdx, %rbp
	mov	$0, %r10d
	mov	(%rsi,%r11,8), %rax
	lea	.Laddmul_prologue_1(%rip), %r14
	jmp	.Lmul_1_entry_1

	.align	16, 0x90
.Lmul_1_prologue_2:
	mov	%rax, %rbx
	mov	%rdx, %rcx
	mov	24(%rsi,%r11,8), %rax
	mov	$0, %ebp
	mov	$0, %r10d
	lea	.Laddmul_prologue_2(%rip), %r14
	jmp	.Lmul_1_entry_2


	

	.align	16, 0x90
.Lmul_1_top:
	mov	%rbx, -16(%rdi,%r11,8)
	add	%rax, %rcx
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %rbp
.Lmul_1_entry_1:
	mov	$0, %ebx
	mul	%r12
	mov	%rcx, -8(%rdi,%r11,8)
	add	%rax, %rbp
	adc	%rdx, %r10
.Lmul_1_entry_0:
	mov	8(%rsi,%r11,8), %rax
	mul	%r12
	mov	%rbp, (%rdi,%r11,8)
	add	%rax, %r10
	adc	%rdx, %rbx
.Lmul_1_entry_3:
	mov	16(%rsi,%r11,8), %rax
	mul	%r12
	mov	%r10, 8(%rdi,%r11,8)
	mov	$0, %ebp		
	mov	%rbp, %r10			
	add	%rax, %rbx
	mov	24(%rsi,%r11,8), %rax
	mov	%rbp, %rcx			
	adc	%rdx, %rcx
.Lmul_1_entry_2:
	mul	%r12
	add	$4, %r11
	js	.Lmul_1_top

	mov	%rbx, -16(%rdi)
	add	%rax, %rcx
	mov	%rcx, -8(%rdi)
	mov	%rbp, 8(%rdi)		
	adc	%rdx, %rbp
	mov	%rbp, (%rdi)

	dec	%r8
	jz	.Lret

	lea	-8(%rsi), %rsi
	lea	8(%r15), %r15

	mov	%r13, %r11
	mov	(%r15), %r12
	mov	8(%r15), %r9

	jmp	*%r14




	.align	16, 0x90
.Lmul_2:
	mov	%r13d, %ebx

	neg	%r13
	mov	-8(%rsi,%r13,8), %rax
	mov	(%r15), %r12
	mov	8(%r15), %r9
	mul	%r9

	and	$-4, %r13		
	mov	%r13, %r11

	and	$3, %ebx
	jz	.Lmul_2_prologue_0
	cmp	$2, %ebx
	jc	.Lmul_2_prologue_1
	jz	.Lmul_2_prologue_2

.Lmul_2_prologue_3:
	mov	%rax, %rcx
	mov	%rdx, %rbp
	lea	.Laddmul_prologue_3(%rip), %r14
	jmp	.Lmul_2_entry_3

	.align	16, 0x90
.Lmul_2_prologue_0:
	mov	%rax, %rbx
	mov	%rdx, %rcx
	lea	.Laddmul_prologue_0(%rip), %r14
	jmp	.Lmul_2_entry_0

	.align	16, 0x90
.Lmul_2_prologue_1:
	mov	%rax, %r10
	mov	%rdx, %rbx
	mov	$0, %ecx
	lea	.Laddmul_prologue_1(%rip), %r14
	jmp	.Lmul_2_entry_1

	.align	16, 0x90
.Lmul_2_prologue_2:
	mov	%rax, %rbp
	mov	%rdx, %r10
	mov	$0, %ebx
	mov	16(%rsi,%r11,8), %rax
	lea	.Laddmul_prologue_2(%rip), %r14
	jmp	.Lmul_2_entry_2


	

	.align	16, 0x90
.Lmul_2_top:
	mov     -8(%rsi,%r11,8), %rax
	mul     %r9
	add     %rax, %rbx
	adc     %rdx, %rcx
.Lmul_2_entry_0:
	mov     $0, %ebp
	mov     (%rsi,%r11,8), %rax
	mul     %r12
	add     %rax, %rbx
	mov     (%rsi,%r11,8), %rax
	adc     %rdx, %rcx
	adc     $0, %ebp
	mul     %r9
	add     %rax, %rcx
	mov     %rbx, (%rdi,%r11,8)
	adc     %rdx, %rbp
.Lmul_2_entry_3:
	mov     8(%rsi,%r11,8), %rax
	mul     %r12
	mov     $0, %r10d
	add     %rax, %rcx
	adc     %rdx, %rbp
	mov     $0, %ebx
	adc     $0, %r10d
	mov     8(%rsi,%r11,8), %rax
	mov     %rcx, 8(%rdi,%r11,8)
	mul     %r9
	add     %rax, %rbp
	mov     16(%rsi,%r11,8), %rax
	adc     %rdx, %r10
.Lmul_2_entry_2:
	mov     $0, %ecx
	mul     %r12
	add     %rax, %rbp
	mov     16(%rsi,%r11,8), %rax
	adc     %rdx, %r10
	adc     $0, %ebx
	mul     %r9
	add     %rax, %r10
	mov     %rbp, 16(%rdi,%r11,8)
	adc     %rdx, %rbx
.Lmul_2_entry_1:
	mov     24(%rsi,%r11,8), %rax
	mul     %r12
	add     %rax, %r10
	adc     %rdx, %rbx
	adc     $0, %ecx
	add     $4, %r11
	mov     %r10, -8(%rdi,%r11,8)
	jnz     .Lmul_2_top

	mov	%rbx, (%rdi)
	mov	%rcx, 8(%rdi)

	sub	$2, %r8
	jz	.Lret

	lea	16(%r15), %r15
	lea	-16(%rsi), %rsi

	mov	%r13, %r11
	mov	(%r15), %r12
	mov	8(%r15), %r9

	jmp	*%r14




	.align	16, 0x90
.Laddmul_prologue_0:
	mov	-8(%rsi,%r11,8), %rax
	mul	%r9
	mov	%rax, %rcx
	mov	%rdx, %rbp
	mov	$0, %r10d
	jmp	.Laddmul_entry_0

	.align	16, 0x90
.Laddmul_prologue_1:
	mov	16(%rsi,%r11,8), %rax
	mul	%r9
	mov	%rax, %rbx
	mov	%rdx, %rcx
	mov	$0, %ebp
	mov	24(%rsi,%r11,8), %rax
	jmp	.Laddmul_entry_1

	.align	16, 0x90
.Laddmul_prologue_2:
	mov	8(%rsi,%r11,8), %rax
	mul	%r9
	mov	%rax, %r10
	mov	%rdx, %rbx
	mov	$0, %ecx
	jmp	.Laddmul_entry_2

	.align	16, 0x90
.Laddmul_prologue_3:
	mov	(%rsi,%r11,8), %rax
	mul	%r9
	mov	%rax, %rbp
	mov	%rdx, %r10
	mov	$0, %ebx
	mov	$0, %ecx
	jmp	.Laddmul_entry_3

	

	.align	16, 0x90
.Laddmul_top:
	mov	$0, %r10d
	add	%rax, %rbx
	mov	-8(%rsi,%r11,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
	mul	%r9
	add	%rbx, -8(%rdi,%r11,8)
	adc	%rax, %rcx
	adc	%rdx, %rbp
.Laddmul_entry_0:
	mov	(%rsi,%r11,8), %rax
	mul	%r12
	add	%rax, %rcx
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %rbp
	adc	$0, %r10d
	mul	%r9
	add	%rcx, (%rdi,%r11,8)
	mov	$0, %ecx
	adc	%rax, %rbp
	mov	$0, %ebx
	adc	%rdx, %r10
.Laddmul_entry_3:
	mov	8(%rsi,%r11,8), %rax
	mul	%r12
	add	%rax, %rbp
	mov	8(%rsi,%r11,8), %rax
	adc	%rdx, %r10
	adc	$0, %ebx
	mul	%r9
	add	%rbp, 8(%rdi,%r11,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
.Laddmul_entry_2:
	mov	16(%rsi,%r11,8), %rax
	mul	%r12
	add	%rax, %r10
	mov	16(%rsi,%r11,8), %rax
	adc	%rdx, %rbx
	adc	$0, %ecx
	mul	%r9
	add	%r10, 16(%rdi,%r11,8)
	nop			
	adc	%rax, %rbx
	mov	$0, %ebp
	mov	24(%rsi,%r11,8), %rax
	adc	%rdx, %rcx
.Laddmul_entry_1:
	mul	%r12
	add	$4, %r11
	jnz	.Laddmul_top

	add	%rax, %rbx
	adc	%rdx, %rcx
	adc	$0, %ebp

	add	%rbx, -8(%rdi)
	adc	%rcx, (%rdi)
	adc	%rbp, 8(%rdi)

	sub	$2, %r8
	jz	.Lret

	lea	16(%r15), %r15
	lea	-16(%rsi), %rsi

	mov	%r13, %r11
	mov	(%r15), %r12
	mov	8(%r15), %r9

	jmp	*%r14




	.align	16, 0x90
.Ldiagonal:
	xor	%ebx, %ebx
	xor	%ecx, %ecx
	xor	%ebp, %ebp

	neg	%r13

	mov	%r8d, %eax
	and	$3, %eax
	jz	.Ldiag_prologue_0
	cmp	$2, %eax
	jc	.Ldiag_prologue_1
	jz	.Ldiag_prologue_2

.Ldiag_prologue_3:
	lea	-8(%r15), %r15
	mov	%r15, %r10
	add	$1, %r8
	mov	%r8, %r11
	lea	.Ldiag_entry_3(%rip), %r14
	jmp	.Ldiag_entry_3

.Ldiag_prologue_0:
	mov	%r15, %r10
	mov	%r8, %r11
	lea	0(%rip), %r14
	mov     -8(%rsi,%r11,8), %rax
	jmp	.Ldiag_entry_0

.Ldiag_prologue_1:
	lea	8(%r15), %r15
	mov	%r15, %r10
	add	$3, %r8
	mov	%r8, %r11
	lea	0(%rip), %r14
	mov     -8(%r10), %rax
	jmp	.Ldiag_entry_1

.Ldiag_prologue_2:
	lea	-16(%r15), %r15
	mov	%r15, %r10
	add	$2, %r8
	mov	%r8, %r11
	lea	0(%rip), %r14
	mov	16(%r10), %rax
	jmp	.Ldiag_entry_2


	

	.align	16, 0x90
.Ldiag_top:
	add     %rax, %rbx
	adc     %rdx, %rcx
	mov     -8(%rsi,%r11,8), %rax
	adc     $0, %rbp
.Ldiag_entry_0:
	mulq    (%r10)
	add     %rax, %rbx
	adc     %rdx, %rcx
	adc     $0, %rbp
.Ldiag_entry_3:
	mov     -16(%rsi,%r11,8), %rax
	mulq    8(%r10)
	add     %rax, %rbx
	mov     16(%r10), %rax
	adc     %rdx, %rcx
	adc     $0, %rbp
.Ldiag_entry_2:
	mulq    -24(%rsi,%r11,8)
	add     %rax, %rbx
	mov     24(%r10), %rax
	adc     %rdx, %rcx
	lea     32(%r10), %r10
	adc     $0, %rbp
.Ldiag_entry_1:
	mulq    -32(%rsi,%r11,8)
	sub     $4, %r11
	jnz	.Ldiag_top

	add	%rax, %rbx
	adc	%rdx, %rcx
	adc	$0, %rbp

	mov	%rbx, (%rdi,%r13,8)

	inc	%r13
	jz	.Ldiag_end

	mov	%r8, %r11
	mov	%r15, %r10

	lea	8(%rsi), %rsi
	mov	%rcx, %rbx
	mov	%rbp, %rcx
	xor	%ebp, %ebp

	jmp	*%r14

.Ldiag_end:
	mov	%rcx, (%rdi)
	mov	%rbp, 8(%rdi)

.Lret:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_mulmid_basecase,.-__gmpn_mulmid_basecase
