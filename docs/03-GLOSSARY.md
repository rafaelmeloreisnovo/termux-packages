# 📖 Unified Glossary & Terminology Reference

**Purpose:** Central reference for all technical terms, acronyms, and concepts used across documentation.

**Format:** Alphabetically organized with cross-references and context.

---

## Quick Reference: Acronyms

| Acronym | Full Form | Context | Doc |
|---------|-----------|---------|-----|
| **API** | Application Programming Interface | Integration points | General |
| **ARM** | Advanced RISC Machine | Architecture (aarch64, armv7l) | BITRAF64 |
| **AVX** | Advanced Vector Extensions | SIMD instruction set (x86-64) | PHASE9_6, 9.9 |
| **CI/CD** | Continuous Integration/Continuous Deployment | Pipeline automation | PHASE9_16 |
| **CPU** | Central Processing Unit | Hardware | BITRAF64, PHASE9_4 |
| **CRT** | C Runtime | Runtime environment | REAL.md |
| **DAG** | Directed Acyclic Graph | Dependency structure | REAL.md |
| **DEB** | Debian package format | Package distribution | README.md |
| **FIPS** | Federal Information Processing Standards | Cryptography standard | REAL.md |
| **LLVM** | Low Level Virtual Machine | Compiler framework | BITRAF64 |
| **NEON** | Advanced SIMD for ARM | Vector instructions (ARM) | PHASE9_6, BITRAF64 |
| **NDK** | Native Development Kit | Android build tools | ndk-patches/ |
| **QA** | Quality Assurance | Testing & validation | PHASE9_14 |
| **RAM** | Random Access Memory | System memory | BITRAF64, PHASE9_8 |
| **SIMD** | Single Instruction Multiple Data | Vectorization | PHASE9_6, 9.9 |
| **SSE** | Streaming SIMD Extensions | SIMD for x86 | PHASE9_6 |
| **SVE** | Scalable Vector Extension | SIMD for ARM | PHASE9_6 |
| **SHA256** | Secure Hash Algorithm 256-bit | Cryptographic hash | REAL.md |
| **UI** | User Interface | Interaction layer | General |
| **x86** | Intel/AMD 32-bit architecture | Legacy architecture | References |

---

## A - Z Terminology

### A

**Abstraction Layer**
- Definition: Software layer that hides implementation details
- Context: SISTEMA_NUCLEO_AUTORAL_COMPLETE.md promotes "zero-abstraction" design
- Opposite: Zero-abstraction (direct hardware access)
- See: SISTEMA_NUCLEO_AUTORAL_COMPLETE.md

**Acyclicity** (graph)
- Definition: Property of a graph with no circular dependencies
- Formula: `graph_acyclicity = 1.0 - (cycle_count > 0 ? ε : 0.0)`
- Ideal: `graph_acyclicity = 1.0` (no cycles)
- See: REAL.md §3.1, REAL_INVARIANTS.md

**Affinity** (CPU)
- Definition: Binding a process to specific CPU cores
- Use: Performance optimization by reducing cache misses
- See: PHASE9_8_HARDWARE_TUNING.md

**Alternative Dependencies**
- Definition: Package dependencies with OR conditions (e.g., `libA | libB`)
- Counter: `alternative_dep_fields` in metrics
- Impact: Increases DAG complexity
- See: REAL.md §3.1

**Architecture**
- Definition: CPU architecture family (ARM, x86-64, etc.)
- Scope: BITRAF64 supports aarch64, armv7l, x86-64
- Detection: Via `real_arch.c` module
- See: BITRAF64 docs, PHASE9_4_DEVICE_VALIDATION.md

**Architecture Probe**
- Definition: Runtime system to detect CPU capabilities
- Output: Device profile (CPU features, memory, cache)
- CLI: `arch-probe` (C), `real_arch_runtime_probe.py` (Python)
- See: REAL.md §2

**ARM** (Advanced RISC Machine)
- Definition: RISC processor architecture family
- Variants: aarch64 (64-bit), armv7l (32-bit)
- SIMD: NEON (aarch64), SVE (newer)
- See: BITRAF64_PHASE1_INTEGRATION.md, FULL_ARM_CROSS_GRAPH_SCOPE.md

**Audit Pass**
- Definition: Iterative validation cycle in REAL layer
- Sequence: Pass 1 (initial) → Pass 7 (final)
- Purpose: Identify & fix issues progressively
- Docs: REAL_HOTFIX_AUDIT_PASS1.md through PASS7.md
- See: 00-INDEX.md

**Authority**
- Definition: Source of truth marker in REAL evidence
- Values: `observed` (runtime), `nominal` (static)
- Rule: `observed_supersedes_nominal` (runtime proof beats static)
- See: REAL.md §1

### B

**Baseline**
- Definition: Initial performance measurement before optimization
- Use: Comparison point for improvement verification
- Method: Profiling with representative workloads
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Binary**
- Definition: Compiled executable or library
- Format: ELF (Linux), typically 64-bit ARM or x86-64
- Optimization: Subject to SIMD, parallelization, tuning
- See: PHASE9_6, 9.7, 9.8

**BITRAF64**
- Definition: Hybrid acceleration framework for ARM & x86-64
- Status: Production (PHASE 1-2 complete)
- Scope: CPU feature detection + optimization paths
- Docs: PHASE1_BITRAF64_INTEGRATION.md, PHASE2_OPTIMIZATION_GUIDE.md

**Build Artifact**
- Definition: Output of build process (binaries, libraries, deb packages)
- Hash: Computed for tamper-detection
- Validation: Via SHA256 checksum
- See: PHASE9_14_END_TO_END_BUILD.md

**Build Dependency**
- Definition: Dependency needed only at build time (not runtime)
- Field: `TERMUX_PKG_BUILD_DEPENDS`
- Example: Compiler, header files
- See: REAL.md §2

**Build Script**
- Definition: `build.sh` file in package directory
- Contains: Build instructions, metadata variables
- Parser: `pkg_parser.c` extracts metadata
- See: REAL.md §2

**Build System**
- Definition: Orchestrated pipeline for compiling & packaging software
- Layers: REAL (measurement) → BITRAF64 (framework) → PHASE9 (optimization)
- Entry: `build-package.sh` or `build-all-orchestrated.sh`
- See: 02-ARCHITECTURE_MAP.md

---

### C

**Cache**
- Definition: High-speed memory buffer between CPU and RAM
- Levels: L1 (smallest, fastest) → L2 → L3 (largest, slowest)
- Optimization: Cache-aware data layout, prefetching
- See: PHASE9_8_HARDWARE_TUNING.md

**Cache Coherence**
- Definition: Ensuring all CPU cores see consistent memory
- Importance: Critical for multi-core correctness
- Overhead: Cache coherence protocols add latency
- See: PHASE9_7_MULTICORE_PARALLELIZATION.md

**Capability**
- Definition: CPU or device feature that can be exploited
- Examples: SIMD support, multi-core, vector extensions
- Detection: Via `arch-probe` (runtime) or `real_arch.c` (static)
- See: BITRAF64, PHASE9_4_DEVICE_VALIDATION.md

**Coherence** (in measurements)
- Definition: Consistency & internal validity of metrics
- Metric: `coherence_phi` in pkg_metrics
- Formula: `coherence_phi = graph_completeness × graph_acyclicity`
- Ideal: `coherence_phi ≈ 1.0`
- See: REAL.md §3.1

**Coherence Phi** (φ)
- Definition: Composite metric for overall measurement quality
- Formula: `coherence_phi = graph_completeness × graph_acyclicity ± 0.0001`
- Range: [0.0, 1.0]
- Use: Executive summary of build system health
- See: REAL_INVARIANTS.md

**Completeness** (graph)
- Definition: Percentage of dependencies that were resolved
- Formula: `graph_completeness = 1 - (unresolved_count / edge_count)`
- Target: `completeness ≈ 1.0` (all deps resolved)
- See: REAL.md §3.1, REAL_INVARIANTS.md

**Component**
- Definition: Modular part of the build system
- Examples: pkg_scanner, pkg_dag, metrics_producer
- Interface: Well-defined inputs/outputs
- See: REAL.md §2, 02-ARCHITECTURE_MAP.md

**Compiler**
- Definition: Tool that translates source code to machine code
- LLVM-based tools used for ARM & x86 compilation
- Optimization: Vectorization flags, inline hints
- See: BITRAF64, PHASE9

**Compiler Directives**
- Definition: Attributes telling compiler how to optimize
- Examples: `__attribute__((hot))`, `__attribute__((cold))`
- File: `core/real_attrs.h`
- See: REAL.md §2

**Configuration**
- Definition: Build-time settings for a package
- File: `build.sh` environment variables
- Parser: `pkg_parser.c` extracts these
- See: REAL.md §2

**Contract**
- Definition: Formal specification of inputs/outputs/invariants
- Format: JSON schema with constraints
- Enforcement: `real_contract.c` (fail-closed validation)
- See: REAL.md §3, REAL_INVARIANTS.md

**Cross-Compilation**
- Definition: Compiling for a target architecture different from build host
- Example: Building ARM binaries on x86-64
- Support: BITRAF64 + PHASE9 enable this
- See: BITRAF64_PHASE1_INTEGRATION.md

**Cycle** (in dependencies)
- Definition: Circular dependency (A → B → A)
- Problem: Breaks topological ordering
- Detection: `cycle_count` in metrics
- Goal: `cycle_count = 0` (no cycles)
- See: REAL.md §3.1, REAL_INVARIANTS.md

---

### D

**DAG** (Directed Acyclic Graph)
- Definition: Graph with directed edges and no cycles
- Use: Represent package dependencies
- Builder: `pkg_dag.c` module
- Properties: Enables topological sorting (build order)
- See: REAL.md §2

**Delivery Contract**
- Definition: Formal agreement on what will be delivered
- Version: RAFCODEPHI_DELIVERY_CONTRACT_V1.md
- Content: Specifications, evidence requirements, acceptance criteria
- See: RAFCODEPHI_DELIVERY_CONTRACT_V1.md

**Dependency**
- Definition: Relationship where one package requires another
- Types: Runtime (`TERMUX_PKG_DEPENDS`), build-time (`TERMUX_PKG_BUILD_DEPENDS`)
- Graph: `pkg_dag.c` builds DAG from these
- See: REAL.md §2

**Deployment**
- Definition: Process of moving built software to production
- Procedure: DEPLOYMENT_RUNBOOK.md specifies steps
- Validation: Pre/during/post-deployment checks
- Rollback: Procedures for recovery if issues arise
- See: DEPLOYMENT_RUNBOOK.md

**Device Profile**
- Definition: Summary of CPU/memory capabilities on a specific device
- Generated by: `arch-probe` (PHASE9.4)
- Contains: SIMD support, core count, cache size, memory limit
- Used by: PHASE9.5-9.17 for optimization decisions
- See: PHASE9_4_DEVICE_VALIDATION.md

**Device Validation**
- Definition: Testing on real hardware to verify correctness
- Stages: PHASE9.4 (initial probe), PHASE9.17 (pre-deployment)
- Importance: Catches issues not visible in simulation
- See: PHASE9_4_DEVICE_VALIDATION.md, PHASE9_17_DEVICE_VALIDATION.md

**Disabled Package**
- Definition: Package no longer built (legacy, incompatible, etc.)
- Location: `disabled-packages/` directory
- Reason: Documented in commit or README
- See: Repository structure

**Docker**
- Definition: Containerization system for isolated build environments
- Usage: `./scripts/run-docker.sh` creates build container
- Benefit: Reproducible, isolated builds across machines
- See: README.md, build instructions

---

### E

**Edge** (in graph)
- Definition: Connection between two nodes (packages)
- Types: `depends_edges` (runtime), `build_dep_edges` (build-time)
- Count: `edge_count = depends_edges + build_dep_edges`
- See: REAL.md §3.1

**Evidence**
- Definition: Measurable proof of system state/behavior
- Sources: Real syscalls, file reads, hash computations
- Storage: JSON artifacts + tamper-evident ledger
- Principle: `Real sources only` (REAL.md §1)
- See: REAL.md

**Evidence Gate**
- Definition: Checkpoint verifying evidence before proceeding
- Types: REAL governance gates, RAFCODEPHI gates
- Correction: RAFCODEPHI_EVIDENCE_GATE_CORRECTION_20260814.md
- See: RAFCODEPHI docs

**Evidence Vector**
- Definition: Multi-dimensional measurement of build quality/completeness
- Schema: REALITY_EVIDENCE_VECTOR_V2.md
- Dimensions: Completeness, correctness, performance, security
- See: REALITY_EVIDENCE_VECTOR_V2.md

---

### F

**Fail-Closed**
- Definition: Default behavior on error is to stop (not continue silently)
- Principle: REAL.md §1 (second principle)
- Implementation: Every failed step exits with non-zero status
- Alternative: Fail-open (continues on error, dangerous)
- See: REAL.md

**Freestanding**
- Definition: Code that doesn't depend on libc/CRT
- Purpose: Avoid circular dependencies, enable early measurement
- Examples: `pkg_count_freestanding.c`, `real_mem.h`
- See: REAL.md §2

**Frequency** (CPU)
- Definition: Clock speed in GHz/MHz
- Tuning: `PHASE9_8_HARDWARE_TUNING.md` may adjust frequency scaling
- Impact: Trade-off between performance & power consumption
- See: PHASE9_8_HARDWARE_TUNING.md

---

### G

**Glossary**
- Definition: This document; reference for terminology
- Purpose: Enable AI & humans to understand concepts uniformly
- See: REAL_GLOSSARY.md (REAL-specific terms), this file (unified)
- Related: 03-GLOSSARY.md (this file)

**Governance**
- Definition: Process for validating & approving changes
- REAL Governance: 10-stage gate in `scripts/real_governance.sh`
- Stages: Inventory → Parse → DAG → Metrics → Receipt → Ledger → Validate → etc.
- See: REAL.md §2

**Granularity**
- Definition: Level of detail in optimization or measurement
- Fine-grained: Per-function optimization
- Coarse-grained: Per-package optimization
- Tradeoff: Fine-grained = better results but higher overhead
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Graph** (dependency)
- Definition: Representation of package relationships
- Structure: Nodes = packages, Edges = dependencies
- Property: Acyclic (DAG) for deterministic build order
- See: REAL.md §2

---

### H

**Hardening**
- Definition: Process of securing system against attacks/failures
- Stage: PHASE9.15 (Production Hardening System)
- Aspects: Memory safety, input validation, error handling, failover
- See: PHASE9_15_PRODUCTION_HARDENING.md

**Hardware**
- Definition: Physical CPU, memory, cache, peripherals
- Discovery: Via `/proc`, `/sys`, runtime probes
- Tuning: PHASE9.8 adapts build for specific hardware
- See: BITRAF64, PHASE9_4, PHASE9_8

**Hash**
- Definition: Fixed-size fingerprint of data
- Algorithm: SHA256 (FIPS 180-4)
- Use: Tamper-detection (any bit change breaks hash)
- Implementation: Freestanding in `core/real_sha256.{h,c}`
- See: REAL.md §2

**Hotfix**
- Definition: Urgent patch addressing critical issue
- Audit Trail: REAL_HOTFIX_AUDIT_PASS*.md (7 passes)
- Example: BITRAF64_PHASE1_PROOF_GATE_20260808.md
- See: Audit docs

**Hotspot**
- Definition: Code section using disproportionate CPU time
- Identification: Via profiling (PHASE9.5)
- Optimization: SIMD (9.6), parallelization (9.7), tuning (9.8)
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Hybrid**
- Definition: Combining multiple approaches
- Context: BITRAF64 = hybrid framework (ARM + x86 support)
- See: BITRAF64 documentation

---

### I

**Incident**
- Definition: Unexpected problem in production
- Triage: INCIDENT_RESPONSE.md specifies procedures
- Example Issues: Build failure, performance regression, security breach
- Response: Immediate mitigation → root cause → fix → validation
- See: INCIDENT_RESPONSE.md

**Incident Response**
- Definition: Procedures for handling production issues
- Document: INCIDENT_RESPONSE.md
- Stages: Detection → Triage → Mitigation → Recovery → Analysis
- See: INCIDENT_RESPONSE.md

**Inline** (function)
- Definition: Compiler directive to inline small functions
- Benefit: Reduces call overhead
- Downside: Increases binary size
- Use: Hot functions in `real_*.c` modules
- See: REAL.md §2

**Integration**
- Definition: Combining components into working system
- Stage: PHASE9.13 (Orchestrated Build System Integration)
- Process: Link all stages 4-9 into one pipeline
- See: PHASE9_INTEGRATION_GUIDE.md

**Invariant**
- Definition: Condition that must always hold
- Examples: `edge_count == depends_edges + build_dep_edges`
- Verification: `real_contract.c` checks cross-field invariants
- See: REAL.md §3.1, REAL_INVARIANTS.md

**Inventory**
- Definition: Complete list of discoverable packages
- Scope: `packages/`, `x11-packages/`, `root-packages/`, `disabled-packages/`
- Metrics: Counts, coverage, errors
- Output: `pkg_metrics/1.0.0` JSON artifact
- See: REAL.md §2

---

### J

**JSON**
- Definition: Data format for structured information
- Use: REAL metrics, receipts, contracts all JSON
- Schema: Versioned (e.g., `pkg_metrics/1.0.0`)
- Validation: `contract_validate_cli.c` enforces schema
- See: REAL.md §3

---

### K

**Kernel**
- Definition: Core OS component
- SISTEMA_NUCLEO_AUTORAL: "Zero-abstraction kernel"
- Distinction: Not Linux kernel, but foundational abstraction
- See: SISTEMA_NUCLEO_AUTORAL_COMPLETE.md

---

### L

**Latency**
- Definition: Time delay for operation
- Measurement: Microseconds (µs)
- Types: `inventory_latency_us`, `dag_latency_us`, `total_latency_us`
- See: REAL.md §3.1

**Layer**
- Definition: Logical level in architecture stack
- Hierarchy: L0 (input) → L1 (REAL) → L2 (BITRAF64) → L3 (PHASE9) → L4 (RAFCODEPHI) → L5 (Operations)
- See: 02-ARCHITECTURE_MAP.md

**Ledger**
- Definition: Append-only record of all operations
- Property: Hash-chained (each entry hashes previous)
- Tamper-detection: Any edit breaks chain
- Implementation: `core/real_ledger.{h,c}`
- See: REAL.md §2

**License**
- Definition: Legal terms for software use
- Storage: `packages/termux-licenses/LICENSES/`
- Requirement: Every package must declare license
- See: Repository structure

**LLVM**
- Definition: Low Level Virtual Machine compiler framework
- Use: Multi-target compilation (ARM, x86, etc.)
- Advantage: Single codebase → multiple architectures
- See: BITRAF64 documentation

**Lock-Free**
- Definition: Concurrent programming without mutex locks
- Benefit: Better performance, no deadlocks
- Complexity: Harder to verify correctness
- Use: PHASE9.7 Multicore orchestration
- See: PHASE9_7_MULTICORE_PARALLELIZATION.md

---

### M

**Measurement**
- Definition: Quantitative observation of system behavior
- Source: Real syscalls, file reads, hardware probes
- Storage: JSON artifacts with timestamps & hashes
- Principle: `Real sources only` (REAL.md §1)
- See: REAL.md

**Memory**
- Definition: RAM available for computation
- Binding: NUMA/CPU affinity to improve locality
- Tuning: PHASE9.8 optimizes for memory hierarchy
- See: PHASE9_8_HARDWARE_TUNING.md

**Memory Safety**
- Definition: Ensuring no buffer overflows, use-after-free, etc.
- Hardening: PHASE9_15_PRODUCTION_HARDENING.md includes this
- Tools: AddressSanitizer, MemorySanitizer
- See: PHASE9_15_PRODUCTION_HARDENING.md

**Metadata**
- Definition: Information describing something else
- Example: Package name, version, dependencies
- Extraction: `pkg_parser.c` pulls from `build.sh`
- See: REAL.md §2

**Metric**
- Definition: Measurable quantity
- Examples: Package count, edge count, build time
- Container: `pkg_metrics/1.0.0` JSON artifact
- See: REAL.md §3.1

**Module**
- Definition: Self-contained component with clear interface
- Examples: `pkg_scanner.c`, `pkg_dag.c`, `real_ledger.c`
- Status: Each has `status` field (REAL, OBSERVED, etc.)
- See: REAL.md §2

**Monitor**
- Definition: Continuous observation of system health
- Stage: PHASE9.16 (CI/CD Dashboard)
- Alerting: Detect anomalies, notify operators
- See: PHASE9_16_CI_CD_DASHBOARD.md

**Multi-Core**
- Definition: CPU with multiple processing cores
- Utilization: PHASE9.7 distributes work across cores
- Tradeoff: Better throughput, coordination overhead
- See: PHASE9_7_MULTICORE_PARALLELIZATION.md

---

### N

**NDK** (Native Development Kit)
- Definition: Android SDK component for native code compilation
- Use: Cross-compile for Android
- Patches: `ndk-patches/` directory
- See: Repository structure

**NEON**
- Definition: SIMD instruction set for ARM processors
- Vectors: 128-bit operations on 8/16/32-bit elements
- Support: Built-in for aarch64, optional for armv7l
- Mapping: PHASE9.6 generates NEON code
- See: PHASE9_6_SIMD_VECTORIZATION.md

**Node** (in graph)
- Definition: Vertex in dependency graph (one package)
- Count: `node_count` in metrics
- Properties: Name, version, dependencies
- See: REAL.md §3.1

**Nominal**
- Definition: Theoretical/static value (not measured at runtime)
- Principle: `Observed supersedes nominal` (REAL.md §1)
- Example: Nominal architecture vs. observed CPU features
- See: REAL.md

**Non-Zero Status**
- Definition: Exit code indicating error (1, 2, etc.)
- Importance: Fail-closed principle requires this
- See: REAL.md §1

**Notation**
- Definition: Mathematical or symbolic representation
- Examples: φ (phi, coherence), δ (delta, difference)
- See: Formulas throughout docs

---

### O

**Observed**
- Definition: Measured at runtime (actual, not theoretical)
- Principle: `Observed supersedes nominal` (REAL.md §1)
- Example: Actual CPU capabilities vs. specification sheet
- See: REAL.md

**Optimization**
- Definition: Improving performance/efficiency
- Stages: Baseline (5) → SIMD (6) → Parallel (7) → Tune (8) → Advanced (9)
- Method: Profile-guided optimization (PGO)
- See: PHASE9_5 through PHASE9_9

**Orchestration**
- Definition: Coordinating multiple components
- Stage: PHASE9.13 (Orchestrated Build System Integration)
- Purpose: Link all optimization stages into one workflow
- See: PHASE9_INTEGRATION_GUIDE.md

**Output**
- Definition: Result or artifact produced
- Examples: Binary executables, JSON metrics, .deb packages
- Hash: Every output should be hashed for verification
- See: Build system documentation

---

### P

**Package**
- Definition: Software distribution unit (source code + metadata)
- Format: Source tarball + `build.sh` + patches
- Location: `packages/[name]/` directory
- Count: ~2000+ in this repository
- See: README.md, REAL.md §2

**Package Manager**
- Definition: System for installing/managing packages
- Termux: APT (Debian-compatible)
- Distribution: `.deb` format
- See: README.md

**Parallelization**
- Definition: Running multiple operations simultaneously
- Stage: PHASE9.7 (Multicore orchestration)
- Benefit: Faster execution on multi-core systems
- Challenge: Synchronization, deadlock avoidance
- See: PHASE9_7_MULTICORE_PARALLELIZATION.md

**Parser**
- Definition: Code that extracts structured data from input
- Module: `pkg_parser.c`
- Input: `build.sh` files
- Output: Extracted metadata (name, version, depends, etc.)
- See: REAL.md §2

**Pass**
- Definition: Single iteration through audit/validation
- Examples: REAL_HOTFIX_AUDIT_PASS1 through PASS7
- Purpose: Identify & fix issues progressively
- Progression: Sequential (1→2→3→...→7)
- See: Audit documentation

**Performance**
- Definition: Speed/efficiency of system
- Measurement: Throughput, latency, operations per second
- Improvement: PHASE9 stages optimize for better performance
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Probe**
- Definition: Runtime inspection of system capabilities
- CLI: `arch-probe` (C), `real_arch_runtime_probe.py` (Python)
- Use: Gather device profile in PHASE9.4
- Output: JSON with CPU, memory, cache capabilities
- See: PHASE9_4_DEVICE_VALIDATION.md

**Production**
- Definition: Live environment serving actual users
- Readiness: PHASE9_15_PRODUCTION_HARDENING.md verifies this
- Deployment: DEPLOYMENT_RUNBOOK.md specifies procedures
- Incident: INCIDENT_RESPONSE.md handles issues
- See: Operations documentation

**Profiling**
- Definition: Measuring where CPU time is spent
- Purpose: Identify hotspots for optimization
- Stage: PHASE9.5 (Performance Optimization)
- Tools: Perf, VTune, custom instrumentation
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Proof**
- Definition: Evidence establishing truth of claim
- Use: REAL layer provides proof of measurement integrity
- Format: JSON artifact + SHA256 seal + ledger chain
- See: REAL.md

**Provenance**
- Definition: Origin & history of something
- Tracking: Git commit, build timestamp, toolchain, host
- Module: `core/real_provenance.{h,c}`
- See: REAL.md §2

---

### Q

**Quality**
- Definition: Degree to which system meets requirements
- Metric: `coherence_phi` (composite quality score)
- Assessment: Multi-dimensional (correctness, performance, security)
- See: REAL_INVARIANTS.md, REALITY_EVIDENCE_VECTOR_V2.md

---

### R

**RAFCODEPHI**
- Definition: Delivery coordination layer
- Components: Contract enforcement, evidence coordination
- Documents: RAFCODEPHI_DELIVERY_CONTRACT_V1.md, REALITY_EVIDENCE_VECTOR_V2.md
- Purpose: Handoff between optimization & deployment layers
- See: RAFCODEPHI documentation

**RAM** (Random Access Memory)
- Definition: Main system memory
- Detection: Via `/proc/meminfo`
- Tuning: Memory-aware optimization in PHASE9.8
- Binding: NUMA affinity to improve locality
- See: PHASE9_8_HARDWARE_TUNING.md

**Receipt**
- Definition: Signed proof of operation completion
- Schema: `receipt/1.0.0` (JSON)
- Content: Operation, timestamp, hash, signature
- Ledger: Each receipt appended to chain
- See: REAL.md §2

**Recovery**
- Definition: Process of restoring system to healthy state
- Context: Rollback after failed deployment
- Procedures: DEPLOYMENT_RUNBOOK.md §Rollback
- Plan: INCIDENT_RESPONSE.md
- See: Operations documentation

**Regression**
- Definition: Performance loss or new bug introduced by change
- Detection: Comparing before/after metrics
- Response: Investigate & rollback if needed
- See: CI/CD docs

**Repository**
- Definition: Central storage for code/documentation
- This repo: termux-packages (fork for Google Play)
- Root: `/home/user/termux-packages/`
- Branches: `main`, feature branches like `claude/documentation-sweep-*`
- See: Git docs

**Reproducibility**
- Definition: Ability to get same result from same inputs
- Importance: Critical for builds & testing
- Enablers: Docker (isolated environment), versioned tools
- See: README.md

**Rollback**
- Definition: Reverting to previous working state
- Trigger: Deployment failure or critical bug
- Procedure: DEPLOYMENT_RUNBOOK.md §Rollback
- Validation: Health checks post-rollback
- See: DEPLOYMENT_RUNBOOK.md

**Runtime**
- Definition: Time when program executes
- Opposite: Compile-time (when program builds)
- Probing: PHASE9.4 runtime probe gathers device capabilities
- See: PHASE9_4_DEVICE_VALIDATION.md

---

### S

**Scan**
- Definition: Systematic examination of directory structure
- Module: `pkg_scanner.c`
- Purpose: Discover all `build.sh` files & packages
- Output: Package inventory (fed to parser)
- See: REAL.md §2

**Schema**
- Definition: Formal structure/contract for data
- Format: Versioned (e.g., `pkg_metrics/1.0.0`)
- Validation: `real_contract.c` enforces compliance
- See: REAL.md §3

**Score** (quality)
- Definition: Numeric measure of system health
- Example: `coherence_phi` ∈ [0.0, 1.0]
- Interpretation: Closer to 1.0 = healthier system
- See: REAL_INVARIANTS.md

**Script**
- Definition: Executable code file
- Examples: `build-package.sh`, `real_governance.sh`
- Purpose: Automate build, test, deploy processes
- See: `/scripts/` directory

**SIMD** (Single Instruction Multiple Data)
- Definition: CPU instructions operating on multiple data elements
- Examples: NEON (ARM), SSE/AVX (x86)
- Benefit: 4-16x performance improvement for vectorizable code
- Mapping: PHASE9.6 generates SIMD code
- See: PHASE9_6_SIMD_VECTORIZATION.md

**Specification**
- Definition: Formal description of requirements
- Example: RAFCODEPHI_DELIVERY_CONTRACT_V1.md
- Purpose: Establish agreement before implementation
- See: RAFCODEPHI documentation

**SSE** (Streaming SIMD Extensions)
- Definition: SIMD instruction set for x86 processors
- Vectors: 128-bit operations
- Support: Built-in for all modern x86-64
- Variants: SSE, SSE2, SSE3, SSE4, SSE4.2
- See: PHASE9_6_SIMD_VECTORIZATION.md

**Stability**
- Definition: System reliability under normal operation
- Hardening: PHASE9_15_PRODUCTION_HARDENING.md
- Testing: Unit, integration, system tests
- See: PHASE9_15_PRODUCTION_HARDENING.md

**Stage** (PHASE 9)
- Definition: One step in optimization pipeline
- Sequence: 4 → 5 → 6 → 7 → 8 → 9 → 13 → 14 → 15 → 16 → 17
- Dependency: Each stage builds on previous
- See: 00-INDEX.md, PHASE9_*

**Standard Library**
- Definition: Set of common functions (libc, libm, etc.)
- Avoidance: Some REAL modules are freestanding (no stdlib)
- Reason: Avoid circular dependencies, enable early measurement
- See: REAL.md §2

**Stress Test**
- Definition: Testing system under extreme load
- Purpose: Verify stability, find edge cases
- Example: REAL_HOTFIX_AUDIT_PASS6.md
- See: Testing documentation

**Subpackage**
- Definition: Alternative variant of a package
- File: `*.subpackage.sh`
- Example: Same source, different compile options
- Discovery: `pkg_scanner.c` finds these
- See: REAL.md §2

**Supersedes**
- Definition: Replaces or takes precedence over
- Context: `observed_supersedes_nominal` (REAL.md §1)
- Example: Measured CPU features beat specification
- See: REAL.md

**SVE** (Scalable Vector Extension)
- Definition: Scalable SIMD for ARM (newer)
- Vectors: 128-2048 bits (implementation-dependent)
- Support: Newer aarch64 processors
- Advantage: Auto-scaling for different hardware
- See: PHASE9_6_SIMD_VECTORIZATION.md, PHASE9_9_ADVANCED_VECTORIZATION.md

**Syscall** (System Call)
- Definition: Request to kernel to perform privileged operation
- Examples: `open()`, `read()`, `mmap()`
- Overhead: Context switch (expensive)
- Use: REAL layer uses real syscalls, not simulated
- See: REAL.md §1

---

### T

**Tamper-Evidence**
- Definition: Detecting any modification to data
- Method: SHA256 seal + ledger hash chain
- Property: Any bit change breaks verification
- Principle: REAL.md §1 (Tamper-evident)
- See: REAL.md

**Telemetry**
- Definition: Automated collection of measurements
- Use: PHASE9_16 CI/CD dashboard for monitoring
- Metrics: Build time, performance, health indicators
- See: PHASE9_16_CI_CD_DASHBOARD.md

**Test**
- Definition: Automated verification of correctness
- Types: Unit (functions), integration (components), system (whole build)
- Stage: PHASE9.14 (End-to-End Build Validation)
- Coverage: Should cover happy path + edge cases
- See: PHASE9_14_END_TO_END_BUILD.md

**Test Suite**
- Definition: Collection of related tests
- Execution: Via `make test` or similar
- Requirement: All tests pass before deployment
- See: Testing documentation

**Throughput**
- Definition: Operations per unit time
- Measurement: Packages built per hour, MB/s, etc.
- Optimization: PHASE9 improves throughput
- See: PHASE9 optimization documentation

**Timestamp**
- Definition: Time when something happened
- Format: Unix epoch (seconds since 1970-01-01)
- Precision: Milliseconds (`generated_unix_ms`)
- Use: Tamper detection (any modification changes time)
- See: REAL.md §3

**Topological Order**
- Definition: Ordering of nodes respecting edges
- Property: Every dependency comes before dependent
- Requirement: Enables deterministic build order
- Field: `topo_ordered` in metrics
- See: REAL.md §3.1

**Trace**
- Definition: Record of program execution
- Format: Detailed log of function calls, memory access, etc.
- Use: Debugging, performance analysis
- See: Profiling documentation

**Trade-off**
- Definition: Accepting less of one thing for more of another
- Examples: Speed vs. size, security vs. convenience
- Optimization: PHASE9 navigates these
- See: PHASE2_OPTIMIZATION_GUIDE.md

**Tuning**
- Definition: Fine-tuning for specific hardware
- Stage: PHASE9.8 (Hardware-Specific Tuning)
- Aspects: CPU frequency, cache settings, memory binding
- See: PHASE9_8_HARDWARE_TUNING.md

---

### U

**Unresolved**
- Definition: Dependency not found in repository
- Example: Dependency on external package not in scope
- Counter: `unresolved_count` in metrics
- Target: `unresolved_count = 0` for completeness
- See: REAL.md §3.1

---

### V

**Validation**
- Definition: Checking that something meets requirements
- Stages: Build validation (9.14), device validation (9.4, 9.17)
- Criteria: Tests pass, performance meets baselines, hardening complete
- See: PHASE9 documentation

**Vector** (in optimization)
- Definition: Array of data processed by single instruction
- Size: 128-bit (SSE/NEON) to 512-bit (AVX-512)
- Use: SIMD operations process multiple elements simultaneously
- See: PHASE9_6_SIMD_VECTORIZATION.md

**Vectorization**
- Definition: Transforming scalar code to use SIMD
- Automatic: Compiler may auto-vectorize loops
- Manual: Developers write SIMD-specific code
- Stages: PHASE9.6 (basic), PHASE9.9 (advanced)
- See: PHASE9_6_SIMD_VECTORIZATION.md, PHASE9_9_ADVANCED_VECTORIZATION.md

**Version**
- Definition: Release number of software
- Format: Semantic versioning (major.minor.patch)
- Tracking: Git tags, package versions
- Schema versioning: e.g., `pkg_metrics/1.0.0`
- See: Repository tags

**Verification**
- Definition: Confirming truth or correctness
- Method: Contract validation, test execution, audit trails
- Cryptographic: SHA256 seal verification
- See: REAL.md, testing docs

---

### W

**Workload**
- Definition: Representative task for measurement
- Use: Performance profiling runs on workloads
- Importance: Workload characteristics affect optimization decisions
- See: PHASE9_5_PERFORMANCE_OPTIMIZATION.md

**Workflow**
- Definition: Sequence of steps for a process
- Example: CI/CD workflow (test → build → deploy → validate)
- See: Operations documentation

---

### X

**x86** / **x86-64**
- Definition: Intel/AMD 64-bit architecture
- Variants: x86 (32-bit, legacy), x86-64 (64-bit, modern)
- SIMD: SSE, SSE2, SSE3, SSE4, AVX, AVX-512
- Support: BITRAF64 optimizes for x86-64
- See: BITRAF64 documentation

**x11-packages**
- Definition: GUI packages requiring X11/Wayland
- Location: `x11-packages/` directory
- Use: Desktop applications
- Complexity: Often larger, more dependencies
- See: Repository structure

---

### Y

(Reserved for future terms)

---

### Z

**Zero-Abstraction**
- Definition: Direct hardware access without abstraction layer
- Benefit: Maximum performance, deterministic behavior
- Tradeoff: Harder to port, maintain
- Document: SISTEMA_NUCLEO_AUTORAL_COMPLETE.md
- See: Foundational architecture docs

---

## Cross-Reference Index

### By Document

| Document | Key Terms |
|----------|-----------|
| `REAL.md` | REAL, measurement, contract, fail-closed, tamper-evidence, provenance |
| `PHASE9_*` | Stages, optimization, profiling, SIMD, parallelization, tuning |
| `BITRAF64_*` | Architecture, device profile, hybrid acceleration |
| `RAFCODEPHI_*` | Delivery contract, evidence coordination, handoff |
| `DEPLOYMENT_RUNBOOK.md` | Deployment, rollback, validation, monitoring |
| `INCIDENT_RESPONSE.md` | Incident, triage, recovery, analysis |

### By Concept

| Concept | Related Terms |
|---------|--------------|
| **Architecture** | ARM, x86-64, SIMD, capability, device profile |
| **Measurement** | Metric, measurement, evidence, audit, proof |
| **Optimization** | Hotspot, profiling, SIMD, parallelization, tuning |
| **Reliability** | Hardening, testing, validation, recovery, rollback |
| **Coordination** | Orchestration, ledger, contract, delivery, handoff |

---

## Using This Glossary

### For Humans
1. Use Ctrl+F to search for term
2. Read definition and context
3. Check cross-references (links to docs)
4. Consult full documents for detailed explanations

### For AI / Search Systems
1. Parse alphabetically by heading
2. Extract definition, context, doc links
3. Build term → concept relationships
4. Use for query expansion & understanding
5. Reference when explaining findings

---

**Last Updated:** 2026-08-17  
**Status:** Complete & comprehensive ✓  
**Related:** See `00-INDEX.md` for document overview, `01-NAVIGATION.md` for navigation strategies.
