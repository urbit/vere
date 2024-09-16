





































































   
   
   
   
    




  

    



	.text
	.align	16, 0x90
	.globl	__gmpn_redc_1
	.type	__gmpn_redc_1,@function
	
__gmpn_redc_1:

	

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rdi
	mov	%rdx, %rdi		
	mov	(%rsi), %rdx

	neg	%rcx
	push	%r8			
	imul	%r8, %rdx	
	mov	%rcx, %r15			

	test	$1, %cl
	jnz	.Lbx1

.Lbx0:	test	$2, %cl
	jz	.Lo0b

	cmp	$-2, %ecx
	jnz	.Lo2


	mov	8(%rsp), %rbx		
	lea	16(%rsp), %rsp		
	.byte	0xc4,98,179,0xf6,39
	.byte	0xc4,98,163,0xf6,87,8
	add	%r12, %r11
	adc	$0, %r10
	add	(%rsi), %r9		
	adc	8(%rsi), %r11		
	adc	$0, %r10		
	mov	%r11, %rdx
	imul	%r8, %rdx
	.byte	0xc4,98,147,0xf6,39
	.byte	0xc4,98,139,0xf6,127,8
	xor	%eax, %eax
	add	%r12, %r14
	adc	$0, %r15
	add	%r11, %r13		
	adc	16(%rsi), %r14		
	adc	$0, %r15		
	add	%r14, %r10
	adc	24(%rsi), %r15
	mov	%r10, (%rbx)
	mov	%r15, 8(%rbx)
	setc	%al
	jmp	.Lret

.Lo2:	lea	2(%rcx), %r14			
	.byte	0xc4,98,179,0xf6,7
	.byte	0xc4,98,163,0xf6,87,8
	sar	$2, %r14
	add	%r8, %r11
	jmp	.Llo2

	.align	16, 0x90
.Ltp2:	adc	%rax, %r9
	lea	32(%rsi), %rsi
	adc	%r8, %r11
.Llo2:	.byte	0xc4,98,147,0xf6,103,16
	mov	(%rsi), %r8
	.byte	0xc4,226,227,0xf6,71,24
	lea	32(%rdi), %rdi
	adc	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	8(%rsi), %r10
	mov	16(%rsi), %r12
	add	%r9, %r8
	mov	24(%rsi), %rbp
	mov	%r8, (%rsi)
	adc	%r11, %r10
	.byte	0xc4,98,179,0xf6,7
	mov	%r10, 8(%rsi)
	adc	%r13, %r12
	mov	%r12, 16(%rsi)
	adc	%rbx, %rbp
	.byte	0xc4,98,163,0xf6,87,8
	mov	%rbp, 24(%rsi)
	inc	%r14
	jnz	.Ltp2

.Led2:	mov	56(%rsi,%rcx,8), %rdx	
	lea	16(%rdi,%rcx,8), %rdi		
	adc	%rax, %r9
	adc	%r8, %r11
	mov	32(%rsi), %r8
	adc	$0, %r10
	imul	(%rsp), %rdx		
	mov	40(%rsi), %rax
	add	%r9, %r8
	mov	%r8, 32(%rsi)
	adc	%r11, %rax
	mov	%rax, 40(%rsi)
	lea	56(%rsi,%rcx,8), %rsi		
	adc	$0, %r10
	mov	%r10, -8(%rsi)
	inc	%r15
	jnz	.Lo2

	jmp	.Lcj


.Lbx1:	test	$2, %cl
	jz	.Lo3a

.Lo1a:	cmp	$-1, %ecx
	jnz	.Lo1b


	mov	8(%rsp), %rbx		
	lea	16(%rsp), %rsp		
	.byte	0xc4,98,163,0xf6,23
	add	(%rsi), %r11
	adc	8(%rsi), %r10
	mov	%r10, (%rbx)
	mov	$0, %eax
	setc	%al
	jmp	.Lret

.Lo1b:	lea	24(%rdi), %rdi
.Lo1:	lea	1(%rcx), %r14			
	.byte	0xc4,98,163,0xf6,87,232
	.byte	0xc4,98,147,0xf6,103,240
	.byte	0xc4,226,227,0xf6,71,248
	sar	$2, %r14
	add	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	(%rsi), %r10
	mov	8(%rsi), %r12
	mov	16(%rsi), %rbp
	add	%r11, %r10
	jmp	.Llo1

	.align	16, 0x90
.Ltp1:	adc	%rax, %r9
	lea	32(%rsi), %rsi
	adc	%r8, %r11
	.byte	0xc4,98,147,0xf6,103,16
	mov	-8(%rsi), %r8
	.byte	0xc4,226,227,0xf6,71,24
	lea	32(%rdi), %rdi
	adc	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	(%rsi), %r10
	mov	8(%rsi), %r12
	add	%r9, %r8
	mov	16(%rsi), %rbp
	mov	%r8, -8(%rsi)
	adc	%r11, %r10
.Llo1:	.byte	0xc4,98,179,0xf6,7
	mov	%r10, (%rsi)
	adc	%r13, %r12
	mov	%r12, 8(%rsi)
	adc	%rbx, %rbp
	.byte	0xc4,98,163,0xf6,87,8
	mov	%rbp, 16(%rsi)
	inc	%r14
	jnz	.Ltp1

.Led1:	mov	48(%rsi,%rcx,8), %rdx	
	lea	40(%rdi,%rcx,8), %rdi		
	adc	%rax, %r9
	adc	%r8, %r11
	mov	24(%rsi), %r8
	adc	$0, %r10
	imul	(%rsp), %rdx		
	mov	32(%rsi), %rax
	add	%r9, %r8
	mov	%r8, 24(%rsi)
	adc	%r11, %rax
	mov	%rax, 32(%rsi)
	lea	48(%rsi,%rcx,8), %rsi		
	adc	$0, %r10
	mov	%r10, -8(%rsi)
	inc	%r15
	jnz	.Lo1

	jmp	.Lcj

.Lo3a:	cmp	$-3, %ecx
	jnz	.Lo3b


.Ln3:	.byte	0xc4,226,227,0xf6,7
	.byte	0xc4,98,179,0xf6,119,8
	add	(%rsi), %rbx
	.byte	0xc4,98,163,0xf6,87,16
	adc	%rax, %r9		
	adc	%r14, %r11		
	mov	8(%rsi), %r14
	mov	%r8, %rdx
	adc	$0, %r10		
	mov	16(%rsi), %rax
	add	%r9, %r14		
	mov	%r14, 8(%rsi)
	.byte	0xc4,66,235,0xf6,238	
	adc	%r11, %rax		
	mov	%rax, 16(%rsi)
	adc	$0, %r10		
	mov	%r10, (%rsi)
	lea	8(%rsi), %rsi		
	inc	%r15
	jnz	.Ln3

	jmp	.Lcj

.Lo3b:	lea	8(%rdi), %rdi
.Lo3:	lea	4(%rcx), %r14			
	.byte	0xc4,226,227,0xf6,71,248
	.byte	0xc4,98,179,0xf6,7
	mov	(%rsi), %rbp
	.byte	0xc4,98,163,0xf6,87,8
	sar	$2, %r14
	add	%rbx, %rbp
	nop
	adc	%rax, %r9
	jmp	.Llo3

	.align	16, 0x90
.Ltp3:	adc	%rax, %r9
	lea	32(%rsi), %rsi
.Llo3:	adc	%r8, %r11
	.byte	0xc4,98,147,0xf6,103,16
	mov	8(%rsi), %r8
	.byte	0xc4,226,227,0xf6,71,24
	lea	32(%rdi), %rdi
	adc	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	16(%rsi), %r10
	mov	24(%rsi), %r12
	add	%r9, %r8
	mov	32(%rsi), %rbp
	mov	%r8, 8(%rsi)
	adc	%r11, %r10
	.byte	0xc4,98,179,0xf6,7
	mov	%r10, 16(%rsi)
	adc	%r13, %r12
	mov	%r12, 24(%rsi)
	adc	%rbx, %rbp
	.byte	0xc4,98,163,0xf6,87,8
	mov	%rbp, 32(%rsi)
	inc	%r14
	jnz	.Ltp3

.Led3:	mov	64(%rsi,%rcx,8), %rdx	
	lea	24(%rdi,%rcx,8), %rdi		
	adc	%rax, %r9
	adc	%r8, %r11
	mov	40(%rsi), %r8
	adc	$0, %r10
	imul	(%rsp), %rdx		
	mov	48(%rsi), %rax
	add	%r9, %r8
	mov	%r8, 40(%rsi)
	adc	%r11, %rax
	mov	%rax, 48(%rsi)
	lea	64(%rsi,%rcx,8), %rsi		
	adc	$0, %r10
	mov	%r10, -8(%rsi)
	inc	%r15
	jnz	.Lo3

	jmp	.Lcj

.Lo0b:	lea	16(%rdi), %rdi
.Lo0:	mov	%rcx, %r14			
	.byte	0xc4,98,147,0xf6,103,240
	.byte	0xc4,226,227,0xf6,71,248
	sar	$2, %r14
	add	%r12, %rbx
	adc	$0, %rax
	mov	(%rsi), %r12
	mov	8(%rsi), %rbp
	.byte	0xc4,98,179,0xf6,7
	add	%r13, %r12
	jmp	.Llo0

	.align	16, 0x90
.Ltp0:	adc	%rax, %r9
	lea	32(%rsi), %rsi
	adc	%r8, %r11
	.byte	0xc4,98,147,0xf6,103,16
	mov	-16(%rsi), %r8
	.byte	0xc4,226,227,0xf6,71,24
	lea	32(%rdi), %rdi
	adc	%r10, %r13
	adc	%r12, %rbx
	adc	$0, %rax
	mov	-8(%rsi), %r10
	mov	(%rsi), %r12
	add	%r9, %r8
	mov	8(%rsi), %rbp
	mov	%r8, -16(%rsi)
	adc	%r11, %r10
	.byte	0xc4,98,179,0xf6,7
	mov	%r10, -8(%rsi)
	adc	%r13, %r12
	mov	%r12, (%rsi)
.Llo0:	adc	%rbx, %rbp
	.byte	0xc4,98,163,0xf6,87,8
	mov	%rbp, 8(%rsi)
	inc	%r14
	jnz	.Ltp0

.Led0:	mov	40(%rsi,%rcx,8), %rdx	
	lea	32(%rdi,%rcx,8), %rdi		
	adc	%rax, %r9
	adc	%r8, %r11
	mov	16(%rsi), %r8
	adc	$0, %r10
	imul	(%rsp), %rdx		
	mov	24(%rsi), %rax
	add	%r9, %r8
	mov	%r8, 16(%rsi)
	adc	%r11, %rax
	mov	%rax, 24(%rsi)
	lea	40(%rsi,%rcx,8), %rsi		
	adc	$0, %r10
	mov	%r10, -8(%rsi)
	inc	%r15
	jnz	.Lo0

.Lcj:
	mov	8(%rsp), %rdi		
	lea	16-8(%rsp), %rsp	
	lea	(%rsi,%rcx,8), %rdx		
	neg	%ecx			

	

	
	call	__gmpn_add_n@PLT


	lea	8(%rsp), %rsp	


.Lret:	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	
	ret
	.size	__gmpn_redc_1,.-__gmpn_redc_1
