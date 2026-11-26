/*
 * RAFCODEPHI API29 compatibility implementation for package-linked ELF objects.
 * Compile into each affected source package only for RAFCODEPHI/aarch64/API29.
 * This is NOT a substitute for a physical Android 10 / kernel-policy test.
 * SPDX-License-Identifier: MIT
 */
#if !defined(__ANDROID__) && !defined(RAFCODEPHI_MEMFD29_HOST_TEST)
# error "RAFCODEPHI memfd29 shim is Android-only (or its explicit host fixture)"
#endif
#if defined(__ANDROID__) && (!defined(__ANDROID_API__) || __ANDROID_API__ != 29)
# error "RAFCODEPHI memfd29 shim must be built for Android API29"
#endif

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_memfd_create
# error "No verified memfd_create syscall number on this toolchain"
#endif

/* --wrap=memfd_create redirects undefined references at link time. Bionic
 * only exports the libc entrypoint on newer Android releases; the kernel
 * syscall itself exists on Android 10 Linux kernels, subject to seccomp.
 * syscall(2) returns -1 and preserves errno when blocked or unavailable.
 */
int __wrap_memfd_create(const char *name, unsigned int flags) {
    return (int)syscall(__NR_memfd_create, name, flags);
}
