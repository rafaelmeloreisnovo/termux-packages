# RAFAELIA — Reconciliação documental de termux-packages — 2026-10-25

> **DRAFT / HOLD · Não promove claims, releases, versões nem direitos.** Documento adicional, não substitui predecessores. A única verdade do inventário de arquivos aqui é a árvore Git no HEAD exato descrito abaixo; o conteúdo operacional exige revisão específica.

## Corte e escopo

- Projeto: `rafaelmeloreisnovo/termux-packages`.
- HEAD ancestral: `fa4f4ffa637ce07ca4587609c5d6c15d441e73cc`.
- Árvore Git completa: **9.542 arquivos, 3240 diretórios**, `truncated=false`.
- Documentos/textos por extensão: **116**; isto inclui documentação potencial, não necessariamente páginas publicadas.
- Diretórios raiz: **16**, dos quais **16 sem README/INDEX/FILES_MAP local direto** (critério heurístico, não prova de orfandade).
- Escopo de interesse `scripts/`: **209 arquivos, 19 subdiretórios** — são arquivos, não aplicativos únicos.
- Função técnica: receitas de pacotes Termux, construção ARM/AArch64, bootstrap real, APT e custódia.

## Divergência já observada

docs/00-INDEX.md tem revisão agosto/2026 e total histórico de 35 documentos; a contagem de 116 inclui documentos fora do hub e não deve ser confundida com páginas indexadas em docs/.

O anterior não será apagado, renomeado ou sobrescrito silenciosamente. Quando houver conflito, prevalecem **fonte versionada → teste do SHA → recibo verificável**, não um selo de "completo" herdado de documento antigo.

## Roteamento atualizado

| Referência existente (verificada no snapshot) | Papel |
| --- | --- |
| [`README.md`](../README.md) | guia ou índice histórico |
| [`docs/00-INDEX.md`](../docs/00-INDEX.md) | guia ou índice histórico |
| [`LICENSE.md`](../LICENSE.md) | fonte ou contrato em versão Git |
| [`THIRD_PARTY_PROVENANCE.md`](../THIRD_PARTY_PROVENANCE.md) | fonte ou contrato em versão Git |
| [`scripts/build-rafcodephi-real-bootstrap.sh`](../scripts/build-rafcodephi-real-bootstrap.sh) | fonte ou contrato em versão Git |
| [`scripts/ci/rafcodephi_prepare_ndk29.sh`](../scripts/ci/rafcodephi_prepare_ndk29.sh) | fonte ou contrato em versão Git |
| [`scripts/build/termux_step_start_build.sh`](../scripts/build/termux_step_start_build.sh) | fonte ou contrato em versão Git |
| [`scripts/generate-bootstraps.sh`](../scripts/generate-bootstraps.sh) | fonte ou contrato em versão Git |
| [`packages`](../packages) | fonte ou contrato em versão Git |
| [`x11-packages`](../x11-packages) | fonte ou contrato em versão Git |
| [`root-packages`](../root-packages) | fonte ou contrato em versão Git |

**Áreas prioritárias de classificação e dono:** `packages/`, `x11-packages/`, `root-packages/`, `disabled-packages/`, `scripts/`, `tests/`, `docs/`, `configs/`, `core/`. Estes caminhos são agrupamentos propostos, sem mudança física na árvore.

## Pastas sem índice local (amostra para triagem)

`scripts/ci/`, `scripts/build/`, `scripts/rafcodephi-freestanding/`, `scripts/tests/`, `ndk-patches/`, `profiles/`, `core/`, `site/`.

Um diretório listado não é automaticamente redundante, abandonado ou livre para remoção. Identificar dono, consumidores, licença e predecessores antes de qualquer reorganização.

## Formato mínimo para cada arquivo / aplicação

`PATH | GIT_BLOB_SHA | PRODUCER | CLASS | CAPABILITY | INPUT | OUTPUT | SIDE_EFFECT | DEPENDENCIES | LICENSE | TEST_SHA | EVIDENCE | OWNER_ROLE | GAP | ROLLBACK`.

Estados: `OBSERVED_SOURCE`, `DOCUMENTED`, `TESTED_SCOPED`, `RUNTIME_PROVEN`, `TOKEN_VAZIO_NOT_TESTED`, `SUPERSEDED`. Ausência não é zero; script listado não equivale a execução.

## Refatoração sem regressão

1. Inventário recursivo pinado ao HEAD e com preservação do blob SHA (não copiar dados privados na documentação pública).
2. Verificar se referências relativas apontam para caminhos realmente existentes; documentos históricos com links quebrados recebem registro de divergência.
3. Priorizar pastas sem índice local de acordo com impacto, não apenas ordem alfabética.
4. Criar `README` por diretório funcional somente após leitura de fontes e consumidores; não inventar interfaces ou capacidades.
5. Separar o que é código próprio, upstream e wrapper, preservando licenças e direitos de terceiros.
6. Atualizar índice de entrada por ponteiro para o documento sucessor; nunca apagar históricos, artefatos autorais ou branches.
7. Exigir revisão humana, CI exata e gate do provedor para autorizar merge; `claim_allowed=false` até a evidência aplicável.

## Falsificadores e fechamento R3

- **F_ok**: árvore recursiva consultada em HEAD exato; alvos da tabela de links checados na árvore base.
- **F_gap**: auditoria semântica total, teste de build/ABI/hardware, checagem exaustiva de links históricos, inventário jurídico e proteção de merge ainda NÃO realizados.
- **F_next**: link-checker negativo (alvo faltante), manifesto canônico por arquivo, revisão de licenças e prioridade P0 das pastas operacionais.
- **Rollback**: reverter somente commits de documentação do PR; preservar todos os predecessores.
