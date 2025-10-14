




































































  
  
  
























	.text
	.align	32, 0x90
	.globl	__gmpn_andn_n
	.type	__gmpn_andn_n,@function
	
__gmpn_andn_n:

	
	mov	(%rdx), %r8
	not	%r8
	mov	%ecx, %eax
	lea	(%rdx,%rcx,8), %rdx
	lea	(%rsi,%rcx,8), %rsi
	lea	(%rdi,%rcx,8), %rdi
	neg	%rcx
	and	$3, %eax
	je	.Lb00
	cmp	$2, %eax
	jc	.Lb01
	je	.Lb10

.Lb11:	and	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	dec	%rcx
	jmp	.Le11
.Lb10:	add	$-2, %rcx
	jmp	.Le10
	.byte	0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90
.Lb01:	and	(%rsi,%rcx,8), %r8
	mov	%r8, (%rdi,%rcx,8)
	inc	%rcx
	jz	.Lret

.Ltop:	mov	(%rdx,%rcx,8), %r8
	not	%r8
.Lb00:	mov	8(%rdx,%rcx,8), %r9
	not	%r9
	and	(%rsi,%rcx,8), %r8
	and	8(%rsi,%rcx,8), %r9
	mov	%r8, (%rdi,%rcx,8)
	mov	%r9, 8(%rdi,%rcx,8)
.Le11:	mov	16(%rdx,%rcx,8), %r8
	not	%r8
.Le10:	mov	24(%rdx,%rcx,8), %r9
	not	%r9
	and	16(%rsi,%rcx,8), %r8
	and	24(%rsi,%rcx,8), %r9
	mov	%r8, 16(%rdi,%rcx,8)
	mov	%r9, 24(%rdi,%rcx,8)
	add	$4, %rcx
	jnc	.Ltop

.Lret:	
	ret
	.size	__gmpn_andn_n,.-__gmpn_andn_n



