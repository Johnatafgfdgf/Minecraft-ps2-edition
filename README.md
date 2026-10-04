# Minecraft PS2 Edition — port de comportamento da Java 1.21.1

Implementação nativa independente em C++ para o PlayStation 2, usando a versão
original 1.21.1 como referência de comportamento. O objetivo é preservar as regras
do jogo enquanto a camada de execução é substituída por PS2SDK e gsKit.

**Estado atual:** desenvolvimento inicial. O repositório não contém um Minecraft
completo, e inicializar um ELF não significa concluir o port. Sistemas só serão
marcados como verificados após comparação com a referência original. A meta de
30 FPS ainda precisa de medições em hardware real.

O código, os JARs, os mappings e os assets distribuídos pela Mojang não devem ser
publicados aqui. Referências e conteúdo importado para uso pessoal ficam apenas
em diretórios locais ignorados pelo Git. O cliente deverá ser fornecido a partir
de uma instalação legítima; a análise inicial das regras pode usar o servidor
1.21.1 distribuído publicamente no site oficial.

Documentação, build nativo e testes de paridade serão adicionados nos próximos
commits deste repositório.
