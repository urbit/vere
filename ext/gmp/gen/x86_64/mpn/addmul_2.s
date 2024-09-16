






















































































	.text
	.align	32, 0x90
	.globl	__gmpn_addmul_2
	.type	__gmpn_addmul_2,@function
	
__gmpn_addmul_2:

	
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13

	mov	(%rcx), %r8
	mov	8(%rcx), %r9

	mov	%rdx, %r11
	shr	$2, %r11

	test	$1, %dl
	jnz	.Lbx1

.Lbx0:	mov	(%rdi), %r12
	mov	8(%rdi), %r13
	test	$2, %dl
	jnz	.Lb10

.Lb00:	mov	(%rsi), %rdx
	lea	16(%rsi), %rsi
	.byte	0xc4,194,251,0xf6,200
	add	%rax, %r12
	.byte	0xc4,194,251,0xf6,233
	adc	$0, %rcx
	mov	%r12, (%rdi)
	add	%rax, %r13
	adc	$0, %rbp
	mov	-8(%rsi), %rdx
	lea	16(%rdi), %rdi
	jmp	.Llo0

.Lb10:	mov	(%rsi), %rdx
	inc	%r11
	.byte	0xc4,194,251,0xf6,200
	add	%rax, %r12
	adc	$0, %rcx
	.byte	0xc4,194,251,0xf6,233
	mov	%r12, (%rdi)
	mov	16(%rdi), %r12
	add	%rax, %r13
	adc	$0, %rbp
	xor	%rbx, %rbx
	jmp	.Llo2

.Lbx1:	mov	(%rdi), %r13
	mov	8(%rdi), %r12
	test	$2, %dl
	jnz	.Lb11

.Lb01:	mov	(%rsi), %rdx
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r13
	adc	$0, %r10
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r12
	adc	$0, %rbx
	mov	8(%rsi), %rdx
	mov	%r13, (%rdi)
	mov	16(%rdi), %r13
	.byte	0xc4,194,251,0xf6,200
	lea	24(%rdi), %rdi
	lea	24(%rsi), %rsi
	jmp	.Llo1

.Lb11:	mov	(%rsi), %rdx
	inc	%r11
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r13
	adc	$0, %r10
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r12
	adc	$0, %rbx
	mov	%r13, (%rdi)
	mov	8(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,200
	lea	8(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	.Llo3

	.align	16, 0x90
.Ltop:	.byte	0xc4,66,251,0xf6,208
	add	%rbx, %r13
	adc	$0, %rbp
	add	%rax, %r13
	adc	$0, %r10
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r12
	adc	$0, %rbx
	lea	32(%rdi), %rdi
	add	%rcx, %r13
	mov	-16(%rsi), %rdx
	mov	%r13, -24(%rdi)
	adc	$0, %r10
	add	%rbp, %r12
	mov	-8(%rdi), %r13
	.byte	0xc4,194,251,0xf6,200
	adc	$0, %rbx
.Llo1:	add	%rax, %r12
	.byte	0xc4,194,251,0xf6,233
	adc	$0, %rcx
	add	%r10, %r12
	mov	%r12, -16(%rdi)
	adc	$0, %rcx
	add	%rax, %r13
	adc	$0, %rbp
	add	%rbx, %r13
	mov	-8(%rsi), %rdx
	adc	$0, %rbp
.Llo0:	.byte	0xc4,66,251,0xf6,208
	add	%rax, %r13
	adc	$0, %r10
	mov	(%rdi), %r12
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %r12
	adc	$0, %rbx
	add	%rcx, %r13
	mov	%r13, -8(%rdi)
	adc	$0, %r10
	mov	(%rsi), %rdx
	add	%rbp, %r12
	.byte	0xc4,194,251,0xf6,200
	adc	$0, %rbx
.Llo3:	add	%rax, %r12
	adc	$0, %rcx
	.byte	0xc4,194,251,0xf6,233
	add	%r10, %r12
	mov	8(%rdi), %r13
	mov	%r12, (%rdi)
	mov	16(%rdi), %r12
	adc	$0, %rcx
	add	%rax, %r13
	adc	$0, %rbp
.Llo2:	mov	8(%rsi), %rdx
	lea	32(%rsi), %rsi
	dec	%r11
	jnz	.Ltop

.Lend:	.byte	0xc4,66,251,0xf6,208
	add	%rbx, %r13
	adc	$0, %rbp
	add	%rax, %r13
	adc	$0, %r10
	.byte	0xc4,194,235,0xf6,193
	add	%rcx, %r13
	mov	%r13, 8(%rdi)
	adc	$0, %r10
	add	%rbp, %rdx
	adc	$0, %rax
	add	%r10, %rdx
	mov	%rdx, 16(%rdi)
	adc	$0, %rax

	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_addmul_2,.-__gmpn_addmul_2
