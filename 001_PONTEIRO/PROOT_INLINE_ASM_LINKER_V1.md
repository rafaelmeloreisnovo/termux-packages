# PRoot → ARMv7/AArch64 inline ASM: camada opcional v1

**Estado:** SOURCE_CANDIDATE / DRAFT-HOLD / não integrado ao PRoot distribuído.
**Produtor:** termux-packages, pacote original `packages/proot/build.sh` intacto.
**Contrato:** `rafcodephi.proot.raw-ptrace/a32-a64/v1`; compilação Linux/Android, não bare metal físico.

- ARM EABI: `r7 = 26` (ptrace), `r0..r3` parâmetros; A32 `SVC #0 = 0xEF000000`, bytes LE `00 00 00 EF`.
- AArch64: `x8 = 117` (ptrace), `x0..x3` parâmetros; A64 `SVC #0 = 0xD4000001`, bytes LE `01 00 00 D4`.
- ABI de retorno = resultado bruto do kernel, **não** a semântica libc `ptrace()`/errno. `PTRACE_PEEK*`, SELinux/seccomp, Android API e rastreamento real precisam de teste separado.
- Código sem libc, heap, variáveis globais, imports externos, loader dinâmico ou aplicação de patch a upstream.
- Gate: `bash scripts/ci/check_proot_asm_abi.sh` compila objetos A32/A64, verifica encoding hexadecimal e `nm -u` vazio, fecha ELF estático sem relocação final e sem seção dinâmica.
- No objeto ARM32, `.ARM.exidx` pode criar `R_ARM_PREL31` local resolvida pelo linker. **Não** rejeitar esta relocação intermediária como se fosse import dinâmica.
- ELF gerado pelo gate **não é programa utilizável**, pois o endereço de entrada serve exclusivamente ao teste de link e não é um ponto de entrada de processo.
- Não remover, modificar ou relicenciar o PRoot upstream (GPL-2.0), seus loaders ou dependências.
- `SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM`; `claim_allowed=false`.

**Falsificadores:** encoding distinto do esperado; símbolo indefinido; falha da linkedição; relocações no ELF final; dependências dinâmicas; divergência ARM/ARM64. **Ainda abertos:** validação de semântica ptrace, segurança, desempenho, instalação física e contrato da aplicação.

**Rollback:** descartar a branch/PR candidata sem alterações no pacote; manter código oficial e os receipts anteriores.
**F_next:** somente após evidência em Android físico e revisão de licença, prototipar adaptador semântico PRoot em branch separada, sem substituir biblioteca ptrace inadvertidamente.
