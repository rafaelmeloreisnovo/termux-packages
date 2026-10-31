# Licenças, autoria e dependências — RAFCODEΦ middleware (V1)

A fonte normativa da árvore é [LICENSE.md](../../../LICENSE.md): a infraestrutura de construção fora dos diretórios de receitas é licenciada sob Apache License 2.0; scripts/patches de pacotes seguem as licenças dos respectivos pacotes. Esta nota não altera termos, propriedade ou avisos existentes.

- As alterações **novas neste ciclo** são verificações de contrato Python stdlib, caminhos YAML e documentação local; sua autoria específica não implica exclusividade sobre GitHub Actions, Termux, Android, QEMU, ferramentas GNU, PyYAML ou receitas upstream.
- GitHub Actions de terceiros e imagens Docker pinadas permanecem dependências de **build/hospedagem**. O pin é requisito de identidade; a licença e a manutenção de cada fornecedor continuam independentes.
- `C11 freestanding` só qualifica o núcleo testado nesse perfil; `GCC/Clang`, ELF linker, CI, sistema hospedeiro, assinaturas APT e Android **não** se tornam freestanding ou autorais por herança.
- Nenhum código de terceiro, binário/firmware ou biblioteca de fornecedor foi copiado, relicenciado, removido ou substituído nesta alteração.
- Antes de qualquer reimplementação de biblioteca/rotina herdada: registrar caminho original, titularidade, licença, função, vetor de regressão, resultado equivalente, custo, autorização, receipt e rollback. Se não houver fonte/autoridade, `TOKEN_VAZIO_LICENSE` e `HOLD`.
- Os arquivos [README](README.md), [contrato de teste](../../../scripts/tests/test_rafcodephi_batch_pipeline_contract.py) e [falsificadores](../../../scripts/tests/test_rafcodephi_batch_pipeline_falsifiers.py) servem como mapa navegável humano/IA. Eles **não** substituem avisos SPDX ou parecer jurídico.

Status: `DRAFT/HOLD`; nenhum release ou publicação comercial autorizado por este documento.
