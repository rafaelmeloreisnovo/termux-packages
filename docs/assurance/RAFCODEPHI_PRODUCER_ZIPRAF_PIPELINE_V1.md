# RAFCODEΦ · Orquestração do produtor e custódia ZIPRAF V1

## Escopo e fronteira

A primeira refatoração é limitada ao pipeline produtor em `termux-packages`: `.github/workflows/rafcodephi-auto-handoff.yml`. Não reorganiza os outros 38 workflows, não modifica receitas de pacotes, não reescreve bootstrap e não publica APK. Os APKs são responsabilidade do consumidor `rafaelmeloreisnovo/termux-app-rafacodephi`, cuja matriz mantém variantes ARMv7/AArch64, signed/unsigned e seus próprios gates.

O handoff existente **deve permanecer byte-compatível**: `rafcodephi-termux-packages-<producer_sha>` entrega `artifacts/rafcodephi-bootstrap/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt`, os ZIPs `rafcodephi-bootstrap-arm.zip` e `rafcodephi-bootstrap-aarch64.zip`, e `build/reports/rafcodephi-auto-handoff.json`. O consumidor `rafcodephi-v1-termux-packages.yml` valida nome, id, digest SHA e caminhos. Essa interface não foi removida para evitar regressão.

## Novo pipeline (dependências explícitas)

```text
PR: contract (self-test/stdlib/identidade) --> HOLD: nada é enviado ao APK

push main / dispatch:
 contract
    |
    v
 produce:
   bind package identity -> real ARM/ARM64 build
   -> manifest + SHA-256 + receipt
   -> deterministic non-APK ZIPRAF + self-verification
   -> portable ZIPRAF GitHub artifact
   -> legacy handoff artifact (unchanged file paths)
   -> artifact-id / artifact-digest binding receipt
    |
    v
 handoff (job separado, needs contract+produce):
   validate outputs and token -> repository_dispatch
    |
    v
 termux-app-rafacodephi:
   download exact producer artifact -> verify ZIP/ELFs
   -> compile split APK -> upload APK artifacts (separate ownership)
```

Se `contract`, `produce`, SHA, identidade ou upload falhar, `handoff` não executa. `GITHUB_RUN_ID`, SHA, name, artifact ID, digest e receipt_sha256 mantêm a semântica antiga da mensagem `rafcodephi_packages_ready`. O segredo existente é usado somente no job final, sem entrar no ZIP.

## Artefatos

| Artefato no GitHub | Conteúdo / destino | Estado / motivo |
|---|---|---|
| `rafcodephi-producer-zipraf-<sha>` | **Um ZIP portátil** `rafcodephi-producer-evidence.zip` e `.sha256`; manifesto tipado dentro do ZIP | Novo, não contém APK |
| `rafcodephi-termux-packages-<sha>` | Arquivos originais da interface de consumo | Mantido para não quebrar o app |
| `rafcodephi-producer-failure-<run>-<sha>` | Logs/triagem somente se houver falha de produção | Mantido, sem fingir build PASS |
| APKs assinados/não assinados ARM32/ARM64 | Criados no **repo consumidor**, em artefatos independentes | Fora desta PR; nenhum APK produzido por `termux-packages` |

O ZIPRAF usa apenas lista allowlist: manifesto, bootstraps ARM32/ARM64 e os relatórios explicitados de `rafcodephi-auto-*`. O manifesto interno `ZIPRAF_MANIFEST.json` associa `sha256` + tamanho por arquivo, `source_sha`, `run_id`, `claim_allowed=false` e `physical_android=TOKEN_VAZIO`. Não agrega arquivos privados, chaves, pastas arbitrárias nem `.apk`.

`scripts/ci/rafcodephi_producer_zipraf.py --self-test` usa stdlib Python: testa determinismo, rejeição de divergência SHA e rejeição de APK na allowlist. A criação verifica CRC, contagem, nomes e hash dos membros; um ZIP correto **não prova execução física**, ciência metrológica, direitos de terceiros ou segurança geral.

### Compatibilidade e próxima fase

Esta é uma **fase de migração segura**: por enquanto existem dois artefatos GitHub de produtor (ZIPRAF consolidado e legado), porque o consumidor depende da estrutura legada. Remover o legado antes de migrar a validação de hash do app seria regressão. Na fase seguinte, após mudar e testar o consumidor, o ZIPRAF poderá virar transporte único sem quebrar o schema/dispatch.

O nome de artefato de GitHub não é um arquivo ZIP físico: `actions/upload-artifact` empacota conteúdos num ZIP de transporte. O arquivo `rafcodephi-producer-evidence.zip` é o ZIP canônico independente, cujo SHA pode ser validado offline.

**P0 autoria/licenças:** essa cadeia registra proveniência; não transfere direitos sobre pacotes terceiros. Antes de distribuir/republicar binários há gate separado de licença, licença por pacote, permissões e identificação de fonte.

**Gates:** `SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM`; `TOKEN_VAZIO != 0`; `IMPLEMENTED_UNTESTED != PASS`. PR source-side não equivale a CI PASS; exigem-se testes da versão exata e, depois, bytes instalados no Android. `claim_allowed=false`.

**Rollback:** fechar a PR ou reverter script + workflow + docs; o nome e conteúdo mínimo do artefato legado foram preservados.

**7M:** Meta = simplificar custódia; Mapa = produtor/consumidor; Medida = ZIP + manifest + outputs; Mudança mínima = um workflow autoritário; Matriz = self-test/syntax/contract/exact CI; Manifesto = `ZIPRAF_MANIFEST.json`; Memória = receipts/Drive HOTSTATE, apenas após evidência.
