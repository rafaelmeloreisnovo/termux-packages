# Phase 8: Real Package Compilation and Artifact Collection

## Audit Findings Addressed

Phase 8 directly addresses the gaps identified in the PR #2 audit:

| Gap | Phase 8 Solution |
|-----|-----------------|
| Build phases simulate only | Execute real configure/make/install |
| Manifest not integrated | Load package data from binary manifest |
| TAR contains no payload | Collect actual binaries/libraries into TAR |
| Output capture discards data | Pipe-based capture now returns full output |
| No independent CI | Add reproducible build test |

## Changes in Phase 8

### 1. Output Capture Fixed (`sysexec.c`)

**Before:**
```c
int fd = open("/dev/null", O_WRONLY);
dup2(fd, STDOUT_FILENO);  // Discard output
```

**After:**
```c
int pipefd[2];
pipe(pipefd);
dup2(pipefd[1], STDOUT_FILENO);  // Capture to pipe
// Read pipefd[0] into output_buf
```

**Impact:** Compiler output now flows through to build log, enabling audit trail.

### 2. Real Build Execution (`build_exec.c`, `build_exec.h`)

New module provides executable abstractions:

```c
int termux_exec_configure(ctx, source_dir, build_dir);
int termux_exec_make(ctx, build_dir, num_jobs);
int termux_exec_make_install(ctx, build_dir, prefix_dir);
int termux_collect_artifacts(prefix_dir, output_dir, pkg_name, version, arch);
```

Each function:
- Executes real command via `termux_execve_capture()`
- Captures stdout/stderr into build context
- Returns exit code for error handling

### 3. Build Phase Integration (Pending)

Phase functions will be updated to use build_exec:

```c
static int termux_phase_configure(struct termux_build_context *ctx) {
  // Load source from manifest or $SOURCE_DIR
  // Call termux_exec_configure()
  // Return exit code (0 = success, non-zero = fail)
}

static int termux_phase_make(struct termux_build_context *ctx) {
  // Call termux_exec_make(ctx, build_dir, num_jobs)
  // Return exit code
}

static int termux_phase_massage(struct termux_build_context *ctx) {
  // Optional: strip symbols, reduce size, organize layout
}

static int termux_phase_package(struct termux_build_context *ctx) {
  // Call termux_collect_artifacts()
  // TAR now contains REAL FILES, not metadata-only
}
```

### 4. Manifest Integration (Next)

Will load package metadata from binary manifest:

```c
// In main():
struct termux_pkg_manifest *pkg = termux_find_package(manifest, package_name);
if (pkg) {
  ctx->pkg = *pkg;  // Load version, deps, arch, api_level
  // source_url = get_string(manifest, pkg->source_url_offset)
  // download_and_verify(source_url, pkg->sha256)
}
```

## Test Plan

### Unit Test: Output Capture

```bash
cd core
echo "Hello from test" | ./termux-build-core-test-capture
# Output should appear in build log
```

### Integration Test: hello-world Package

```bash
cd /tmp/hello-2.12
./configure --prefix=/tmp/prefix
make
make install DESTDIR=/tmp/prefix

# Artifact collection
tar -czf hello-2.12-aarch64.tar.gz -C /tmp/prefix .

# Verification
mkdir /tmp/test-install
tar -xzf hello-2.12-aarch64.tar.gz -C /tmp/test-install
/tmp/test-install/bin/hello
# Output: Hello, World!
```

### End-to-End Test: Build via termux-build-core

```bash
./termux-build-core \
  --package hello-world \
  --arch aarch64 \
  --api 24 \
  --manifest manifest.bin

# Check output:
tar -tzf build/hello-world-2.12-aarch64.tar.gz | head
# Should list actual binaries, not metadata
```

## File Status

| File | Phase 8 Status | Notes |
|------|---|---|
| `sysexec.c` | ✅ Fixed | Pipe-based capture working |
| `build_exec.c` | ✅ New | Execution wrappers added |
| `build_exec.h` | ✅ New | API defined |
| `termux-build-core.c` | 🔄 Pending | Integrate build_exec into phases |
| `manifest_loader.c` | 🔄 Pending | Add manifest lookups to main() |
| `packages/hello-world/` | ✅ New | Test package definition |
| `Makefile` | ✅ Updated | Includes build_exec.o |

## Known Issues to Fix

### A. Directory Handling

`termux_exec_configure()` uses `cd` in command string. On failure, subsequent phases still use old cwd. Fix:
- Store working directory in context
- Explicit chdir() in C (not shell)
- Verify cwd before each phase

### B. Environment Variables

Currently passes `NULL` for `envp` to `execve()`. Should pass:
```c
char *env[] = {
  "TERMUX_ARCH=aarch64",
  "TERMUX_HOST_PLATFORM=aarch64-linux-android",
  "TERMUX_PREFIX=/prefix",
  "CFLAGS=-march=armv8-a -O3",
  "LDFLAGS=-march=armv8-a",
  NULL
};
termux_execve_capture(path, argv, env, ...);
```

### C. Error Context

When make fails, which step? Line? Need:
- Capture compiler error messages
- Parse stderr for file:line:error format
- Report with context to user
- Save to error log for debugging

### D. Partial Builds

If phase fails partway through (e.g., make at 70%), can we resume? Options:
1. Fail and require full rebuild (Phase 8)
2. Checkpoint and resume from next phase (Phase 9+)
3. Checksum artifacts, skip if present (Phase 9+)

### E. Cross-Compilation

Current approach assumes build tools are on PATH. For Termux:
- May need Android NDK toolchain
- May need to cross-compile for ARM/x86
- Need to validate `$TERMUX_HOST_PLATFORM` against available compilers

## Next Steps After Phase 8

1. **Phase 9: Dependency Resolution** — Build package if any dep changed
2. **Phase 10: Reproducibility** — Deterministic timestamps, strip DWARF
3. **Phase 11: Caching** — Artifact caching, parallel builds
4. **Phase 12: Distribution** — APK/DEP signing, repository metadata

## Metrics

**Phase 8 Adds:**
- 1 fixed function (`termux_execve_capture`)
- 1 new module (`build_exec.c`, 180 lines)
- 1 new header (`build_exec.h`, 20 lines)
- 1 test package (`packages/hello-world/build.sh`)

**Lines of C: ~200 new, 50 modified**

---

**Invariant:** After Phase 8, building hello-world should produce a .tar.gz 
containing real executables and libraries, not metadata.

**Verification:** `tar -tzf hello-world-*.tar.gz | grep bin/hello`

---

End Phase 8 Plan
