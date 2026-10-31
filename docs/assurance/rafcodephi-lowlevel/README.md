# RAFCODEΦ low-level middleware — fronteiras e perfil de segurança (V1)

**Estado:** design scoped + batch YAML guard implemented on a draft branch. **Not** an Android device/ELF/GPU proof. `claim_allowed=false`, `release_allowed=false`.

## Autoridade e fontes
- Fábrica: [termux-packagesMy](../../../README.md) e [contrato de entrega](../../RAFCODEPHI_DELIVERY_CONTRACT_V1.md).
- Publisher/rollback permanece [rafcodephi-publish-dev-apt.yml](../../../.github/workflows/rafcodephi-publish-dev-apt.yml).
- Prévia sem publicação: [rafcodephi-batch-pipeline.yml](../../../.github/workflows/rafcodephi-batch-pipeline.yml).
- Testes: [contrato](../../../scripts/tests/test_rafcodephi_batch_pipeline_contract.py) e [falsificadores](../../../scripts/tests/test_rafcodephi_batch_pipeline_falsifiers.py).
- Licenças e origem: [LICENSES.md](LICENSES.md) e [LICENSE.md](../../../LICENSE.md).

## Caminho mínimo determinístico
`SOURCE_PIN + RECIPE → PREFLIGHT_L0 → SOURCE_BUILD_ARM/ARM64_L1 → DEB/ZIP_SHA_L2 → APT_TEST_SIGNED_L3 → RELEASE_HOLD`.

A implementação pure C11/ELF freestanding, quando escolhida e medida, executa lógica limitada de cálculo/verificação; não substitui o compilador, o construtor Debian, a criptografia da assinatura APT, o Android/ART, o QEMU nem o GitHub Actions. Nenhum subprocesso, rede, heap, libc, Java, DEX, syscalls ou driver é permitido **dentro do núcleo puro**. O adaptador hospedado é explicitamente externo e pode usar mecanismos da plataforma que o contrato exigir. Remover funções existentes apenas quando houver equivalência comprovada.

## Perfis de capacidade (ordem de decisão, não alegação de disponibilidade)

| Camada | Perfil seguro | Condição para promoção |
| --- | --- | --- |
| C/ELF | `SCALAR_REFERENCE` | vetor de referência/ABI, sem UB, digest |
| SIMD/NEON ARM32 | `DISABLED_UNTIL_PROVEN` | artefato ARMv7 e detecção real de suporte NEON |
| SIMD/NEON ARM64 | `DISABLED_UNTIL_PROVEN` | artifact AArch64, equivalência bit a bit e benchmark |
| GPU | `TOKEN_VAZIO_DEVICE_DRIVER` | driver/API permitida, testes de resultado e custo total |
| NUMA | `TOKEN_VAZIO_TOPOLOGY` | topologia realmente disponível e medida, não presumida |
| ART/DEX/JNI | `HOSTED_BOUNDARY` | integração Android no repo consumidor, não neste YAML |
| Duplo watchdog | `PROPOSED_SEPARATE_SCOPES` | monitor independente de progresso + integridade, tempos/erros medidos |

O custo inclui CPU, memória, energia, transferência para GPU, cache, bateria, setup e armazenamento; desempenho SIMD ou GPU **não** é inferido pelo nome do recurso. Sem medição válida, usar o escalar reproduzível. Arquiteturas `arm` e `aarch64` são produtores distintos; agregação somente com ambos os receipts presentes.

## Contenção de falhas e limites
1. **Fail-safe:** identidade não verificada, checksum inválido, metadado ausente, permissões inseguras ou fonte não pinada → parar a etapa relevante, salvar recibo BLOCKED.
2. **Fallback:** escolher apenas implementação de referência já testada, nunca reinterpretar ausência de NEON/GPU como sucesso otimizado.
3. **Failover:** só alternar para outro produtor assinante se identidade, permissões, versão de protocolo, artefato e custo forem validados. **Não ativado aqui.**
4. **Rollback:** manter o publisher anterior e o SHA de base; reverter somente o delta da branch sem reescrever histórico.
5. **Watchdogs:** o primeiro observa prazo/progresso do trabalho; o segundo valida integridade/identidade e decisão. Se discordarem, `HOLD`; não criar ciclo de relançamento.
6. **Orçamento:** PR = L0 barato; builds caros ARM/ARM64 e APT local apenas via dispatch opt-in; não publicar, não pedir chaves de produção no PR.

## Critérios de aceitação/falsificação
- Parser de todos os workflows e Bash incorporado PASS no exato HEAD; o nome `on` deve ser preservado pelo loader adequado.
- Falsificadores devem bloquear: `Trusted: yes` positivo, remoção da negação, `contents: write`, dispatch caro ligado por padrão, remoção de gatilho ou de upload de falhas.
- O comando `! grep -Fq 'Trusted: yes' ...` deve **passar**, pois rejeita a configuração insegura; `grep -Fq ...` sem negação deve falhar.
- CI atual, build fonte, .deb real, assinatura de publicação, ARM32/ARM64 no dispositivo e performance permanecem `TOKEN_VAZIO` até receipts com exato SHA.
- Nenhum `PASS_SCOPED` do contrato pode ser promovido para `DEVICE_PROVEN` ou `RELEASE_ALLOWED`.

## Receipt mínimo
`event|repo@sha|source/path|scope|classification|action|negative-tests|execution|artifact_sha256|evidence|gap|rollback|claim_allowed=false`.

**SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM.** `TOKEN_VAZIO ≠ 0`.
