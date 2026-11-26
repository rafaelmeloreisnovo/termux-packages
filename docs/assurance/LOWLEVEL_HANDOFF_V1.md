# Handoff do núcleo low-level para o consumidor

Este repositório é produtor de fontes, receitas e handoff. Não promove instalação ou runtime do app.

O handoff mínimo deve ligar:

- commit exato do produtor;
- módulo/receita;
- ABI alvo;
- hashes das fontes e artefatos;
- versão do contrato;
- expectativa do consumidor;
- receipt de validação;
- rollback;
- lacunas tipadas.

O package não copia a biblioteca Java do app. A camada Java/DEX pertence ao consumidor Android; o package pode fornecer somente componentes PURE_CORE ou PLATFORM_GATE cuja proveniência e licença sejam preservadas.

'IMPLEMENTED_UNTESTED' não é PASS. Compilação do package não prova instalação, DEX, APK ou execução física.
