# Termux Packages scripts/ci — mapa operacional 2026-10-25

A pasta scripts/ci contém verificadores de pacotes e recibos. Os arquivos de código não demonstram que um aparelho recebeu e instalou os pacotes.

| Caminho | Responsabilidade observável |
|---|---|
| [rafcodephi_prepare_ndk29.sh](rafcodephi_prepare_ndk29.sh) | Preparar toolchain NDK29 e exigir pin de origem. |
| [rafcodephi_ndk_preflight.sh](rafcodephi_ndk_preflight.sh) | Verificação inicial da toolchain. |
| [rafcodephi_parallel_batches.py](rafcodephi_parallel_batches.py) | Comprovar separadamente lote arm/aarch64 e juntar apenas recibos válidos. |
| [rafcodephi_bootstrap_prefix_gate.py](rafcodephi_bootstrap_prefix_gate.py) | Validar prefixos de instalação. |
| [rafcodephi_deb_set_gate.py](rafcodephi_deb_set_gate.py) | Validar conjunto DEB. |
| [rafcodephi_failure_triage.py](rafcodephi_failure_triage.py) | Triagem de falhas. |
| [rafcodephi_producer_zipraf.py](rafcodephi_producer_zipraf.py) | Embalagem e integridade de artefatos. |
| [rafcodephi_555_compose.py](rafcodephi_555_compose.py) | Composição controlada de registros. |

O gate de docs aqui reside em scripts/ci e requer --root . quando chamado da raiz. Não inventar pinos ou assinar pacotes sem autoridade. CI/script PASS != instalação física. Risco de segredo e falha de supply chain exige revisão humana. claim_allowed=false; HOLD.
