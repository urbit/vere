








































































				
		


	.text
	.align	16, 0x90
	.globl	__gmpn_addmul_1
	.type	__gmpn_addmul_1,@function
	
__gmpn_addmul_1:

	mov	(%rsi), %r8

	push	%rbx
	push	%r12
	push	%r13

	mov	%rdx, %rax
	mov	%rcx, %rdx
	mov	%rax, %rcx

	and	$3, %al
	jz	.Lb0
	cmp	$2, %al
	jl	.Lb1
	jz	.Lb2

.Lb3:	.byte	0xc4,66,163,0xf6,208
	.byte	0xc4,98,147,0xf6,102,8
	.byte	0xc4,226,227,0xf6,70,16
	inc	%rcx
	lea	-8(%rsi), %rsi
	lea	-24(%rdi), %rdi
	jmp	.Llo3

.Lb0:	.byte	0xc4,66,179,0xf6,192
	.byte	0xc4,98,163,0xf6,86,8
	.byte	0xc4,98,147,0xf6,102,16
	lea	-16(%rdi), %rdi
	jmp	.Llo0

.Lb2:	.byte	0xc4,66,147,0xf6,224
	.byte	0xc4,226,227,0xf6,70,8
	lea	-2(%rcx), %rcx
	jrcxz	.Ln2
	.byte	0xc4,98,179,0xf6,70,16
	lea	16(%rsi), %rsi
	jmp	.Llo2
.Ln2:	jmp	.Lwd2

.Lb1:	.byte	0xc4,194,227,0xf6,192
	sub	$1, %rcx
	jrcxz	.Ln1
	.byte	0xc4,98,179,0xf6,70,8
	.byte	0xc4,98,163,0xf6,86,16
	lea	8(%rsi), %rsi
	lea	-8(%rdi), %rdi
	jmp	.Llo1
.Ln1:	add	(%rdi), %rbx
	adc	%rcx, %rax
	mov	%rbx, (%rdi)
	pop	%r13
	pop	%r12
	pop	%rbx
	ret

.Ltop:	.byte	0xc4,98,179,0xf6,6
	.byte	0x66,77,0x0f,0x38,0xf6,234
	mov	%r11, -8(%rdi)
.Llo2:	.byte	0xf3,76,0x0f,0x38,0xf6,47
	.byte	0xc4,98,163,0xf6,86,8
	.byte	0x66,73,0x0f,0x38,0xf6,220
	mov	%r13, (%rdi)
.Llo1:	.byte	0xf3,72,0x0f,0x38,0xf6,95,8
	.byte	0xc4,98,147,0xf6,102,16
	.byte	0x66,76,0x0f,0x38,0xf6,200
	mov	%rbx, 8(%rdi)
.Llo0:	.byte	0xf3,76,0x0f,0x38,0xf6,79,16
	.byte	0xc4,226,227,0xf6,70,24
	.byte	0x66,77,0x0f,0x38,0xf6,216
	mov	%r9, 16(%rdi)
.Llo3:	.byte	0xf3,76,0x0f,0x38,0xf6,95,24
	lea	32(%rsi), %rsi
	lea	32(%rdi), %rdi
	lea	-4(%rcx), %rcx
	jrcxz	.Lend
	jmp	.Ltop

.Lend:	.byte	0x66,77,0x0f,0x38,0xf6,234
	mov	%r11, -8(%rdi)
.Lwd2:	.byte	0xf3,76,0x0f,0x38,0xf6,47
	.byte	0x66,73,0x0f,0x38,0xf6,220
	mov	%r13, (%rdi)
	.byte	0xf3,72,0x0f,0x38,0xf6,95,8
	.byte	0x66,72,0x0f,0x38,0xf6,193
	.byte	0xf3,72,0x0f,0x38,0xf6,193
	mov	%rbx, 8(%rdi)
	pop	%r13
	pop	%r12
	pop	%rbx
	ret
	.size	__gmpn_addmul_1,.-__gmpn_addmul_1

