/* RAFCODEPHI opt-in raw Linux/Android syscall adapter, no libc.
 * Not installed in PRoot and not interchangeable with libc ptrace():
 * - returns raw kernel values (negative errno is NOT normalized),
 * - no ABI-specific PEEK translation or tracing-policy bypass,
 * - caller owns validated pointers and all authorization.
 * ARMv7 uses A32 instructions, not Thumb.
 */
#if defined(__aarch64__)
long raf_proot_raw_ptrace(long request, long pid, long address, long data)
{
    register long x0 __asm__("x0") = request;
    register long x1 __asm__("x1") = pid;
    register long x2 __asm__("x2") = address;
    register long x3 __asm__("x3") = data;
    register long x8 __asm__("x8") = 117L; /* __NR_ptrace, AArch64 */
    __asm__ __volatile__(".inst 0xd4000001" : "+r"(x0)
                        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
                        : "memory", "cc");
    return x0;
}
#elif defined(__arm__) && !defined(__thumb__)
long raf_proot_raw_ptrace(long request, long pid, long address, long data)
{
    register long r0 __asm__("r0") = request;
    register long r1 __asm__("r1") = pid;
    register long r2 __asm__("r2") = address;
    register long r3 __asm__("r3") = data;
    register long r7 __asm__("r7") = 26L; /* __NR_ptrace, ARM EABI */
    __asm__ __volatile__(".inst 0xef000000" : "+r"(r0)
                        : "r"(r1), "r"(r2), "r"(r3), "r"(r7)
                        : "memory", "cc");
    return r0;
}
#else
#error "raw PRoot adapter supports only ARMv7 A32 and AArch64"
#endif
