# APT → Bootstrap → Ninja — contrato V1

Estado: SOURCE_IMPLEMENTED_HOST_TESTED; Android físico = TOKEN_VAZIO.
Contrato: `rafcodephi.apt-bootstrap-ninja/v1`.

| Fronteira | Autoridade e obrigação |
| --- | --- |
| Fonte/pacotes | [termux-packages](https://github.com/rafaelmeloreisnovo/termux-packages): construir `ninja` e suas dependências para `com.termux.rafacodephi`, ARM32 e ARM64; preservar receita, licença, SHA e prefixo. Nome anterior: termux-packagesMy. |
| Bootstrap | Exigir `bin/ninja`, registro `Package: ninja` com `Status: install ok installed`, ELF e contrato idêntico no manifesto, BOOTSTRAP_PROFILE.json e BOOTSTRAP_INFO. Não aceitar bridge como Ninja real. |
| App | [termux-app-rafacodephi](https://github.com/rafaelmeloreisnovo/termux-app-rafacodephi): validar as três declarações, ELF da ABI correta, prefixo e dpkg antes de consumir. Payload antigo continua legado; não recebe evidência de Ninja. |
| APT | HTTPS, chave externa pinada e Signed-By mantidos. Atualização parcial ou qualquer etapa falha bloqueia a cadeia e não gera receipt de sucesso. |
| Aparelho | Confirmar execução de um grafo Ninja e bytes do resultado; `--version` sozinho não fecha o gate. Hashes dos executáveis reais no receipt. |

Rotas de implementação:

- Produtor: `scripts/build-rafcodephi-real-bootstrap.sh` → `scripts/build-rafcodephi-live-bootstrap.sh` → `.github/workflows/rafcodephi-publish-dev-apt.yml`.
- Consumidor: `scripts/import_rafcodephi_real_bootstrap.py` → `scripts/bootstrap_pair_transaction.py` → `scripts/prepare_bootstrap_env.sh` → `scripts/device_signed_apt_smoke.sh`.
- Testes: contratos locais de workflow, `test_apt_bootstrap_ninja_contract.py` e `test_device_signed_apt_smoke.py`.
- Proveniência desta mudança: [receipt local](receipts/MU-TERMUX-APT-BOOTSTRAP-NINJA-20261103-S1.json).

No app instalado e com chave pública conferida por canal independente:

```sh
RAF_APT_TRUSTED_KEY_SHA256=<sha256_conferido> sh scripts/device_signed_apt_smoke.sh --check
RAF_APT_TRUSTED_KEY_SHA256=<sha256_conferido> sh scripts/device_signed_apt_smoke.sh --ninja-smoke
```

`--check` só verifica o estado local. `--ninja-smoke` cria e remove um diretório temporário no prefixo para executar o grafo. `--install-ninja` é opt-in: faz apt update estrito, instala Ninja pelo repositório assinado e executa o mesmo grafo. Os comandos precisam do script desta revisão no aparelho. Não remover o bloqueio APT de um bootstrap não publicado.

Dependências e autoria:

- Ninja 1.13.2, upstream ninja-build/ninja, Apache-2.0, fonte SHA256 `974d6b2f4eeefa25625d34da3cb36bdcebe7fbce40f4c16ac0835fd1c0cbae17`, conforme receita `packages/ninja/build.sh` neste baseline. Runtime herdado C++/libc++ permanece explícito; não é reclassificado como núcleo autoral freestanding. A substituição requer compatibilidade de grafo e testes independentes.
- Shell, APT/dpkg, Android e Python stdlib são fronteiras hosted/plataforma existentes. Esta correção não adiciona biblioteca Python nem relicencia código de terceiros.

Gates abertos: build real dos novos DEBs e APK, chave/repositório de produção publicado, instalação e grafo no Android físico com identidade do APK/bootstrap/executáveis. Um fixture sintético não fecha esses gates.
Rollback: fechar PR não integrado; se integrado, revert revisado dos caminhos deste hotfix. Preservar commits, receitas, bootstrap anterior e receipts; não usar force-push ou apagar histórico. Nenhuma proteção de branch ou credencial é alterada por este contrato.

SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM. `claim_allowed=false` para runtime/release sem os gates correspondentes.
