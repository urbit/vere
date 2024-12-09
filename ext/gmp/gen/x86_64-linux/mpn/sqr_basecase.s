





































































































	.text
	.align	16, 0x90
	.globl	__gmpn_sqr_basecase
	.type	__gmpn_sqr_basecase,@function
	
__gmpn_sqr_basecase:

	
	mov	%edx, %ecx
	mov	%edx, %r11d		

	add	$-40, %rsp

	and	$3, %ecx
	cmp	$4, %edx
	lea	4(%rcx), %r8

	mov	%rbx, 32(%rsp)
	mov	%rbp, 24(%rsp)
	mov	%r12, 16(%rsp)
	mov	%r13, 8(%rsp)
	mov	%r14, (%rsp)

	cmovg	%r8, %rcx

	lea	.Ltab(%rip), %rax
	movslq	(%rax,%rcx,4), %r10
	add	%r10, %rax
	jmp	*%rax

	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Ltab:	.long	.L4-.Ltab
	.long	.L1-.Ltab
	.long	.L2-.Ltab
	.long	.L3-.Ltab
	.long	.L0m4-.Ltab
	.long	.L1m4-.Ltab
	.long	.L2m4-.Ltab
	.long	.L3m4-.Ltab
	.text

.L1:	mov	(%rsi), %rax
	mul	%rax
	add	$40, %rsp
	mov	%rax, (%rdi)
	mov	%rdx, 8(%rdi)
	
	ret

.L2:	mov	(%rsi), %rax
	mov	%rax, %r8
	mul	%rax
	mov	8(%rsi), %r11
	mov	%rax, (%rdi)
	mov	%r11, %rax
	mov	%rdx, %r9
	mul	%rax
	add	$40, %rsp
	mov	%rax, %r10
	mov	%r11, %rax
	mov	%rdx, %r11
	mul	%r8
	xor	%r8, %r8
	add	%rax, %r9
	adc	%rdx, %r10
	adc	%r8, %r11
	add	%rax, %r9
	mov	%r9, 8(%rdi)
	adc	%rdx, %r10
	mov	%r10, 16(%rdi)
	adc	%r8, %r11
	mov	%r11, 24(%rdi)
	
	ret

.L3:	mov	(%rsi), %rax
	mov	%rax, %r10
	mul	%rax
	mov	8(%rsi), %r11
	mov	%rax, (%rdi)
	mov	%r11, %rax
	mov	%rdx, 8(%rdi)
	mul	%rax
	mov	16(%rsi), %rcx
	mov	%rax, 16(%rdi)
	mov	%rcx, %rax
	mov	%rdx, 24(%rdi)
	mul	%rax
	mov	%rax, 32(%rdi)
	mov	%rdx, 40(%rdi)

	mov	%r11, %rax
	mul	%r10
	mov	%rax, %r8
	mov	%rcx, %rax
	mov	%rdx, %r9
	mul	%r10
	xor	%r10, %r10
	add	%rax, %r9
	mov	%r11, %rax
	mov	%r10, %r11
	adc	%rdx, %r10

	mul	%rcx
	add	$40, %rsp
	add	%rax, %r10
	adc	%r11, %rdx
	add	%r8, %r8
	adc	%r9, %r9
	adc	%r10, %r10
	adc	%rdx, %rdx
	adc	%r11, %r11
	add	%r8, 8(%rdi)
	adc	%r9, 16(%rdi)
	adc	%r10, 24(%rdi)
	adc	%rdx, 32(%rdi)
	adc	%r11, 40(%rdi)
	
	ret

.L4:	mov	(%rsi), %rax
	mov	%rax, %r11
	mul	%rax
	mov	8(%rsi), %rbx
	mov	%rax, (%rdi)
	mov	%rbx, %rax
	mov	%rdx, 8(%rdi)
	mul	%rax
	mov	%rax, 16(%rdi)
	mov	%rdx, 24(%rdi)
	mov	16(%rsi), %rax
	mul	%rax
	mov	%rax, 32(%rdi)
	mov	%rdx, 40(%rdi)
	mov	24(%rsi), %rax
	mul	%rax
	mov	%rax, 48(%rdi)
	mov	%rbx, %rax
	mov	%rdx, 56(%rdi)

	mul	%r11
	add	$32, %rsp
	mov	%rax, %r8
	mov	%rdx, %r9
	mov	16(%rsi), %rax
	mul	%r11
	xor	%r10, %r10
	add	%rax, %r9
	adc	%rdx, %r10
	mov	24(%rsi), %rax
	mul	%r11
	xor	%r11, %r11
	add	%rax, %r10
	adc	%rdx, %r11
	mov	16(%rsi), %rax
	mul	%rbx
	xor	%rcx, %rcx
	add	%rax, %r10
	adc	%rdx, %r11
	adc	$0, %rcx
	mov	24(%rsi), %rax
	mul	%rbx
	pop	%rbx
	add	%rax, %r11
	adc	%rdx, %rcx
	mov	16(%rsi), %rdx
	mov	24(%rsi), %rax
	mul	%rdx
	add	%rax, %rcx
	adc	$0, %rdx

	add	%r8, %r8
	adc	%r9, %r9
	adc	%r10, %r10
	adc	%r11, %r11
	adc	%rcx, %rcx
	mov	$0, %eax
	adc	%rdx, %rdx

	adc	%rax, %rax
	add	%r8, 8(%rdi)
	adc	%r9, 16(%rdi)
	adc	%r10, 24(%rdi)
	adc	%r11, 32(%rdi)
	adc	%rcx, 40(%rdi)
	adc	%rdx, 48(%rdi)
	adc	%rax, 56(%rdi)
	
	ret


.L0m4:
	lea	-16(%rdi,%r11,8), %r12		
	mov	(%rsi), %r13
	mov	8(%rsi), %rax
	lea	(%rsi,%r11,8), %rsi		

	lea	-4(%r11), %r8

	xor	%r9d, %r9d
	sub	%r11, %r9

	mul	%r13
	xor	%ebp, %ebp
	mov	%rax, %rbx
	mov	16(%rsi,%r9,8), %rax
	mov	%rdx, %r10
	jmp	.LL3

	.align	16, 0x90
.Lmul_1_m3_top:
	add	%rax, %rbp
	mov	%r10, (%r12,%r9,8)
	mov	(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	xor	%ebx, %ebx
	mul	%r13
	xor	%r10d, %r10d
	mov	%rbp, 8(%r12,%r9,8)
	add	%rax, %rcx
	adc	%rdx, %rbx
	mov	8(%rsi,%r9,8), %rax
	mov	%rcx, 16(%r12,%r9,8)
	xor	%ebp, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	16(%rsi,%r9,8), %rax
	adc	%rdx, %r10
.LL3:	xor	%ecx, %ecx
	mul	%r13
	add	%rax, %r10
	mov	24(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	mov	%rbx, 24(%r12,%r9,8)
	mul	%r13
	add	$4, %r9
	js	.Lmul_1_m3_top

	add	%rax, %rbp
	mov	%r10, (%r12)
	adc	%rdx, %rcx
	mov	%rbp, 8(%r12)
	mov	%rcx, 16(%r12)

	lea	16(%r12), %r12	
	lea	-8(%rsi), %rsi
	jmp	.Ldowhile


.L1m4:
	lea	8(%rdi,%r11,8), %r12		
	mov	(%rsi), %r13		
	mov	8(%rsi), %rax		
	lea	8(%rsi,%r11,8), %rsi		

	lea	-3(%r11), %r8

	lea	-3(%r11), %r9
	neg	%r9

	mov	%rax, %r14		
	mul	%r13			
	mov	%rdx, %rcx
	xor	%ebp, %ebp
	mov	%rax, 8(%rdi)
	jmp	.Lm0

	.align	16, 0x90
.Lmul_2_m0_top:
	mul	%r14
	add	%rax, %rbx
	adc	%rdx, %rcx
	mov	-24(%rsi,%r9,8), %rax
	mov	$0, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	-24(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
	mul	%r14			
	add	%rax, %rcx
	mov	%rbx, -24(%r12,%r9,8)
	adc	%rdx, %rbp
.Lm0:	mov	-16(%rsi,%r9,8), %rax	
	mul	%r13			
	mov	$0, %r10d
	add	%rax, %rcx
	adc	%rdx, %rbp
	mov	-16(%rsi,%r9,8), %rax
	adc	$0, %r10d
	mov	$0, %ebx
	mov	%rcx, -16(%r12,%r9,8)
	mul	%r14
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mov	$0, %ecx
	mul	%r13
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	adc	$0, %ebx
	mul	%r14
	add	%rax, %r10
	mov	%rbp, -8(%r12,%r9,8)
	adc	%rdx, %rbx
.Lm2x:	mov	(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	adc	%rdx, %rbx
	adc	$0, %ecx
	add	$4, %r9
	mov	-32(%rsi,%r9,8), %rax
	mov	%r10, -32(%r12,%r9,8)
	js	.Lmul_2_m0_top

	mul	%r14
	add	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, -8(%r12)
	mov	%rcx, (%r12)

	lea	-16(%rsi), %rsi
	lea	0(%r12), %r12	
	jmp	.Ldowhile_end


.L2m4:
	lea	-16(%rdi,%r11,8), %r12		
	mov	(%rsi), %r13
	mov	8(%rsi), %rax
	lea	(%rsi,%r11,8), %rsi		

	lea	-4(%r11), %r8

	lea	-2(%r11), %r9
	neg	%r9

	mul	%r13
	mov	%rax, %rbp
	mov	(%rsi,%r9,8), %rax
	mov	%rdx, %rcx
	jmp	.LL1

	.align	16, 0x90
.Lmul_1_m1_top:
	add	%rax, %rbp
	mov	%r10, (%r12,%r9,8)
	mov	(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
.LL1:	xor	%ebx, %ebx
	mul	%r13
	xor	%r10d, %r10d
	mov	%rbp, 8(%r12,%r9,8)
	add	%rax, %rcx
	adc	%rdx, %rbx
	mov	8(%rsi,%r9,8), %rax
	mov	%rcx, 16(%r12,%r9,8)
	xor	%ebp, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	16(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	xor	%ecx, %ecx
	mul	%r13
	add	%rax, %r10
	mov	24(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	mov	%rbx, 24(%r12,%r9,8)
	mul	%r13
	add	$4, %r9
	js	.Lmul_1_m1_top

	add	%rax, %rbp
	mov	%r10, (%r12)
	adc	%rdx, %rcx
	mov	%rbp, 8(%r12)
	mov	%rcx, 16(%r12)

	lea	16(%r12), %r12	
	lea	-8(%rsi), %rsi
	jmp	.Ldowhile_mid


.L3m4:
	lea	8(%rdi,%r11,8), %r12		
	mov	(%rsi), %r13		
	mov	8(%rsi), %rax		
	lea	8(%rsi,%r11,8), %rsi		

	lea	-5(%r11), %r8

	lea	-1(%r11), %r9
	neg	%r9

	mov	%rax, %r14		
	mul	%r13			
	mov	%rdx, %r10
	xor	%ebx, %ebx
	xor	%ecx, %ecx
	mov	%rax, 8(%rdi)
	jmp	.Lm2

	.align	16, 0x90
.Lmul_2_m2_top:
	mul	%r14
	add	%rax, %rbx
	adc	%rdx, %rcx
	mov	-24(%rsi,%r9,8), %rax
	mov	$0, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	-24(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
	mul	%r14			
	add	%rax, %rcx
	mov	%rbx, -24(%r12,%r9,8)
	adc	%rdx, %rbp
	mov	-16(%rsi,%r9,8), %rax
	mul	%r13
	mov	$0, %r10d
	add	%rax, %rcx
	adc	%rdx, %rbp
	mov	-16(%rsi,%r9,8), %rax
	adc	$0, %r10d
	mov	$0, %ebx
	mov	%rcx, -16(%r12,%r9,8)
	mul	%r14
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mov	$0, %ecx
	mul	%r13
	add	%rax, %rbp
	mov	-8(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	adc	$0, %ebx
	mul	%r14
	add	%rax, %r10
	mov	%rbp, -8(%r12,%r9,8)
	adc	%rdx, %rbx
.Lm2:	mov	(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	adc	%rdx, %rbx
	adc	$0, %ecx
	add	$4, %r9
	mov	-32(%rsi,%r9,8), %rax
	mov	%r10, -32(%r12,%r9,8)
	js	.Lmul_2_m2_top

	mul	%r14
	add	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, -8(%r12)
	mov	%rcx, (%r12)

	lea	-16(%rsi), %rsi
	jmp	.Ldowhile_mid

.Ldowhile:

	lea	4(%r8), %r9
	neg	%r9

	mov	16(%rsi,%r9,8), %r13
	mov	24(%rsi,%r9,8), %r14
	mov	24(%rsi,%r9,8), %rax
	mul	%r13
	xor	%r10d, %r10d
	add	%rax, 24(%r12,%r9,8)
	adc	%rdx, %r10
	xor	%ebx, %ebx
	xor	%ecx, %ecx
	jmp	.Lam2

	.align	16, 0x90
.Laddmul_2_m2_top:
	add	%r10, (%r12,%r9,8)
	adc	%rax, %rbx
	mov	8(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	mov	$0, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	8(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
	mul	%r14				
	add	%rbx, 8(%r12,%r9,8)
	adc	%rax, %rcx
	adc	%rdx, %rbp
	mov	16(%rsi,%r9,8), %rax
	mov	$0, %r10d
	mul	%r13				
	add	%rax, %rcx
	mov	16(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	adc	$0, %r10d
	mul	%r14				
	add	%rcx, 16(%r12,%r9,8)
	adc	%rax, %rbp
	mov	24(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mul	%r13
	mov	$0, %ebx
	add	%rax, %rbp
	adc	%rdx, %r10
	mov	$0, %ecx
	mov	24(%rsi,%r9,8), %rax
	adc	$0, %ebx
	mul	%r14
	add	%rbp, 24(%r12,%r9,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
.Lam2:	mov	32(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	mov	32(%rsi,%r9,8), %rax
	adc	%rdx, %rbx
	adc	$0, %ecx
	mul	%r14
	add	$4, %r9
	js	.Laddmul_2_m2_top

	add	%r10, (%r12)
	adc	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, 8(%r12)
	mov	%rcx, 16(%r12)

	lea	16(%r12), %r12	

	add	$-2, %r8d		

.Ldowhile_mid:

	lea	2(%r8), %r9
	neg	%r9

	mov	(%rsi,%r9,8), %r13
	mov	8(%rsi,%r9,8), %r14
	mov	8(%rsi,%r9,8), %rax
	mul	%r13
	xor	%ecx, %ecx
	add	%rax, 8(%r12,%r9,8)
	adc	%rdx, %rcx
	xor	%ebp, %ebp
	jmp	.L20

	.align	16, 0x90
.Laddmul_2_m0_top:
	add	%r10, (%r12,%r9,8)
	adc	%rax, %rbx
	mov	8(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	mov	$0, %ebp
	mul	%r13
	add	%rax, %rbx
	mov	8(%rsi,%r9,8), %rax
	adc	%rdx, %rcx
	adc	$0, %ebp
	mul	%r14				
	add	%rbx, 8(%r12,%r9,8)
	adc	%rax, %rcx
	adc	%rdx, %rbp
.L20:	mov	16(%rsi,%r9,8), %rax
	mov	$0, %r10d
	mul	%r13				
	add	%rax, %rcx
	mov	16(%rsi,%r9,8), %rax
	adc	%rdx, %rbp
	adc	$0, %r10d
	mul	%r14				
	add	%rcx, 16(%r12,%r9,8)
	adc	%rax, %rbp
	mov	24(%rsi,%r9,8), %rax
	adc	%rdx, %r10
	mul	%r13
	mov	$0, %ebx
	add	%rax, %rbp
	adc	%rdx, %r10
	mov	$0, %ecx
	mov	24(%rsi,%r9,8), %rax
	adc	$0, %ebx
	mul	%r14
	add	%rbp, 24(%r12,%r9,8)
	adc	%rax, %r10
	adc	%rdx, %rbx
	mov	32(%rsi,%r9,8), %rax
	mul	%r13
	add	%rax, %r10
	mov	32(%rsi,%r9,8), %rax
	adc	%rdx, %rbx
	adc	$0, %ecx
	mul	%r14
	add	$4, %r9
	js	.Laddmul_2_m0_top

	add	%r10, (%r12)
	adc	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, 8(%r12)
	mov	%rcx, 16(%r12)

	lea	16(%r12), %r12	
.Ldowhile_end:

	add	$-2, %r8d		
	jne	.Ldowhile


	mov	-16(%rsi), %r13
	mov	-8(%rsi), %r14
	mov	-8(%rsi), %rax
	mul	%r13
	xor	%r10d, %r10d
	add	%rax, -8(%r12)
	adc	%rdx, %r10
	xor	%ebx, %ebx
	xor	%ecx, %ecx
	mov	(%rsi), %rax
	mul	%r13
	add	%rax, %r10
	mov	(%rsi), %rax
	adc	%rdx, %rbx
	mul	%r14
	add	%r10, (%r12)
	adc	%rax, %rbx
	adc	%rdx, %rcx
	mov	%rbx, 8(%r12)
	mov	%rcx, 16(%r12)


	lea	-4(%r11,%r11), %r9

	mov	8(%rdi), %r11
	lea	-8(%rsi), %rsi
	lea	(%rdi,%r9,8), %rdi
	neg	%r9
	mov	(%rsi,%r9,4), %rax
	mul	%rax
	test	$2, %r9b
	jnz	.Lodd

.Levn:	add	%r11, %r11
	sbb	%ebx, %ebx		
	add	%rdx, %r11
	mov	%rax, (%rdi,%r9,8)
	jmp	.Ld0

.Lodd:	add	%r11, %r11
	sbb	%ebp, %ebp		
	add	%rdx, %r11
	mov	%rax, (%rdi,%r9,8)
	lea	-2(%r9), %r9
	jmp	.Ld1

	.align	16, 0x90
.Ltop:	mov	(%rsi,%r9,4), %rax
	mul	%rax
	add	%ebp, %ebp		
	adc	%rax, %r10
	adc	%rdx, %r11
	mov	%r10, (%rdi,%r9,8)
.Ld0:	mov	%r11, 8(%rdi,%r9,8)
	mov	16(%rdi,%r9,8), %r10
	adc	%r10, %r10
	mov	24(%rdi,%r9,8), %r11
	adc	%r11, %r11
	nop
	sbb	%ebp, %ebp		
	mov	8(%rsi,%r9,4), %rax
	mul	%rax
	add	%ebx, %ebx		
	adc	%rax, %r10
	adc	%rdx, %r11
	mov	%r10, 16(%rdi,%r9,8)
.Ld1:	mov	%r11, 24(%rdi,%r9,8)
	mov	32(%rdi,%r9,8), %r10
	adc	%r10, %r10
	mov	40(%rdi,%r9,8), %r11
	adc	%r11, %r11
	sbb	%ebx, %ebx		
	add	$4, %r9
	js	.Ltop

	mov	(%rsi), %rax
	mul	%rax
	add	%ebp, %ebp		
	adc	%rax, %r10
	adc	%rdx, %r11
	mov	%r10, (%rdi)
	mov	%r11, 8(%rdi)
	mov	16(%rdi), %r10
	adc	%r10, %r10
	sbb	%ebp, %ebp		
	neg	%ebp
	mov	8(%rsi), %rax
	mul	%rax
	add	%ebx, %ebx		
	adc	%rax, %r10
	adc	%rbp, %rdx
	mov	%r10, 16(%rdi)
	mov	%rdx, 24(%rdi)

	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_sqr_basecase,.-__gmpn_sqr_basecase
