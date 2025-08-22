































































	.text
	.align	32, 0x90
	.globl	__gmpn_lshiftc
	
	.def	__gmpn_lshiftc
	.scl	2
	.type	32
	.endef
__gmpn_lshiftc:

	push	%rdi
	push	%rsi
	mov	%rcx, %rdi
	mov	%rdx, %rsi
	mov	%r8, %rdx
	mov	%r9, %rcx

	neg	%ecx		
	mov	-8(%rsi,%rdx,8), %rax
	shr	%cl, %rax		

	neg	%ecx		
	lea	1(%rdx), %r8d
	and	$3, %r8d
	je	Lrlx			

	dec	%r8d
	jne	L1

	mov	-8(%rsi,%rdx,8), %r10
	shl	%cl, %r10
	neg	%ecx		
	mov	-16(%rsi,%rdx,8), %r8
	shr	%cl, %r8
	or	%r8, %r10
	not	%r10
	mov	%r10, -8(%rdi,%rdx,8)
	dec	%rdx
	jmp	Lrll

L1:	dec	%r8d
	je	L1x			

	mov	-8(%rsi,%rdx,8), %r10
	shl	%cl, %r10
	neg	%ecx		
	mov	-16(%rsi,%rdx,8), %r8
	shr	%cl, %r8
	or	%r8, %r10
	not	%r10
	mov	%r10, -8(%rdi,%rdx,8)
	dec	%rdx
	neg	%ecx		
L1x:
	cmp	$1, %rdx
	je	Last
	mov	-8(%rsi,%rdx,8), %r10
	shl	%cl, %r10
	mov	-16(%rsi,%rdx,8), %r11
	shl	%cl, %r11
	neg	%ecx		
	mov	-16(%rsi,%rdx,8), %r8
	mov	-24(%rsi,%rdx,8), %r9
	shr	%cl, %r8
	or	%r8, %r10
	shr	%cl, %r9
	or	%r9, %r11
	not	%r10
	not	%r11
	mov	%r10, -8(%rdi,%rdx,8)
	mov	%r11, -16(%rdi,%rdx,8)
	sub	$2, %rdx

Lrll:	neg	%ecx		
Lrlx:	mov	-8(%rsi,%rdx,8), %r10
	shl	%cl, %r10
	mov	-16(%rsi,%rdx,8), %r11
	shl	%cl, %r11

	sub	$4, %rdx			
	jb	Lend			
	.align	16, 0x90
Ltop:
	
	neg	%ecx		
	mov	16(%rsi,%rdx,8), %r8
	mov	8(%rsi,%rdx,8), %r9
	shr	%cl, %r8
	or	%r8, %r10
	shr	%cl, %r9
	or	%r9, %r11
	not	%r10
	not	%r11
	mov	%r10, 24(%rdi,%rdx,8)
	mov	%r11, 16(%rdi,%rdx,8)
	
	mov	0(%rsi,%rdx,8), %r8
	mov	-8(%rsi,%rdx,8), %r9
	shr	%cl, %r8
	shr	%cl, %r9

	
	neg	%ecx		
	mov	8(%rsi,%rdx,8), %r10
	mov	0(%rsi,%rdx,8), %r11
	shl	%cl, %r10
	or	%r10, %r8
	shl	%cl, %r11
	or	%r11, %r9
	not	%r8
	not	%r9
	mov	%r8, 8(%rdi,%rdx,8)
	mov	%r9, 0(%rdi,%rdx,8)
	
	mov	-8(%rsi,%rdx,8), %r10
	mov	-16(%rsi,%rdx,8), %r11
	shl	%cl, %r10
	shl	%cl, %r11

	sub	$4, %rdx
	jae	Ltop			
Lend:
	neg	%ecx		
	mov	8(%rsi), %r8
	shr	%cl, %r8
	or	%r8, %r10
	mov	(%rsi), %r9
	shr	%cl, %r9
	or	%r9, %r11
	not	%r10
	not	%r11
	mov	%r10, 16(%rdi)
	mov	%r11, 8(%rdi)

	neg	%ecx		
Last:	mov	(%rsi), %r10
	shl	%cl, %r10
	not	%r10
	mov	%r10, (%rdi)
	pop	%rsi
	pop	%rdi
	ret
	
