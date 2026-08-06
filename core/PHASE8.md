# Phase 8 — compilação real, manifesto fail-closed e proveniência

Estado desta branch:

```text
maturity=EXPERIMENTAL_BUILDER
claim_allowed=false
release_allowed=false
physical_termux_receipt=TOKEN_VAZIO
```

## O que está materializado

- `configure`, `make`, `make install` e coleta de payload real;
- captura conjunta de stdout/stderr;
- caminhos absolutos para `DESTDIR` e artefatos;
- seleção do shell por `TERMUX_BUILD_SHELL`, `$PREFIX/bin/sh` ou `/bin/sh`;
- manifesto binário carregado e validado antes da execução;
- pacote, versão, flags, dependências, arquitetura e API copiados do manifesto;
- pacote ausente, layout inválido, string sem terminador e limite rompido falham antes do build;
- `configure` existente que retorna erro conserva o erro — não vira “sem configure”;
- pacote TAR somente é criado quando o prefixo contém payload;
- testes fail-closed para manifesto malformado, pacote ausente, fonte ausente e resume não validado.

## Correções do formato V1

O gerador agora fixa:

```text
header=20 bytes
string_pool_size_field=4 bytes
entries=N×184 bytes
string_pool_offset=20+4+N×184
```

O offset `0` é reservado exclusivamente para ausência; a primeira string real começa em `1`.
Dependências acima de 16 deixam de ser truncadas silenciosamente e bloqueiam a geração.
Dependências cujo nome ainda não resolve para um pacote local recebem `0xFFFF` e são reportadas como:

```text
TOKEN_VAZIO_UNRESOLVED_DEPENDENCIES
```

## Limites honestos

| Gate | Estado |
|---|---|
| fonte previamente materializada | exigida |
| download + SHA-256 da fonte | `TOKEN_VAZIO_SOURCE_FETCH` |
| execução de patches declarados | `TOKEN_VAZIO_PATCH_EXECUTION` |
| pós-processamento/strip/rpath | `TOKEN_VAZIO_POST_PROCESSING` |
| resume por checkpoint | `TOKEN_VAZIO_RESUME_NOT_VALIDATED` |
| fechamento recursivo de dependências | `TOKEN_VAZIO_DEPENDENCY_CLOSURE` |
| NDK ARMv7/AArch64 | `TOKEN_VAZIO_TOOLCHAIN_RECEIPT` |
| formato `.deb` / `apt` / `dpkg` | `TOKEN_VAZIO_DISTRIBUTION_CONTRACT` |
| instalação no APK RAFCODE-Φ | `TOKEN_VAZIO_DEVICE_INSTALL` |

## Invariante

```text
manifesto válido
→ fonte materializada
→ patches resolvidos
→ configure/make/install
→ payload não vazio
→ pacote
→ receipt físico
```

Nenhuma etapa posterior promove uma anterior ausente.
O `hello-world` de host demonstra apenas a fatia local preparada; não demonstra ainda pacote Android, fechamento de 2.056 pacotes nem compatibilidade com `pkg/apt/dpkg`.

## Próximo gate

1. reconciliar o commit experimental no `runtime-lock.json` do `termux-app-rafacodephi`;
2. instalar e verificar NDK `26.3.11579264`;
3. materializar fonte por hash;
4. executar ARMv7 e AArch64;
5. validar prefixo RAFCODE-Φ;
6. instalar, executar, reverter e selar receipts.
