# 🗺️ Documentation Navigation Guide

**Purpose:** Detailed guide to the documentation structure, reading paths, and how to efficiently locate information.

**Audience:** AI systems, developers, build engineers, and documentation maintainers.

---

## 📐 Documentation Structure

### Hierarchy Levels

```
docs/
├── 00-INDEX.md                    [Master index & quick-jump tables]
├── 01-NAVIGATION.md               [This file - navigation guide]
├── 02-ARCHITECTURE_MAP.md         [Visual & textual architecture]
├── 03-GLOSSARY.md                 [Unified terminology]
│
├── REAL.md                        [Core: Provenance & Evidence]
├── REAL_*.md                      [Supporting: Glossary, Invariants, Audits]
│
├── PHASE*.md                      [Implementation: Build & Optimization]
│
├── BITRAF64*.md                   [Framework: Hybrid Acceleration]
├── RAFCODEPHI*.md                 [Coordination: Delivery & Contracts]
│
├── DEPLOYMENT_RUNBOOK.md          [Operations: Production]
└── INCIDENT_RESPONSE.md           [Operations: Crisis Management]
```

### Document Naming Conventions

| Pattern | Meaning | Example |
|---------|---------|---------|
| `^[0-9]{2}-` | Navigation/meta | `00-INDEX.md`, `01-NAVIGATION.md` |
| `^REAL` | Provenance & measurement | `REAL.md`, `REAL_GLOSSARY.md` |
| `^PHASE[0-9]` | Build stages & optimization | `PHASE9_5_*.md` |
| `^BITRAF64` | Hybrid framework | `BITRAF64_PHASE1_*.md` |
| `^RAFCODEPHI` | Delivery & coordination | `RAFCODEPHI_DELIVERY_*.md` |
| `^DEPLOYMENT` | Production procedures | `DEPLOYMENT_RUNBOOK.md` |
| `^INCIDENT` | Crisis procedures | `INCIDENT_RESPONSE.md` |
| `^SISTEMA` | Foundational architecture | `SISTEMA_NUCLEO_*.md` |
| `_PASS[1-9]` | Iterative audit/validation | `REAL_HOTFIX_AUDIT_PASS3.md` |
| `_[0-9]{8}` | Dated changes/hotfixes | `...20260814.md` |

---

## 🧭 Navigation Strategies

### Strategy 1: Topic-Based Navigation

**Use when:** You know what you're looking for (optimization, deployment, etc.)

**Steps:**
1. Consult `00-INDEX.md` Quick Navigation table
2. Choose your section (REAL, PHASE 9, etc.)
3. Follow the recommended reading path
4. Use subsection anchors to jump within documents

**Example:**
```
Goal: Optimize build performance
→ Open: 00-INDEX.md
→ Find: "Performance Optimization" use case
→ Follow: PHASE9_5_* → 9.6 → 9.7 → 9.8 → 9.9
```

### Strategy 2: Dependency-Based Navigation

**Use when:** You need understanding prerequisites

**Approach:**
1. Identify your goal document
2. Check `| Depends On |` column in 00-INDEX.md
3. Read prerequisites first (top-down)
4. Then read the goal document

**Example:**
```
Goal: Understand PHASE9_16_CI_CD_DASHBOARD.md
→ Depends on: 9.15 → 9.14 → 9.13 → 9.4-9.9
→ Read order: Start with 9.4, progress through 9.5-9.9, then 9.13-9.16
```

### Strategy 3: Incident-Based Navigation

**Use when:** Responding to production issues

**Steps:**
1. Read `INCIDENT_RESPONSE.md` for initial triage
2. Identify affected component (REAL, PHASE 9, etc.)
3. Cross-reference `REAL_HOTFIX_AUDIT_PASS*.md` for similar cases
4. Check `DEPLOYMENT_RUNBOOK.md` §Rollback for recovery
5. Update incident log with resolution

### Strategy 4: Glossary-First Navigation

**Use when:** Encountering unfamiliar terminology

**Steps:**
1. Note the unfamiliar term(s)
2. Search `03-GLOSSARY.md` for definition
3. If defined, follow to related docs
4. If not found, check `REAL_GLOSSARY.md` for REAL-specific terms
5. Add new terms to glossary as needed

### Strategy 5: Cross-Reference Navigation

**Use when:** Understanding component relationships

**Steps:**
1. Identify your starting document
2. Scan its cross-reference sections (usually marked with "See also:")
3. Build a mental map of related documents
4. Navigate the implied graph

**Example path for understanding end-to-end build:**
```
PHASE9_14_END_TO_END_BUILD.md
  → References: 9.13 (orchestration)
  → References: 9.4-9.9 (stages)
  → References: REAL.md (contracts)
  → References: DEPLOYMENT_RUNBOOK.md (post-build)
```

---

## 📍 Location Guide

### By System Layer

#### **Provenance & Evidence (REAL Layer)**
```
Primary: REAL.md
├─ Architecture & contracts → §1-3
├─ Module index → §2
├─ Schema definitions → §3
└─ Implementation details → §4-5

Support Docs:
├─ REAL_GLOSSARY.md → Status tokens
├─ REAL_INVARIANTS.md → Structural contracts
├─ REAL_TOUR_OUTPUT.md → Live example
└─ REAL_HOTFIX_AUDIT*.md → Validation history
```

#### **Build Orchestration (PHASE 9)**
```
Primary: PHASE9_INTEGRATION_GUIDE.md (9.13)
├─ Device validation → PHASE9_4_DEVICE_VALIDATION.md
├─ Performance baseline → PHASE9_5_PERFORMANCE_OPTIMIZATION.md
├─ SIMD mapping → PHASE9_6_SIMD_VECTORIZATION.md
├─ Parallelization → PHASE9_7_MULTICORE_PARALLELIZATION.md
├─ Hardware tuning → PHASE9_8_HARDWARE_TUNING.md
├─ Advanced vectors → PHASE9_9_ADVANCED_VECTORIZATION.md
├─ Validation → PHASE9_14_END_TO_END_BUILD.md
├─ Hardening → PHASE9_15_PRODUCTION_HARDENING.md
├─ CI/CD → PHASE9_16_CI_CD_DASHBOARD.md
└─ Device validation → PHASE9_17_DEVICE_VALIDATION.md
```

#### **Hybrid Acceleration (BITRAF64)**
```
Integration: PHASE1_BITRAF64_INTEGRATION.md
├─ Framework architecture
├─ ARM scope → FULL_ARM_CROSS_GRAPH_SCOPE.md
└─ Optimization → PHASE2_OPTIMIZATION_GUIDE.md

Recent Hotfixes:
└─ BITRAF64_PHASE1_PROOF_GATE_20260808.md (critical patch)
```

#### **Delivery & Coordination (RAFCODEPHI)**
```
Contract: RAFCODEPHI_DELIVERY_CONTRACT_V1.md
├─ Formal delivery spec
├─ Evidence mapping → REALITY_EVIDENCE_VECTOR_V2.md
└─ Gate corrections → RAFCODEPHI_EVIDENCE_GATE_CORRECTION_20260814.md
```

#### **Operations**
```
Production Deploy: DEPLOYMENT_RUNBOOK.md
├─ Pre-deployment checks
├─ Deployment sequence
├─ Post-deployment validation
└─ Rollback procedures

Incident Response: INCIDENT_RESPONSE.md
├─ Triage procedures
├─ Recovery playbooks
└─ Post-incident analysis
```

---

## 🔎 Search & Discovery Patterns

### For AI / Query Systems

**Pattern 1: Find all documents about [TOPIC]**
```
→ Search: 00-INDEX.md for "[TOPIC]" keywords
→ Result: Section with all related docs
→ Navigate: Follow "Depends On" links
```

**Pattern 2: Build understanding chain for [GOAL]**
```
→ Step 1: Find [GOAL] in 00-INDEX.md Use Cases
→ Step 2: Follow listed reading path (in order)
→ Step 3: Parse cross-references from each doc
→ Step 4: Verify completeness with checklist
```

**Pattern 3: Find related hotfixes/audits**
```
→ Identify component: "REAL", "PHASE9_X", etc.
→ Search 00-INDEX.md for audit passes
→ Read: REAL_HOTFIX_AUDIT_PASS* (Pass 1→7 chronological)
→ Cross-reference: INCIDENT_RESPONSE.md for similar incidents
```

**Pattern 4: Trace dependency graph**
```
→ Start: Target document
→ Extract: All "Depends On" references
→ Recurse: Follow each dependency
→ Result: Complete dependency tree
→ Navigate: Depth-first or breadth-first per need
```

---

## 📋 Reading Paths by Role

### Build Engineer
**Goal:** Understand & modify the build system

**Path:**
1. `REAL.md` — Understand measurement layer
2. `PHASE1_BITRAF64_INTEGRATION.md` — Framework architecture
3. `PHASE9_INTEGRATION_GUIDE.md` — Orchestration
4. `PHASE9_4-9.9` — Progressive optimization stages
5. `PHASE9_14_END_TO_END_BUILD.md` — Validation
6. `PHASE9_15_PRODUCTION_HARDENING.md` — Hardening

**Depth:** 5-7 documents, ~3000+ lines

### Performance Optimizer
**Goal:** Profile, optimize, and validate performance

**Path:**
1. `PHASE9_5_PERFORMANCE_OPTIMIZATION.md` — Baseline & profiling
2. `PHASE9_6_SIMD_VECTORIZATION.md` — SIMD strategy
3. `PHASE9_7_MULTICORE_PARALLELIZATION.md` — Parallelism
4. `PHASE9_8_HARDWARE_TUNING.md` — Platform-specific tuning
5. `PHASE9_9_ADVANCED_VECTORIZATION.md` — Advanced techniques
6. `PHASE9_14_END_TO_END_BUILD.md` — Validate results

**Depth:** 6 documents, ~2500 lines

### DevOps / Release Engineer
**Goal:** Deploy safely, monitor, respond to incidents

**Path:**
1. `PHASE9_15_PRODUCTION_HARDENING.md` — Security & stability
2. `PHASE9_17_DEVICE_VALIDATION.md` — Pre-deployment checks
3. `DEPLOYMENT_RUNBOOK.md` — Step-by-step procedures
4. `PHASE9_16_CI_CD_DASHBOARD.md` — Monitoring
5. `INCIDENT_RESPONSE.md` — Incident procedures

**Depth:** 5 documents, ~1700 lines

### Evidence Auditor / Compliance
**Goal:** Verify integrity, trace provenance, validate contracts

**Path:**
1. `REAL.md` §1-3 — Principles, modules, contracts
2. `REAL_GLOSSARY.md` — Status tokens
3. `REAL_INVARIANTS.md` — Structural contracts
4. `REAL_TOUR_OUTPUT.md` — Live example
5. `REAL_HOTFIX_AUDIT_PASS1.md` → PASS7 — Audit trail (chronological)
6. `RAFCODEPHI_DELIVERY_CONTRACT_V1.md` — Delivery spec

**Depth:** 11 documents, ~3100 lines

### System Architect
**Goal:** Understand complete architecture, design new components

**Path:**
1. `SISTEMA_NUCLEO_AUTORAL_COMPLETE.md` — Zero-abstraction foundation
2. `REAL.md` — Measurement & provenance
3. `PHASE1_BITRAF64_INTEGRATION.md` — Integration design
4. `PHASE9_INTEGRATION_GUIDE.md` — Orchestration pattern
5. `RAFCODEPHI_DELIVERY_CONTRACT_V1.md` — Delivery contracts

**Depth:** 5 documents, ~1800 lines

---

## 🎯 Quick Lookup Tables

### "I need to find information about..."

| Topic | Primary Doc | Secondary | Glossary |
|-------|-------------|-----------|----------|
| **DAG & dependencies** | `REAL.md` §2 | `PHASE9_INTEGRATION_GUIDE.md` | `03-GLOSSARY.md` |
| **SIMD optimization** | `PHASE9_6_SIMD_VECTORIZATION.md` | `PHASE9_9_ADVANCED_VECTORIZATION.md` | ← term def |
| **Device validation** | `PHASE9_4_DEVICE_VALIDATION.md` | `PHASE9_17_DEVICE_VALIDATION.md` | ← term def |
| **Contracts & schema** | `REAL.md` §3 | `REAL_INVARIANTS.md` | ← term def |
| **Evidence verification** | `REAL_TOUR_OUTPUT.md` | `REAL_HOTFIX_AUDIT.md` | ← term def |
| **Production deploy** | `DEPLOYMENT_RUNBOOK.md` | `PHASE9_15_PRODUCTION_HARDENING.md` | ← term def |
| **Incident triage** | `INCIDENT_RESPONSE.md` | `REAL_HOTFIX_AUDIT_PASS*.md` | ← term def |
| **Delivery spec** | `RAFCODEPHI_DELIVERY_CONTRACT_V1.md` | `REALITY_EVIDENCE_VECTOR_V2.md` | ← term def |

---

## 🚀 Efficient Navigation Tips

### For Humans

1. **Bookmark this file** (`01-NAVIGATION.md`) for offline reference
2. **Use browser search (Ctrl+F)** within each document
3. **Follow the recommended reading paths** for your role
4. **Check table of contents** at top of each large document
5. **Use section anchors** to jump within documents (e.g., `#real-layer`)

### For Search/Discovery Systems

1. **Index by prefix** (00-, 01-, REAL, PHASE, etc.) for rapid filtering
2. **Extract & build graphs** from `| Depends On |` columns
3. **Use role-based paths** to recommend reading sequences
4. **Parse use cases** from 00-INDEX.md for query understanding
5. **Track audit passes** (PASS 1-7) chronologically for historical context

### For Documentation Maintenance

1. **Update 00-INDEX.md** whenever a new doc is added
2. **Add dates** to new documents (suffix with `_YYYYMMDD` if critical)
3. **Mark dependencies** in document headers using `| Depends On |` pattern
4. **Keep glossary synchronized** (03-GLOSSARY.md)
5. **Cross-link related sections** using markdown anchors

---

## 📊 Navigation Efficiency Metrics

| Metric | Baseline | Target | Status |
|--------|----------|--------|--------|
| Time to find doc by name | <5s | <2s | ✓ Quick |
| Time to understand cross-refs | <2m | <30s | ⚠️ Improve |
| Time to build reading path | <3m | <1m | ⚠️ Improve |
| Broken links | TBD | 0 | 🔄 Validate |
| Circular dependencies | TBD | 0 | 🔄 Validate |

---

## ✅ Navigation Checklist

Use this when adding new documentation:

- [ ] Document has clear title (first `#` heading)
- [ ] Document has purpose statement (first paragraph)
- [ ] Document lists dependencies/prerequisites (if complex)
- [ ] Document has table of contents or section headers
- [ ] Key terms are defined or linked to `03-GLOSSARY.md`
- [ ] Cross-references to related docs are included
- [ ] Document is indexed in `00-INDEX.md`
- [ ] Filename follows naming convention (prefix or topic)
- [ ] Internal anchors are used for subsections
- [ ] Dated hotfix docs include date suffix (`_YYYYMMDD`)

---

## 🔗 Cross-Reference Index

### All outbound references (for building graphs)

**From this file (01-NAVIGATION.md):**
- → `00-INDEX.md` (master index)
- → `02-ARCHITECTURE_MAP.md` (visual architecture)
- → `03-GLOSSARY.md` (terminology)
- → `REAL.md` (core layer)
- → `PHASE*.md` (optimization suite)
- → All others referenced in tables above

---

**Last updated:** 2026-08-17  
**Maintainer:** Documentation system  
**Status:** Complete & navigable ✓

For questions or improvements, see the repository's CONTRIBUTING.md.
