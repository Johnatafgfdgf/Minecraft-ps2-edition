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
- Kernel `ImprovedNoise`, incluindo consumo do PRNG e amostragem binary64 exata.
- Coordenadas BlockPos/SectionPos/ChunkPos e layout SimpleBitStorage.
- Fila de ticks pendentes por chunk, com identidade, deduplicação e prioridades.
- Importação privada da referência oficial com verificação de hashes.
- Harness que invoca os métodos do JAR original por reflection, sem copiar código.
- Registro nativo de todos os block states/propriedades e transições equivalentes.
- Gerador privado de registries, world data pack e conversão de texturas pessoais.

**Paridade no host:** 1.313 cenários, 75.908 resultados iguais ao servidor original
1.21.1. Além disso, **33.721 transições de block states** e **32.896 observações de
ImprovedNoise** coincidiram com as chamadas originais: **142.525 resultados** no
total. Todos os 26.684 estados também foram conferidos contra o report do jogo.
Isso não valida geração completa, redstone, física nem execução no console.
O ELF foi compilado para Emotion Engine; boot e desempenho em PS2/PCSX2 ainda
precisam ser medidos. Cinco probes pequenos executam no ELF e mostram sua máscara
de aprovação; não substituem a suíte completa.

```sh
make test
make ps2     # após configurar PS2DEV, PS2SDK e GSKIT
make parity  # após importar a referência e configurar JDK 21
make reference-data && make registry-parity
make noise-parity
```

- [Build reproduzível](docs/BUILD.md)
- [Correspondência com a arquitetura original](docs/PORT_MAPPING.md)
- [Comportamentos analisados e limites dos testes](docs/PARITY.md)
- [Próximas etapas do port](docs/ROADMAP.md)
- [Pipeline privada de conteúdo e formatos](docs/CONTENT_PIPELINE.md)
- [Builds e ELF no GitHub Actions](https://github.com/Johnatafgfdgf/Minecraft-ps2-edition/actions)
- [Dependências e avisos de terceiros](docs/THIRD_PARTY.md)

O projeto não é afiliado à Mojang ou à Microsoft. Seus arquivos originais não
integram o repositório.
