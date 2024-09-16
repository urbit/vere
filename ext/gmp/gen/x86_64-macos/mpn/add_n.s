




































































	
	
	
	
	


	
	
	








	.text
	.align	4, 0x90
	.globl	___gmpn_add_nc
	
	
___gmpn_add_nc:

	


	mov	%ecx, %eax
	shr	$3, %rcx
	and	$7, %eax

	lea	Ltab(%rip), %r9
	neg	%r8			

	movslq	(%r9,%rax,4), %rax
	lea	(%r9,%rax), %rax	
	jmp	*%rax

	

	.align	4, 0x90
	.globl	___gmpn_add_n
	
	
___gmpn_add_n:

	

	mov	%ecx, %eax
	shr	$3, %rcx
	and	$7, %eax		

	lea	Ltab(%rip), %r9

	movslq	(%r9,%rax,4), %rax
	lea	(%r9,%rax), %rax	
	jmp	*%rax


L0:	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	adc	(%rdx), %r8
	jmp	Le0

L4:	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	adc	(%rdx), %r8
	lea	-32(%rsi), %rsi
	lea	-32(%rdx), %rdx
	lea	-32(%rdi), %rdi
	inc	%rcx
	jmp	Le4

L5:	mov	(%rsi), %r11
	mov	8(%rsi), %r8
	mov	16(%rsi), %r9
	adc	(%rdx), %r11
	lea	-24(%rsi), %rsi
	lea	-24(%rdx), %rdx
	lea	-24(%rdi), %rdi
	inc	%rcx
	jmp	Le5

L6:	mov	(%rsi), %r10
	adc	(%rdx), %r10
	mov	8(%rsi), %r11
	lea	-16(%rsi), %rsi
	lea	-16(%rdx), %rdx
	lea	-16(%rdi), %rdi
	inc	%rcx
	jmp	Le6

L7:	mov	(%rsi), %r9
	mov	8(%rsi), %r10
	adc	(%rdx), %r9
	adc	8(%rdx), %r10
	lea	-8(%rsi), %rsi
	lea	-8(%rdx), %rdx
	lea	-8(%rdi), %rdi
	inc	%rcx
	jmp	Le7

	.align	4, 0x90
Ltop:
Le3:	mov	%r9, 40(%rdi)
Le2:	mov	%r10, 48(%rdi)
Le1:	mov	(%rsi), %r8
	mov	8(%rsi), %r9
	adc	(%rdx), %r8
	mov	%r11, 56(%rdi)
	lea	64(%rdi), %rdi
Le0:	mov	16(%rsi), %r10
	adc	8(%rdx), %r9
	adc	16(%rdx), %r10
	mov	%r8, (%rdi)
Le7:	mov	24(%rsi), %r11
	mov	%r9, 8(%rdi)
Le6:	mov	32(%rsi), %r8
	mov	40(%rsi), %r9
	adc	24(%rdx), %r11
	mov	%r10, 16(%rdi)
Le5:	adc	32(%rdx), %r8
	mov	%r11, 24(%rdi)
Le4:	mov	48(%rsi), %r10
	mov	56(%rsi), %r11
	mov	%r8, 32(%rdi)
	lea	64(%rsi), %rsi
	adc	40(%rdx), %r9
	adc	48(%rdx), %r10
	adc	56(%rdx), %r11
	lea	64(%rdx), %rdx
	dec	%rcx
	jnz	Ltop

Lend:	mov	%r9, 40(%rdi)
	mov	%r10, 48(%rdi)
	mov	%r11, 56(%rdi)
	mov	%ecx, %eax
	adc	%ecx, %eax
	
	ret

	.align	4, 0x90
L3:	mov	(%rsi), %r9
	mov	8(%rsi), %r10
	mov	16(%rsi), %r11
	adc	(%rdx), %r9
	adc	8(%rdx), %r10
	adc	16(%rdx), %r11
	jrcxz	Lx3
	lea	24(%rsi), %rsi
	lea	24(%rdx), %rdx
	lea	-40(%rdi), %rdi
	jmp	Le3
Lx3:	mov	%r9, (%rdi)
	mov	%r10, 8(%rdi)
	mov	%r11, 16(%rdi)
	mov	%ecx, %eax
	adc	%ecx, %eax
	
	ret

	.align	4, 0x90
L1:	mov	(%rsi), %r11
	adc	(%rdx), %r11
	jrcxz	Lx1
	lea	8(%rsi), %rsi
	lea	8(%rdx), %rdx
	lea	-56(%rdi), %rdi
	jmp	Le1
Lx1:	mov	%r11, (%rdi)
	mov	%ecx, %eax
	adc	%ecx, %eax
	
	ret

	.align	4, 0x90
L2:	mov	(%rsi), %r10
	mov	8(%rsi), %r11
	adc	(%rdx), %r10
	adc	8(%rdx), %r11
	jrcxz	Lx2
	lea	16(%rsi), %rsi
	lea	16(%rdx), %rdx
	lea	-48(%rdi), %rdi
	jmp	Le2
Lx2:	mov	%r10, (%rdi)
	mov	%r11, 8(%rdi)
	mov	%ecx, %eax
	adc	%ecx, %eax
	
	ret
	
	.text
	.align	3, 0x90
Ltab:	.set	L0_tmp, L0-Ltab
	.long	L0_tmp

	.set	L1_tmp, L1-Ltab
	.long	L1_tmp

	.set	L2_tmp, L2-Ltab
	.long	L2_tmp

	.set	L3_tmp, L3-Ltab
	.long	L3_tmp

	.set	L4_tmp, L4-Ltab
	.long	L4_tmp

	.set	L5_tmp, L5-Ltab
	.long	L5_tmp

	.set	L6_tmp, L6-Ltab
	.long	L6_tmp

	.set	L7_tmp, L7-Ltab
	.long	L7_tmp

