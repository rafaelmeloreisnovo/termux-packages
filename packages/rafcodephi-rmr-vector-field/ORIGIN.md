# RAFCODEPHI RMR Vector Field — Source Origin

This package intentionally transcribes a **minimal, byte-identical** freestanding
kernel from the RAFCODEPHI Vectras fork. It does not claim that the copied files
originated in `termux-packages`.

## Exact origin

- Origin-Repository: `rafaelmeloreisnovo/Vectras-VM-Android`
- Origin-Commit: `34aa3db434627c40cdc6fdb595591634e74d090d`
- Governing-License: `GPL-2.0-only`
- Parent-project attribution: original Vectras VM Android project by **xoureldeen**;
  the fork preserves that credit in its `LICENSE`.

| Origin path | Git blob | Copied path | File header credit |
|---|---|---|---|
| `engine/rmr/src/rmr_vector_field.c` | `8dfb230410b20e36236dbc3863d301e16a4c595d` | `files/rmr_vector_field.c` | `Copyright (C) Rafael M. R. — rafaelmeloreisnovo` |
| `engine/rmr/include/rmr_vector_field.h` | `10cb0ab97fcb59ca691f8b216ca9165231e6de74` | `files/rmr_vector_field.h` | same |
| `engine/rmr/include/rmr_types.h` | `433843be4b8a1c1a722bc814b63c366db3314cd6` | `files/rmr_types.h` | same |

## Local transformation

The three source/header files are copied without source edits. Their Git blob IDs
remain identical to the origin blobs. The local addition is the Termux package
recipe, archive/install layout and provenance gate.

Compile contract:

```text
C11 + -ffreestanding + -fno-builtin + -fno-stack-protector
no heap
no libc
no libm
no unresolved external symbols in the produced object
```

A compiler is still allowed to lower arithmetic to runtime helpers. Therefore the
package and CI explicitly inspect undefined symbols; if an ARM32 helper such as
`__aeabi_*` appears, the build fails closed rather than pretending to be
freestanding.

## Redistribution

This package is GPL-2.0-only because the packaged source files declare
GPL-2.0-only. The Apache-2.0 license used by RAFCODEPHI infrastructure/metapackage
glue does not override this package or the original Vectras/QEMU/Termux licenses.
Full upstream license/attribution evidence remains available at the exact origin
commit above and in the repository-level third-party attribution index.
