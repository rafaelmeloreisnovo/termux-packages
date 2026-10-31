# Termux Packages RAFCODEΦ — estado verificável e handoff (31/10/2026)

> **DRAFT/HOLD · documentação, não entrega de pacotes.** Corte: `rafaelmeloreisnovo/termux-packagesMy@c56d34d330ff8ca621c93bbe8c36d6ab52f3844e` (`main` na leitura). `claim_allowed=false`, `release_allowed=false`. Informações históricas não são receipts de builds atuais.

## Autoridade e navegação

Esta é a **fábrica de fontes/receitas/pacotes**, não o consumidor APK. App consumidor: [`termux-app-rafacodephi`](https://github.com/rafaelmeloreisnovo/termux-app-rafacodephi). O experimento [`termux-packages-rafcode`](https://github.com/rafaelmeloreisnovo/termux-packages-rafcode) testa um núcleo C freestanding isolado, **não** substitui esta árvore de receitas. Fontes: [`AGENTS.md`](../AGENTS.md), [`README.md`](../README.md), [`build-package-rafcodephi.sh`](../build-package-rafcodephi.sh), [`scripts/build-rafcodephi-real-bootstrap.sh`](../scripts/build-rafcodephi-real-bootstrap.sh), [`docs/RAFCODEPHI_DELIVERY_CONTRACT_V1.md`](RAFCODEPHI_DELIVERY_CONTRACT_V1.md), [`docs/RAFCODEPHI_SIGNED_APT_RELEASE_V1.md`](RAFCODEPHI_SIGNED_APT_RELEASE_V1.md).

## Pipeline comprovável por inspeção de código

1. `build-package-rafcodephi.sh` aplica a sobreposição de `scripts/properties.sh` via `scripts/apply-rafcodephi-build-properties.py`, escreve receipt JSON, chama `scripts/validate-rafcodephi-build-properties.sh` e recusa execução se o SHA-256 de `properties.sh` diferir do `sha256_after` do receipt; então executa `./build-package.sh "$@"`.
2. `scripts/build-rafcodephi-real-bootstrap.sh` usa `scripts/generate-bootstraps.sh --build`, declara `bridge_allowed=false` e `legacy_prefix_allowed=false` e restringe seu primeiro gate real a `arm` e `aarch64`. `i686/x86_64` não estão nessa rota source-built inicial.
3. O manifesto do builder declara `schema=rafcodephi.real-bootstrap-sourcebuild/v1` e `package_repo_runtime_state=BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED`. **O contrato não é prova de que o ZIP real foi construído ou que o APT tem origem publicada**.
4. O workflow [`rafcodephi-real-bootstrap.yml`](../.github/workflows/rafcodephi-real-bootstrap.yml) contém job de fonte real, seletores de arquiteturas, preflight e testes; presença de YAML não prova execução/artefato. Consultar sempre run, job, head SHA e digest exatos.
5. [`RAFCODEPHI_SIGNED_APT_RELEASE_V1.md`](RAFCODEPHI_SIGNED_APT_RELEASE_V1.md) define a custódia de índices e assinatura APT; `SIGNED_REPOSITORY_READY != PUBLISHED` e uma publicação eventual ainda não prova `pkg update` no telefone.

## Comandos orientativos (não executados por esta revisão)

```sh
# Ambiente de build apropriado; um pacote concreto deve ser escolhido e auditado:
./build-package-rafcodephi.sh -i <nome-do-pacote>

# Validação estrutural de atribuição e componentes federados:
python3 scripts/validate_rafcodephi_federated_provenance.py

# Construção de bootstrap real exige dependências de build e conjunto fonte disponível:
bash scripts/build-rafcodephi-real-bootstrap.sh --architectures arm,aarch64
```

Nunca declarar `PASS` apenas pelo comando estar documentado. Reexecutar só com ambiente apropriado, observando licença da receita, fonte original, hashes de entrada, pacote `.deb`, assinatura, índice APT e falhas reais. A receita é majoritariamente upstream/terceiros; a camada RAFCODEΦ é aditiva e deve preservar créditos, licenças e notices por arquivo.

## Divergências de documentação

- [`docs/00-INDEX.md`](00-INDEX.md) é índice de **17/08/2026**, com rótulos “Production” / “Complete”; tais rótulos não demonstram readiness no HEAD atual.
- [`RAFAELIA_DOCS_RECONCILIATION_20261025.md`](RAFAELIA_DOCS_RECONCILIATION_20261025.md) usa a identidade histórica `rafaelmeloreisnovo/termux-packages`; o repositório **efetivamente acessível nesta revisão** é `termux-packagesMy`. Preservar o registro histórico, mas não usá-lo como identificador de execução.
- O README descreve a origem Google Play/Termux upstream. Isso não torna este fork um repositório de APKs nem prova conformidade/publicação deste canal RAFCODEΦ.

## Gate de handoff ao aplicativo

Requer, separadamente: receita + commit de origem → artefato `.deb` + SHA-256 + arquitetura → metadados APT + assinatura/cadeia de custódia → bootstrap ZIP + manifesto → APK consumidor com digest → device `pkg/apt/dpkg/proot` e teste negativo. `arm` da fábrica mapeia para `armeabi-v7a` no APK; `aarch64` mapeia para `arm64-v8a`.

**F_ok:** arquivos de recipe/build/bootstrap/workflow e gates existentes observados em fonte. **F_gap:** publicação real de APT, saída `.deb` / ZIP executada no HEAD, validação CI exata, instalação ARM32/ARM64 e atualização de pacotes no celular = `TOKEN_VAZIO_NOT_RUN`. **F_next:** selecionar 1 receita essencial e comprovar cadeia D1→D3 com digest antes de D4/APT e antes do consumo APK. **Rollback:** fechar PR documental sem merge; nenhum script, receita ou licença alterados.
