






































































   
   
   
   






      

	.text
	.align	16, 0x90
	.globl	__gmpn_mul_1c
	.type	__gmpn_mul_1c,@function
	
__gmpn_mul_1c:




	push	%rbx
	mov	%r8, %r10
	
	jmp	.Lcommon
	.size	__gmpn_mul_1c,.-__gmpn_mul_1c

	.globl	__gmpn_mul_1
	.type	__gmpn_mul_1,@function
	
__gmpn_mul_1:





	push	%rbx
	xor	%r10, %r10
.Lcommon:
	mov	(%rsi), %rax		
	mov	%rdx, %rbx   	

	mul	%rcx
	mov	%rbx, %r11         

	add	%r10, %rax
	adc	$0, %rdx

	and	$3, %ebx
	jz	.Lb0
	cmp	$2, %ebx
	jz	.Lb2
	jg	.Lb3

.Lb1:	dec	%r11
	jne	.Lgt1
	mov	%rax, (%rdi)
	jmp	.Lret
.Lgt1:	lea	8(%rsi,%r11,8), %rsi
	lea	-8(%rdi,%r11,8), %rdi
	neg	%r11
	xor	%r10, %r10
	xor	%ebx, %ebx
	mov	%rax, %r9
	mov	(%rsi,%r11,8), %rax
	mov	%rdx, %r8
	jmp	.LL1

.Lb0:	lea	(%rsi,%r11,8), %rsi
	lea	-16(%rdi,%r11,8), %rdi
	neg	%r11
	xor	%r10, %r10
	mov	%rax, %r8
	mov	%rdx, %rbx
	jmp	 .LL0

.Lb3:	lea	-8(%rsi,%r11,8), %rsi
	lea	-24(%rdi,%r11,8), %rdi
	neg	%r11
	mov	%rax, %rbx
	mov	%rdx, %r10
	jmp	.LL3

.Lb2:	lea	-16(%rsi,%r11,8), %rsi
	lea	-32(%rdi,%r11,8), %rdi
	neg	%r11
	xor	%r8, %r8
	xor	%ebx, %ebx
	mov	%rax, %r10
	mov	24(%rsi,%r11,8), %rax
	mov	%rdx, %r9
	jmp	.LL2

	.align	16, 0x90
.Ltop:	mov	%r10, (%rdi,%r11,8)
	add	%rax, %r9
	mov	(%rsi,%r11,8), %rax
	adc	%rdx, %r8
	mov	$0, %r10d
.LL1:	mul	%rcx
	mov	%r9, 8(%rdi,%r11,8)
	add	%rax, %r8
	adc	%rdx, %rbx
.LL0:	mov	8(%rsi,%r11,8), %rax
	mul	%rcx
	mov	%r8, 16(%rdi,%r11,8)
	add	%rax, %rbx
	adc	%rdx, %r10
.LL3:	mov	16(%rsi,%r11,8), %rax
	mul	%rcx
	mov	%rbx, 24(%rdi,%r11,8)
	mov	$0, %r8d		
	mov	%r8, %rbx		
	add	%rax, %r10
	mov	24(%rsi,%r11,8), %rax
	mov	%r8, %r9		
	adc	%rdx, %r9
.LL2:	mul	%rcx
	add	$4, %r11
	js	 .Ltop

	mov	%r10, (%rdi,%r11,8)
	add	%rax, %r9
	adc	%r8, %rdx
	mov	%r9, 8(%rdi,%r11,8)
	add	%r8, %rdx
.Lret:	mov	%rdx, %rax

	pop	%rbx


	ret
	.size	__gmpn_mul_1,.-__gmpn_mul_1
