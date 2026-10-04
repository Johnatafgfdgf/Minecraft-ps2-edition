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

## Implementado e verificado nesta etapa

- ELF nativo com gsKit/GIF DMA, leitura de controle e ponte POSIX do PS2SDK.
- Relógio de simulação separado dos frames, sem descartar dívida de ticks.
- PRNGs Legacy/Xoroshiro, forks, reseeding e derivação de seeds de worldgen.
- Coordenadas BlockPos/SectionPos/ChunkPos e layout SimpleBitStorage.
- Fila de ticks pendentes por chunk, com identidade, deduplicação e prioridades.
- Importação privada da referência oficial com verificação de hashes.
- Harness que invoca os métodos do JAR original por reflection, sem copiar código.
- Registro nativo de todos os block states/propriedades e transições equivalentes.
- Gerador privado de registries, world data pack e conversão de texturas pessoais.

**Paridade no host:** 1.313 cenários, 75.908 resultados iguais ao servidor original
1.21.1. Além disso, **33.721 comparações de transições de block states** coincidiram
com `StateHolder.setValue` original, e todos os 26.684 estados foram conferidos
contra o report do jogo. Isso não valida geração completa, redstone, física nem execução no console.
O ELF foi compilado para Emotion Engine; boot e desempenho em PS2/PCSX2 ainda
precisam ser medidos.

```sh
make test
make ps2     # após configurar PS2DEV, PS2SDK e GSKIT
make parity  # após importar a referência e configurar JDK 21
```

- [Build reproduzível](docs/BUILD.md)
- [Correspondência com a arquitetura original](docs/PORT_MAPPING.md)
- [Comportamentos analisados e limites dos testes](docs/PARITY.md)
- [Próximas etapas do port](docs/ROADMAP.md)
- [Pipeline privada de conteúdo e formatos](docs/CONTENT_PIPELINE.md)

O projeto não é afiliado à Mojang ou à Microsoft. Seus arquivos originais não
integram o repositório.
