// We want to use __builtin_setjmp but clang has a longstanding bug that causes
// __builtin_setjmp to store a corrupt frame pointer. Until that is fixed we
// write our own. See https://github.com/llvm/llvm-project/pull/186843 for
// details.

__attribute__((naked,returns_twice))
int windows_setjmp(void** buf)
{
  // We store the frame pointer, instruction pointer and stack pointer into the
  // buffer. Note that we can keep using __builtin_longjmp because we store
  // exactly what it expects.
    __asm(
          "mov %rbp, 0(%rcx)\n"
          "mov (%rsp), %rax\n"
          "mov %rax,  8(%rcx)\n"
          "lea 8(%rsp), %rax\n"
          "mov %rax,  16(%rcx)\n"
          "xor %eax, %eax\n"
          "ret\n"
    );
}
