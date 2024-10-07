























































































	.text
	.align	32, 0x90
	.globl	__gmpn_mul_2
	.type	__gmpn_mul_2,@function
	
__gmpn_mul_2:

	
	push	%rbx
	push	%rbp

	mov	(%rcx), %r8
	mov	8(%rcx), %r9

	lea	3(%rdx), %r11
	shr	$2, %r11

	test	$1, %dl
	jnz	.Lbx1

.Lbx0:	xor	%rbx, %rbx
	test	$2, %dl
	mov	(%rsi), %rdx
	.byte	0xc4,194,211,0xf6,200
	jz	.Llo0

.Lb10:	lea	-16(%rdi), %rdi
	lea	-16(%rsi), %rsi
	jmp	.Llo2

.Lbx1:	xor	%rbp, %rbp
	test	$2, %dl
	mov	(%rsi), %rdx
	.byte	0xc4,66,227,0xf6,208
	jnz	.Lb11

.Lb01:	lea	-24(%rdi), %rdi
	lea	8(%rsi), %rsi
	jmp	.Llo1

.Lb11:	lea	-8(%rdi), %rdi
	lea	-8(%rsi), %rsi
	jmp	.Llo3

	.align	16, 0x90
.Ltop:	.byte	0xc4,194,251,0xf6,217
	add	%rax, %rbp		
	mov	(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,200
	adc	$0, %rbx			
	add	%rax, %rbp		
	adc	$0, %rcx			
	add	%r10, %rbp			
.Llo0:	mov	%rbp, (%rdi)		
	adc	$0, %rcx			
	.byte	0xc4,194,251,0xf6,233
	add	%rax, %rbx		
	mov	8(%rsi), %rdx
	adc	$0, %rbp			
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %rbx		
	adc	$0, %r10			
	add	%rcx, %rbx			
.Llo3:	mov	%rbx, 8(%rdi)		
	adc	$0, %r10			
	.byte	0xc4,194,251,0xf6,217
	add	%rax, %rbp		
	mov	16(%rsi), %rdx
	.byte	0xc4,194,251,0xf6,200
	adc	$0, %rbx			
	add	%rax, %rbp		
	adc	$0, %rcx			
	add	%r10, %rbp			
.Llo2:	mov	%rbp, 16(%rdi)		
	adc	$0, %rcx			
	.byte	0xc4,194,251,0xf6,233
	add	%rax, %rbx		
	mov	24(%rsi), %rdx
	adc	$0, %rbp			
	.byte	0xc4,66,251,0xf6,208
	add	%rax, %rbx		
	adc	$0, %r10			
	add	%rcx, %rbx			
	lea	32(%rsi), %rsi
.Llo1:	mov	%rbx, 24(%rdi)		
	adc	$0, %r10			
	dec	%r11
	lea	32(%rdi), %rdi
	jnz	.Ltop

.Lend:	.byte	0xc4,194,235,0xf6,193
	add	%rdx, %rbp
	adc	$0, %rax
	add	%r10, %rbp
	mov	%rbp, (%rdi)
	adc	$0, %rax

	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_mul_2,.-__gmpn_mul_2
