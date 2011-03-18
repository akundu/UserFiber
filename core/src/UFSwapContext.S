#if __WORDSIZE == 64
.text
	.align 4
.globl uf_swapcontext
	.type	 uf_swapcontext,@function

uf_swapcontext:
        // store
	movq    %r15, 0x60(%rdi)
	movq    %r14, 0x58(%rdi)
	movq    %r13, 0x50(%rdi)
	movq    %r12, 0x48(%rdi)
	movq    %r9, 0x30(%rdi)
	movq    %r8, 0x28(%rdi)
	movq    %rdi, 0x68(%rdi)
	movq    %rsi, 0x70(%rdi)
        movq    %rbp, 0x78(%rdi)
        movq    %rbx, 0x80(%rdi)
	movq    %rdx, 0x88(%rdi)
	movq    %rcx, 0x98(%rdi)
        
	movq    (%rsp), %rcx
	movq    %rcx, 0xa8(%rdi)
	leaq    0x8(%rsp), %rcx
	movq    %rcx, 0xa0(%rdi)
        
	leaq    0x1a8(%rdi), %rcx
	movq    %rcx, 0xe0(%rdi)
        
	fnstenv (%rcx)
	stmxcsr 0x1c0(%rdi)
        
        // restore
        movq    %rsi, %rdi
        movq    0xe0(%rdi), %rcx
        fldenv  (%rcx)
        ldmxcsr 0x1c0(%rdi)

        movq    0x60(%rdi), %r15
        movq    0x58(%rdi), %r14
        movq    0x50(%rdi), %r13
        movq    0x48(%rdi), %r12
        movq    0x30(%rdi), %r9
        movq    0x28(%rdi), %r8
        movq    0x70(%rdi), %rsi
        movq    0x78(%rdi), %rbp
        movq    0x80(%rdi), %rbx
        movq    0x88(%rdi), %rdx
        movq    0x98(%rdi), %rcx
        
        movq    0xa0(%rdi), %rsp
        movq    0xa8(%rdi), %rcx
        pushq   %rcx
        
        movq    0x68(%rdi), %rdi
        
        xorl    %eax, %eax
        ret
#endif
