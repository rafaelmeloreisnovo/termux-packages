# RAFCODEPHI Ω — Fechamento controlado do PREFIX e do pkg real (23/10/2026)

**Rota:** `termux-packages` (produtor) → arquivos `.deb` → ZIP ARM/AArch64 → importação no `termux-app-rafacodephi` → APK → aparelho físico → repositório APT assinado.

**Invariantes:** `SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM`; `TOKEN_VAZIO ≠ 0`; `claim_allowed=false`; nenhuma reescrita de ELF, alteração do upstream ou remoção de dados para obter um PASS.

## 1. Evidência conhecida, não herdada

A auditoria anterior, aplicada ao bootstrap ARM32 originado de pacotes legados, mostrou:
- **310 entradas binárias distintas** associadas a **70 pacotes** com referências incompatíveis ao PREFIX antigo (620 achados considerando duas variantes do caminho).
- `prefix-audit.json` SHA-256 `652926e70d79d3aff72641bd0b5307b10b2492ab6d2cac6c018264dfaa4864da`;
- `rebuild-scope.json` SHA-256 `b2005654735428d1b5b1febff95dc26c6f39ce5bc9c83a9b6cb94f6482b387ba`;
- `READY_TO_REBUILD` **não** é `REBUILT`; a reconstrução física desses pacotes ainda exige execução e recibos próprios.

Família de pacotes afetados (70, registrada apenas como escopo observado):
`apt, attr, bash, busybox, bzip2, coreutils, curl, dash, dialog, diffutils, dpkg, findutils, gawk, gpgv, grep, gzip, less, libacl, libandroid-glob, libandroid-posix-semaphore, libandroid-selinux, libandroid-shmem, libandroid-support, libassuan, libbz2, libcap-ng, libcurl, libevent, libffi, libgcrypt, libgmp, libgnutls, libgpg-error, libiconv, libidn2, liblz4, liblzma, libmd, libmpfr, libnettle, libnghttp2, libnghttp3, libngtcp2, libnpth, libsmartcols, libssh2, libtalloc, libtasn1, libunbound, libunistring, ncurses, ncurses-utils, openssl, p11-kit, pcre2, procps, proot, psmisc, readline, sed, tar, termux-am-socket, termux-core, termux-exec, termux-tools, util-linux, xxhash, xz-utils, zlib, zstd`.

Esta lista é **observação de um conjunto anterior**, não é allowlist nem lista de arquivos reconstituídos. A autoridade de build precisa reavaliar todos os `.deb` exatos da nova execução.

## 2. Família de inspeções implementada na PR

| Dimensão | Inspeção falsificável | Estado/limite |
| --- | --- | --- |
| Núcleo C autoral | `scripts/rafcodephi-freestanding/rafcodephi_prefix_gate_core.c`, byte scan sem libc/heap/syscalls; compilação `-ffreestanding -nostdlib`, `nm -u` vazio | apenas operação em buffer |
| ZIP | `scripts/ci/rafcodephi_bootstrap_prefix_gate.py` varre entradas binárias/executáveis/configs e perfis ARM/ARM64, com limite de descompressão e recibo | PASS_SCOPED/BLOCKED, não runtime |
| DEB | `scripts/ci/rafcodephi_deb_set_gate.py` verifica inventário exato `SHA256SUMS` e payload de cada `.deb`; duplicados, extras ou mismatch bloqueiam | PASS_SCOPED/BLOCKED, não publicação |
| Bootstrap | `build-rafcodephi-real-bootstrap.sh` exige ambos os gates antes de exportar cada ZIP, liga SHA de recibos ao manifesto | sem correção cega de ELF |
| Fonte ARM/ARM64 | workflow `rafcodephi-real-bootstrap.yml` executa falsificadores e source-build por arquitetura | requer CI exato HEAD, não herda PASS |
| App e Android | importar par `arm,aarch64`, assinar/identificar APK e verificar `lib/armeabi-v7a` / `lib/arm64-v8a` | TOKEN_VAZIO até artefato real |
| Repositório APT | gerar índice de `.deb`, publicar apenas por transporte TLS autenticado e `InRelease` assinado, validar `Signed-By` | BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED |
| Dispositivo | instalar sem apagar dados, testar shell, teclado, `pkg update`, `pkg install nano python git` | DEVICE_RUNTIME=TOKEN_VAZIO |

### Testes de falsificação

```bash
python3 scripts/tests/test_rafcodephi_prefix_gate.py
python3 scripts/tests/test_rafcodephi_deb_set_gate.py
python3 scripts/tests/test_rafcodephi_real_bootstrap_workflow_contract.py
bash -n scripts/build-rafcodephi-real-bootstrap.sh
```

### Build source-built (somente com capacidade de build, checkout e SHA fixados)

```bash
./scripts/build-rafcodephi-real-bootstrap.sh --architectures arm,aarch64
```

O comando deve produzir ZIPs, manifesto, `SHA256SUMS` e recibos por arquitetura **somente** se os gates passarem. Os pipelines de build usam bash/Python/compiladores Docker; apenas o sentinela C é freestanding. Não atribuir ao `apt/dpkg` propriedade sem libc.

## 3. Sequência de fechamento autorizável

1. **P0 — SOURCE:** resolver SHA exato da fonte, compilar do código-fonte a árvore necessária para o PREFIX `/data/data/com.termux.rafacodephi/files/usr`; nenhum pacote prebuilt de `com.termux` pode satisfazer o gate.
2. **P0 — ARTIFACT:** varrer todos os ZIPs e `.deb`; anexar SHA256, arquitetura, manifesto, mapa de proveniência e negative tests; não publicar um ZIP se um gate falhar.
3. **P0 — TRUST:** publicar repositório binário separado somente com chave/assinatura de origem confirmada; em seguida habilitar configuração apt com revisão humana. Não inventar URL ou chave. Preservar hook `RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED` enquanto esse estágio não existir.
4. **P1 — APK:** importar o par real source-built no app, registrar SHA de ZIP embutido e SHA final do APK, confirmar assinatura e layout de PREFIX.
5. **P1 — EXECUTION:** ADB em Moto E7 Power ARM32 e ARM64 de referência; shell, teclado, TermuxService, DNS/TLS, `pkg update`, três pacotes instalados; anexar stdout/stderr, códigos de saída, SDK e hashes.
6. **P2 — PROVIDER:** corrigir ruleset/required checks no provedor. `CI PASS` não substitui ruleset ou revisão independente.
7. **P3 — MEASURE:** perf/energia/memória/rollback sem apagar instalações de produção.

## 4. Fronteira do claim e rollback

`PASS_SCOPED` demonstra apenas ausência das ocorrências vetadas nos bytes inspecionados na revisão **exata**. Não demonstra segurança total, distribuição APT válida, compatibilidade física, nem sucesso de `pkg install`.

Se qualquer gate falhar: registrar `BLOCKED` ou `TOKEN_VAZIO` com identificador, hash, nome do arquivo, e manter o código em PR draft. Nenhum reset forçado, exclusão de artefatos de Drive/GitHub ou promoção automática. Rollback: deixar de integrar a branch, ou, se integrada por revisão autorizada, revert cirúrgico com confirmação de HEAD.
