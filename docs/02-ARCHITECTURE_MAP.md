# 🏗️ Architecture Map & Component Topology

**Purpose:** Visual and textual representation of architecture, component relationships, data flows, and system layers.

**Format:** ASCII diagrams + textual mapping for comprehensive understanding by humans and AI.

---

## 📐 System Architecture Overview

### Complete System Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         TERMUX BUILD SYSTEM                       │
│                      (rafaelmeloreisnovo fork)                   │
└─────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                     INPUT LAYER (Repository)                       │
├──────────────────────────────────────────────────────────────────┤
│  packages/         (2000+ packages)                               │
│  x11-packages/     (GUI packages)                                 │
│  root-packages/    (Root-only utilities)                          │
│  disabled-packages/ (Legacy/unused)                              │
│  build-package.sh  (Entry point)                                 │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│                   LAYER 1: REAL (EVIDENCE)                         │
├──────────────────────────────────────────────────────────────────┤
│                                                                     │
│  pkg_scanner.c ──┐                                                │
│  pkg_parser.c ───┼──→ pkg_dag.c ──→ metrics_producer.c           │
│  pkg_count.c ────┘                                                │
│                                                                     │
│  Real inputs:  syscalls, /proc, /sys, file hashes (SHA256)       │
│  Real outputs: JSON metrics, receipts, tamper-evident ledger      │
│                                                                     │
│  Status: REAL (verifiable, fail-closed, schema-first)            │
│  Contracts:                                                        │
│    - pkg_metrics/1.0.0 (inventory + DAG)                          │
│    - receipt/1.0.0 (operation proof)                              │
│    - ledger_entry/1.0.0 (append-only chain)                       │
│                                                                     │
│  See: REAL.md, REAL_GLOSSARY.md, REAL_INVARIANTS.md              │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│              LAYER 2: BITRAF64 (HYBRID ACCELERATION)              │
├──────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Framework:                                                        │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │ Hybrid acceleration for ARM & x86-64 architectures       │     │
│  │ - CPU feature detection (SIMD: NEON, SVE, SSE, AVX)      │     │
│  │ - Device capability probing                               │     │
│  │ - Platform-specific optimization paths                   │     │
│  └─────────────────────────────────────────────────────────┘     │
│                                                                     │
│  Scope: ARM (aarch64, armv7l) + x86-64 (Intel, AMD)              │
│  Status: Production (PHASE1 integration complete)                │
│                                                                     │
│  See: PHASE1_BITRAF64_INTEGRATION.md, PHASE2_OPTIMIZATION_GUIDE  │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│            LAYER 3: PHASE 9 (ORCHESTRATED OPTIMIZATION)           │
├──────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Stage 4: Device Validation                                       │
│    └─ Probe CPU, memory, cache, vector capabilities             │
│                                                                     │
│  Stage 5: Performance Optimization                                │
│    └─ Baseline profiling, hotspot identification                 │
│                                                                     │
│  Stage 6: SIMD Vectorization                                      │
│    ├─ Map hotspots to vector instructions                        │
│    └─ Generate architecture-specific code paths                  │
│                                                                     │
│  Stage 7: Multi-Core Parallelization                              │
│    └─ Lock-free orchestration, work distribution                 │
│                                                                     │
│  Stage 8: Hardware Tuning                                         │
│    └─ CPU affinity, cache optimization, memory binding           │
│                                                                     │
│  Stage 9: Advanced Vectorization                                  │
│    └─ Scalable SIMD backends, auto-tuning                        │
│                                                                     │
│  Stage 13: Integration                                            │
│    └─ End-to-end orchestration, test integration                 │
│                                                                     │
│  Stage 14: Validation                                             │
│    └─ Build + test entire system, measure results                │
│                                                                     │
│  Stage 15: Production Hardening                                   │
│    └─ Security, stability, failover procedures                   │
│                                                                     │
│  Stage 16: CI/CD Dashboard                                        │
│    └─ Monitor coherence, track metrics, detect anomalies         │
│                                                                     │
│  Stage 17: Device Validation                                      │
│    └─ Final pre-deployment verification on real hardware         │
│                                                                     │
│  See: PHASE9_4 → 9.5 → 9.6 → 9.7 → 9.8 → 9.9 → 9.13-9.17        │
│       (Follow in order for complete understanding)               │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│            LAYER 4: RAFCODEPHI (DELIVERY & COORDINATION)          │
├──────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Contract Orchestration:                                          │
│    - Delivery contract enforcement (V1)                           │
│    - Evidence vector coordination                                 │
│    - Evidence gates & corrections                                 │
│                                                                     │
│  Status: Production (handoff between layers)                      │
│  Consumes: REAL metrics, PHASE9 results                           │
│  Produces: Delivery proof, evidence certificate                   │
│                                                                     │
│  See: RAFCODEPHI_DELIVERY_CONTRACT_V1.md,                         │
│       REALITY_EVIDENCE_VECTOR_V2.md                               │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│                  LAYER 5: OPERATIONS                               │
├──────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Deployment (DEPLOYMENT_RUNBOOK.md)                               │
│    - Pre-deployment checks                                        │
│    - Staged rollout                                               │
│    - Validation at each stage                                     │
│    - Rollback procedures                                          │
│                                                                     │
│  Incident Response (INCIDENT_RESPONSE.md)                         │
│    - Production monitoring                                        │
│    - Incident triage                                              │
│    - Recovery playbooks                                           │
│    - Post-incident analysis                                       │
│                                                                     │
│  Status: Production ✓                                             │
│  See: DEPLOYMENT_RUNBOOK.md, INCIDENT_RESPONSE.md               │
└──────────────────────────────────────────────────────────────────┘
```

---

## 🧱 Component Dependency Graph

### Data Flow & Dependencies

```
                    REPOSITORY INPUT
                   packages/, scripts/
                          ↓
            ┌─────────────────────────────┐
            │    REAL LAYER (EVIDENCE)     │
            │                               │
            │  • pkg_scanner.c             │
            │  • pkg_parser.c              │
            │  • pkg_dag.c                 │
            │  • metrics_producer.c        │
            │  • real_receipt.c            │
            │  • real_ledger.c             │
            └────────────────┬────────────┘
                              │
                ┌─────────────┴──────────────┐
                ↓                             ↓
        [REAL METRICS]              [REAL RECEIPTS]
        (pkg_metrics/1.0.0)         (receipt/1.0.0)
        - DAG structure              - Operation proof
        - Node/edge counts           - Timestamp
        - Latency metrics            - Content hash
        - Coverage stats             - Signature chain
                │                             │
                └──────────┬──────────────────┘
                           ↓
        ┌──────────────────────────────────────┐
        │  BITRAF64 LAYER (ACCELERATION)       │
        │                                       │
        │  Framework: Device → Probe → Tune    │
        │  ├─ ARM: aarch64, armv7l              │
        │  └─ x86: Intel, AMD (SSE, AVX)       │
        └────────────────┬─────────────────────┘
                         │
         [DEVICE CAPABILITIES]
         - CPU features
         - Memory/cache
         - Vector support
         - Thermal limits
                         │
        ┌────────────────▼──────────────────┐
        │  PHASE 9 (9.4-9.9)                │
        │  OPTIMIZATION PIPELINE             │
        │                                     │
        │  9.4: Device → 9.5: Profile        │
        │  ↓                                  │
        │  9.6: SIMD → 9.7: Parallelization │
        │  ↓                                  │
        │  9.8: Tune → 9.9: Advanced SIMD    │
        └────────────────┬───────────────────┘
                         │
    [OPTIMIZED BUILD ARTIFACTS]
    - SIMD-accelerated binaries
    - Parallelized build
    - Hardware-tuned
    - Performance validated
                         │
        ┌────────────────▼──────────────────┐
        │  PHASE 9 (9.13-9.17)               │
        │  INTEGRATION & VALIDATION           │
        │                                     │
        │  9.13: Integrate  → 9.14: Build+Test
        │  ↓                                  │
        │  9.15: Harden  → 9.16: Monitor     │
        │  ↓                                  │
        │  9.17: Device Validate             │
        └────────────────┬───────────────────┘
                         │
        ┌────────────────▼─────────────────┐
        │ RAFCODEPHI (DELIVERY HANDOFF)     │
        │                                    │
        │ • Contract enforcement             │
        │ • Evidence coordination            │
        │ • Delivery proof                   │
        └────────────────┬────────────────┘
                         │
        ┌────────────────▼──────────────────┐
        │  OPERATIONS LAYER                  │
        │                                     │
        │  DEPLOYMENT_RUNBOOK               │
        │  └─ Staged rollout + validation   │
        │                                     │
        │  INCIDENT_RESPONSE                │
        │  └─ Production monitoring + triage │
        │                                     │
        │  Status: ✓ Production Ready        │
        └────────────────────────────────────┘
```

---

## 🎯 Key Components & Their Responsibilities

### REAL Layer Components

| Component | File | Responsibility | Input | Output | Status |
|-----------|------|-----------------|-------|--------|--------|
| **pkg_scanner** | `core/pkg_scanner.{h,c}` | Discover packages & scripts | Filesystem | Package list | REAL |
| **pkg_parser** | `core/pkg_parser.{h,c}` | Extract build variables | `build.sh` | Parsed metadata | REAL |
| **pkg_dag** | `core/pkg_dag.{h,c}` | Build dependency graph | Parsed metadata | DAG structure | REAL |
| **metrics_producer** | `core/metrics_producer.c` | Emit measurements | DAG + probes | JSON metrics | REAL |
| **real_receipt** | `core/real_receipt.{h,c}` | Seal operations | Event data | Signed receipt | REAL |
| **real_ledger** | `core/real_ledger.{h,c}` | Hash-chain ledger | Receipts | Append-only chain | REAL |
| **real_sha256** | `core/real_sha256.{h,c}` | Cryptographic hash | Any data | SHA256 digest | REAL |
| **real_contract** | `core/real_contract.{h,c}` | Validate schemas | JSON artifact | Pass/fail verdict | REAL |

### PHASE 9 Stages

| Stage | Key Responsibility | Input | Output | Dependency |
|-------|-------------------|-------|--------|------------|
| **9.4** | Device capability probing | Hardware info | Device profile | BITRAF64 |
| **9.5** | Performance baseline | Build artifacts | Hotspot map | 9.4 |
| **9.6** | SIMD vectorization | Hotspot map | Vector code paths | 9.5 |
| **9.7** | Multi-core orchestration | Vector paths | Parallelized code | 9.6 |
| **9.8** | Hardware-specific tuning | Parallelized code | Tuned binaries | 9.7 |
| **9.9** | Scalable SIMD backends | Tuned binaries | Advanced vectors | 9.8 |
| **9.13** | Orchestration glue | All stages above | Integrated build | 9.4-9.9 |
| **9.14** | Build + test | Integrated build | Tested binaries | 9.13 |
| **9.15** | Hardening | Tested binaries | Hardened system | 9.14 |
| **9.16** | CI/CD monitoring | Hardened system | Metrics + alerts | 9.15 |
| **9.17** | Device validation | Hardened system | Deployment ready | 9.16 |

---

## 🔄 Data Flow Patterns

### Pattern 1: REAL Layer Measurement

```
Repository Input
    ↓
pkg_scanner (discover)
    ↓
pkg_parser (extract variables)
    ↓
pkg_dag (build DAG)
    ↓
metrics_producer (measure)
    ↓
[JSON artifact: pkg_metrics/1.0.0]
    ↓
real_receipt (seal)
    ↓
real_ledger (append to chain)
    ↓
[Tamper-evident evidence]
```

**Key Properties:**
- Fail-closed: Any step failure → exit non-zero
- No simulation: All data from real syscalls/files
- Tamper-evident: SHA256-sealed with ledger chain
- Schema-first: JSON validated against contract

### Pattern 2: PHASE 9 Optimization Pipeline

```
Build Artifacts
    ↓
9.4: Device Probe
    ├─ CPU features (SIMD capabilities)
    ├─ Memory hierarchy
    └─ Thermal envelope
    ↓
9.5: Profile
    ├─ Hotspot identification
    └─ Baseline measurements
    ↓
9.6: SIMD
    ├─ Map hotspots → instructions
    └─ Generate code paths
    ↓
9.7: Parallelize
    ├─ Lock-free distribution
    └─ Work queue design
    ↓
9.8: Tune
    ├─ CPU affinity
    └─ Cache optimization
    ↓
9.9: Advanced
    ├─ Auto-tuning
    └─ Scalable backends
    ↓
[Optimized Binaries]
    ↓
9.13: Integrate
9.14: Validate
9.15: Harden
9.16: Monitor
9.17: Pre-Deploy
    ↓
[Production Ready]
```

### Pattern 3: Delivery & Handoff (RAFCODEPHI)

```
REAL Evidence
    ↓
PHASE 9 Results
    ↓
RAFCODEPHI Contract
    ├─ Validate evidence completeness
    ├─ Cross-reference all measurements
    └─ Generate delivery proof
    ↓
[Delivery Certificate]
    ↓
Deployment Authorization
```

---

## 🗺️ Architecture Layers (Vertical Stack)

```
┌────────────────────────────────┐
│    L5: OPERATIONS              │  DEPLOYMENT_RUNBOOK
│    (Production)                │  INCIDENT_RESPONSE
├────────────────────────────────┤
│    L4: DELIVERY COORDINATION   │  RAFCODEPHI
│    (Handoff)                   │  REALITY_EVIDENCE_VECTOR
├────────────────────────────────┤
│    L3: OPTIMIZATION            │  PHASE 9.4-9.17
│    (Performance & Hardening)   │  11 stages
├────────────────────────────────┤
│    L2: ACCELERATION FRAMEWORK  │  BITRAF64
│    (Device Capability Probe)   │  PHASE1-2
├────────────────────────────────┤
│    L1: EVIDENCE & MEASUREMENT  │  REAL Layer
│    (Provenance & Tamper-Proof) │  REAL_*.md
├────────────────────────────────┤
│    L0: REPOSITORY INPUT        │  packages/
│    (Source Code & Config)      │  scripts/
└────────────────────────────────┘
```

**Each layer:**
- Has well-defined inputs and outputs
- Consumes previous layer's outputs
- Produces contract-validated artifacts
- Enables the next layer's operation

---

## 📍 File-to-Component Mapping

### Core Implementation

```
core/
├── pkg_scanner.{h,c}           ← REAL Layer: Package discovery
├── pkg_parser.{h,c}            ← REAL Layer: Metadata extraction
├── pkg_dag.{h,c}               ← REAL Layer: Dependency graph
├── real_provenance.{h,c}       ← REAL Layer: Provenance tracking
├── real_arch.{h,c}             ← BITRAF64: Architecture catalog
├── real_contract.{h,c}         ← REAL Layer: Schema validation
├── real_sha256.{h,c}           ← REAL Layer: Cryptographic hash
├── real_receipt.{h,c}          ← REAL Layer: Operation receipts
├── real_ledger.{h,c}           ← REAL Layer: Append-only ledger
├── real_attrs.h                ← REAL Layer: Compiler directives
├── real_syscalls.h             ← REAL Layer: Freestanding syscalls
├── real_mem.h                  ← REAL Layer: Freestanding memory
├── metrics_producer.c          ← REAL Layer: Metrics emission
├── arch_probe_cli.c            ← BITRAF64: Runtime probe CLI
├── pkg_count_freestanding.c    ← REAL Layer: Package counter
├── contract_validate_cli.c     ← REAL Layer: Contract validator
├── receipt_validate_cli.c      ← REAL Layer: Receipt validator
└── receipt_ledger_cli.c        ← REAL Layer: Ledger manager
```

### Scripts

```
scripts/
├── build-package.sh            ← PHASE 9: Main build orchestrator
├── build-all.sh                ← PHASE 9: Batch build
├── build-all-orchestrated.sh   ← PHASE 9.13: Orchestrated build
├── real_governance.sh          ← REAL Layer: Governance gate (10-stage)
├── real_arch_runtime_probe.py  ← BITRAF64: Runtime probe (Python)
└── run-docker.sh               ← Operations: Docker environment
```

---

## 🔗 Cross-Layer Communication

### REAL ↔ BITRAF64 Interface

```
REAL Layer Output:
  pkg_metrics/1.0.0 (DAG structure)
        ↓
BITRAF64 Input:
  ├─ Architecture index
  ├─ Device capability probe
  └─ Optimization hints
        ↓
BITRAF64 Output:
  Device profile (CPU features, memory, cache)
```

### BITRAF64 ↔ PHASE 9 Interface

```
BITRAF64 Output:
  Device capabilities
        ↓
PHASE 9.4 Input:
  Device probe → profile
        ↓
PHASE 9 Stages (5-17):
  Progressive optimization
        ↓
PHASE 9 Output:
  Hardened, optimized binaries
```

### PHASE 9 ↔ RAFCODEPHI Interface

```
PHASE 9 Outputs:
  Build results
  Performance metrics
  Hardening status
        ↓
RAFCODEPHI:
  Coordinate measurements
  Validate delivery contract
  Generate proof
        ↓
Deployment Ready Signal
```

---

## 📊 System Metrics & Observability

### Points of Measurement (REAL Layer)

```
Repository Scan
    ├─ inventory_coverage (package discovery)
    ├─ parse_failures (parsing success rate)
    └─ alternative_dep_fields (dep complexity)
                    ↓
Dependency Graph
    ├─ node_count (total packages)
    ├─ edge_count (total dependencies)
    ├─ cycle_count (circular deps)
    ├─ max_depth (longest chain)
    ├─ topo_ordered (topological ordering possible)
    ├─ coherence_phi (overall quality metric)
    ├─ graph_completeness (resolved deps)
    └─ graph_acyclicity (acyclic structure)
                    ↓
Performance Metrics
    ├─ inventory_latency_us (scanning time)
    ├─ dag_latency_us (graph building time)
    └─ total_latency_us (end-to-end time)
```

### Points of Validation (PHASE 9)

```
Device Profile
    ├─ CPU features detected
    ├─ Memory available
    └─ Cache configuration
                    ↓
Performance Baseline
    ├─ Hotspot identification
    ├─ Current throughput
    └─ Latency profile
                    ↓
Optimization Results
    ├─ SIMD code paths generated
    ├─ Parallelization applied
    ├─ Hardware tuning applied
    ├─ Performance improvement %
    └─ Binary size change
                    ↓
Build Validation
    ├─ All packages built
    ├─ Tests passed
    ├─ Hardening applied
    └─ Deployment ready
```

---

## 🎯 Decision Points & Branching

### Critical Decisions in Architecture

```
1. Device Probe (PHASE 9.4)
   ├─ Capability detected? → YES → Continue to 9.5
   └─ NO → Fallback to baseline (9.5)
                    ↓
2. Hotspot Found (PHASE 9.5-9.6)
   ├─ Vectorizable? → YES → Apply SIMD (9.6)
   └─ NO → Skip to 9.7
                    ↓
3. Parallelizable (PHASE 9.7)
   ├─ Dependencies? → YES → Lock-free pattern
   └─ Independent → Standard parallelization
                    ↓
4. Tuning Applied (PHASE 9.8)
   ├─ Performance improved? → YES → Continue
   └─ NO → Revert & skip
                    ↓
5. Build Validation (PHASE 9.14)
   ├─ Tests pass? → YES → Harden (9.15)
   └─ NO → Root cause analysis + fix
                    ↓
6. Production Readiness (PHASE 9.15-9.17)
   ├─ Hardening complete? → YES → Deploy
   └─ NO → Fix issues + re-validate
```

---

## 🔐 Security & Integrity Checkpoints

```
REAL Layer Checkpoints:
    ├─ Fail-closed contract: Missing → Abort
    ├─ SHA256 seal integrity: Broken → Abort
    ├─ Ledger chain: Tampered → Alert
    └─ Schema validation: Invalid → Abort
                    ↓
BITRAF64 Checkpoints:
    ├─ Device profile signature: Invalid → Use baseline
    └─ Capability detection: Inconclusive → Conservative mode
                    ↓
PHASE 9 Checkpoints:
    ├─ Build artifact hash: Mismatch → Rebuild
    ├─ Test suite: Failed → Investigate
    └─ Hardening validation: Failed → Reject
                    ↓
Operations Checkpoints:
    ├─ Pre-deployment: Incomplete → Block
    ├─ Staged rollout: Anomaly detected → Pause
    └─ Post-deployment: Health check: Failed → Rollback
```

---

## 📈 Scaling & Performance Characteristics

### Scaling by Package Count

```
100 packages    → ~100ms   inventory + DAG
1000 packages   → ~500ms   inventory + DAG
10000 packages  → ~3sec    inventory + DAG (estimated)

Memory usage:
  DAG structure: O(nodes + edges)
  Receipts:      O(operations)
  Ledger:        O(transaction history)
```

### Optimization Overhead by Stage

```
9.4 Device Probe      : ~100ms  (syscalls, /proc, /sys)
9.5 Performance Prof  : ~1-5s   (benchmark runs)
9.6 SIMD Mapping      : ~100ms  (code analysis)
9.7 Parallelization   : ~50ms   (work distribution design)
9.8 Hardware Tuning   : ~100ms  (affinity + cache config)
9.9 Advanced SIMD     : ~200ms  (backend generation)
────────────────────────────────────
Total per build       : ~1-10s  (depending on package size)
```

---

## 🧩 Component Interaction Matrix

| Component | Depends On | Consumed By | Type |
|-----------|-----------|------------|------|
| pkg_scanner | Filesystem | pkg_parser | Discovery |
| pkg_parser | build.sh content | pkg_dag | Parsing |
| pkg_dag | Parsed metadata | metrics_producer, PHASE9 | Graph |
| metrics_producer | pkg_dag, probes | real_receipt | Measurement |
| real_receipt | metrics | real_ledger | Sealing |
| real_ledger | receipts | Verification | Chain |
| arch_probe | CPU/memory | PHASE9.4 | Probing |
| PHASE9.4-9.9 | Device profile | PHASE9.13 | Optimization |
| PHASE9.13-9.17 | Optimized artifacts | RAFCODEPHI | Integration |
| RAFCODEPHI | REAL + PHASE9 | Operations | Handoff |
| Operations | RAFCODEPHI | External systems | Deployment |

---

## ✅ Validation Checklist

When adding a new component or layer:

- [ ] Component has clear inputs and outputs
- [ ] Contract is defined (JSON schema)
- [ ] Failure mode is fail-closed (not silent)
- [ ] Measurement points are real (not simulated)
- [ ] Component is documented in REAL.md §2
- [ ] Cross-references to dependent components exist
- [ ] Performance baseline is established
- [ ] Security checkpoints are defined
- [ ] Integration path is documented

---

**Last Updated:** 2026-08-17  
**Status:** Complete architecture map ✓  
**Next:** See `00-INDEX.md` for reading paths by role.
