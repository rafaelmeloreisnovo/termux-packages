#ifndef REAL_ATTRS_H
#define REAL_ATTRS_H

/*
 * REAL: mini-block attribute directives (compiler → linker channel).
 * Status: REAL — every macro emits a real GCC/clang attribute or builtin.
 *
 * Each macro is a specialized "mini-block" that tells the compiler what
 * to tell the linker. Combined with -Wl,--gc-sections -flto they let the
 * linker drop code, collapse tails, and hide symbols per-function without
 * changing behavior.
 *
 * Effect summary:
 *   HIDDEN         — hide the symbol; not exported. GC can drop if unused.
 *   HOT / COLD     — steers code into .text.hot / .text.cold; branch layout.
 *   PURE / CONST   — enables CSE; loop-invariant motion; duplicate call fold.
 *   NORETURN       — no epilogue emitted; tail becomes bare jmp.
 *   MALLOC         — noalias return; enables alias-analysis eliminations.
 *   NONNULL(...)   — null-check elision on args.
 *   WARN_UNUSED    — forces callers to check return; catches dropped errors.
 *   UNUSED         — silences -Wunused without behavior change.
 *   LIKELY/UNLIKELY— reorders branches; hot path becomes fall-through.
 *   UNREACHABLE()  — eliminates the entire path (default: after switch).
 *   FLATTEN        — inlines all leaf calls into caller; kills call symbols.
 *   ALWAYS_INLINE  — force inline even at -O0.
 *   NOINLINE       — keep function out-of-line (useful for cold paths).
 *
 * These are compile-time only. They do not change runtime semantics; they
 * only change what the linker sees and can discard.
 */

#if defined(__GNUC__) || defined(__clang__)
#  define REAL_HIDDEN         __attribute__((visibility("hidden")))
#  define REAL_HOT            __attribute__((hot))
#  define REAL_COLD           __attribute__((cold))
#  define REAL_PURE           __attribute__((pure))
#  define REAL_CONST          __attribute__((const))
#  define REAL_NORETURN       __attribute__((noreturn))
#  define REAL_MALLOC         __attribute__((malloc))
#  define REAL_NONNULL(...)   __attribute__((nonnull(__VA_ARGS__)))
#  define REAL_NONNULL_ALL    __attribute__((nonnull))
#  define REAL_WARN_UNUSED    __attribute__((warn_unused_result))
#  define REAL_UNUSED         __attribute__((unused))
#  define REAL_USED           __attribute__((used))
#  define REAL_FLATTEN        __attribute__((flatten))
#  define REAL_ALWAYS_INLINE  __attribute__((always_inline)) inline
#  define REAL_NOINLINE       __attribute__((noinline))
#  define REAL_LIKELY(x)      __builtin_expect(!!(x), 1)
#  define REAL_UNLIKELY(x)    __builtin_expect(!!(x), 0)
#  define REAL_UNREACHABLE()  __builtin_unreachable()
#  define REAL_SECTION(name)  __attribute__((section(name)))
#  define REAL_ALIGNED(n)     __attribute__((aligned(n)))
#else
#  define REAL_HIDDEN
#  define REAL_HOT
#  define REAL_COLD
#  define REAL_PURE
#  define REAL_CONST
#  define REAL_NORETURN
#  define REAL_MALLOC
#  define REAL_NONNULL(...)
#  define REAL_NONNULL_ALL
#  define REAL_WARN_UNUSED
#  define REAL_UNUSED
#  define REAL_USED
#  define REAL_FLATTEN
#  define REAL_ALWAYS_INLINE  inline
#  define REAL_NOINLINE
#  define REAL_LIKELY(x)      (x)
#  define REAL_UNLIKELY(x)    (x)
#  define REAL_UNREACHABLE()  ((void)0)
#  define REAL_SECTION(name)
#  define REAL_ALIGNED(n)
#endif

#endif /* REAL_ATTRS_H */
