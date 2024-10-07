





























































































  
  








	.text
	.align	32, 0x90
	.globl	__gmpn_rsblsh_n
	.type	__gmpn_rsblsh_n,@function
	
__gmpn_rsblsh_n:

	


	mov	(%rdx), %r10

	mov	%ecx, %eax
	shr	$3, %rcx
	xor	%r9d, %r9d
	sub	%r8, %r9
	and	$7, %eax

	lea	.Ltab(%rip), %r11

	movslq	(%r11,%rax,4), %rax
	add	%r11, %rax
	jmp	*%rax


.L0:	lea	32(%rsi), %rsi
	lea	32(%rdx), %rdx
	lea	32(%rdi), %rdi
	xor	%r11d, %r11d
	jmp	.Le0

.L7:	mov	%r10, %r11
	lea	24(%rsi), %rsi
	lea	24(%rdx), %rdx
	lea	24(%rdi), %rdi
	xor	%r10d, %r10d
	jmp	.Le7

.L6:	lea	16(%rsi), %rsi
	lea	16(%rdx), %rdx
	lea	16(%rdi), %rdi
	xor	%r11d, %r11d
	jmp	.Le6

.L5:	mov	%r10, %r11
	lea	8(%rsi), %rsi
	lea	8(%rdx), %rdx
	lea	8(%rdi), %rdi
	xor	%r10d, %r10d
	jmp	.Le5

.Lend:	sbb	24(%rsi), %rax
	mov	%rax, -40(%rdi)
	.byte	0xc4,194,179,0xf7,195
	sbb	%rcx, %rax
	
	ret

	.align	32, 0x90
.Ltop:	jrcxz	.Lend
	mov	-32(%rdx), %r10
	sbb	24(%rsi), %rax
	lea	64(%rsi), %rsi
	.byte	0xc4,66,179,0xf7,219
	mov	%rax, -40(%rdi)
.Le0:	dec	%rcx
	.byte	0xc4,194,185,0xf7,194
	lea	(%r11,%rax), %rax
	mov	-24(%rdx), %r11
	sbb	-32(%rsi), %rax
	.byte	0xc4,66,179,0xf7,210
	mov	%rax, -32(%rdi)
.Le7:	.byte	0xc4,194,185,0xf7,195
	lea	(%r10,%rax), %rax
	mov	-16(%rdx), %r10
	sbb	-24(%rsi), %rax
	.byte	0xc4,66,179,0xf7,219
	mov	%rax, -24(%rdi)
.Le6:	.byte	0xc4,194,185,0xf7,194
	lea	(%r11,%rax), %rax
	mov	-8(%rdx), %r11
	sbb	-16(%rsi), %rax
	.byte	0xc4,66,179,0xf7,210
	mov	%rax, -16(%rdi)
.Le5:	.byte	0xc4,194,185,0xf7,195
	lea	(%r10,%rax), %rax
	mov	(%rdx), %r10
	sbb	-8(%rsi), %rax
	.byte	0xc4,66,179,0xf7,219
	mov	%rax, -8(%rdi)
.Le4:	.byte	0xc4,194,185,0xf7,194
	lea	(%r11,%rax), %rax
	mov	8(%rdx), %r11
	sbb	(%rsi), %rax
	.byte	0xc4,66,179,0xf7,210
	mov	%rax, (%rdi)
.Le3:	.byte	0xc4,194,185,0xf7,195
	lea	(%r10,%rax), %rax
	mov	16(%rdx), %r10
	sbb	8(%rsi), %rax
	.byte	0xc4,66,179,0xf7,219
	mov	%rax, 8(%rdi)
.Le2:	.byte	0xc4,194,185,0xf7,194
	lea	(%r11,%rax), %rax
	mov	24(%rdx), %r11
	sbb	16(%rsi), %rax
	lea	64(%rdx), %rdx
	.byte	0xc4,66,179,0xf7,210
	mov	%rax, 16(%rdi)
	lea	64(%rdi), %rdi
.Le1:	.byte	0xc4,194,185,0xf7,195
	lea	(%r10,%rax), %rax
	jmp	.Ltop

.L4:	xor	%r11d, %r11d
	jmp	.Le4

.L3:	mov	%r10, %r11
	lea	-8(%rsi), %rsi
	lea	-8(%rdx), %rdx
	lea	-8(%rdi), %rdi
	xor	%r10d, %r10d
	jmp	.Le3

.L2:	lea	-16(%rsi), %rsi
	lea	-16(%rdx), %rdx
	lea	-16(%rdi), %rdi
	xor	%r11d, %r11d
	jmp	.Le2

.L1:	mov	%r10, %r11
	lea	-24(%rsi), %rsi
	lea	40(%rdx), %rdx
	lea	40(%rdi), %rdi
	xor	%r10d, %r10d
	jmp	.Le1
	.size	__gmpn_rsblsh_n,.-__gmpn_rsblsh_n
	.section	.data.rel.ro.local,"a",@progbits
	.align	8, 0x90
.Ltab:	.long	.L0-.Ltab
	.long	.L1-.Ltab
	.long	.L2-.Ltab
	.long	.L3-.Ltab
	.long	.L4-.Ltab
	.long	.L5-.Ltab
	.long	.L6-.Ltab
	.long	.L7-.Ltab

