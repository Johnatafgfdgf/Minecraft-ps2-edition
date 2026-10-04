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
- Factories posicionais, hash UTF-16 do Java e seeds MD5/UTF-8, incluindo Unicode.
- Kernel `ImprovedNoise`, incluindo consumo do PRNG e amostragem binary64 exata.
- `PerlinNoise` e `NormalNoise`, com octaves esparsas, modo Legacy e parâmetros vanilla.
- `BlendedNoise`, com 40 octaves, seleção dos campos de densidade, bounds e reseeding.
- Grafo de densidade com operações aritméticas, gradientes, limites e avaliação de ramos na ordem original.
- Simplex 2D e cálculo das ilhas do End, preservando PRNG, floats e overflow originais.
- Splines Hermite com valores aninhados, limites binary32 e avaliação sem recursão.
- Ligação dos ruídos à seed; conversor/leitor MCDG de todos os 105 campos vanilla, comparados ao RandomState original.
- Cinco caches de NoiseChunk, interpolação e ciclo de células com buffers externos e ponte para campos de densidade.
- Coordenadas BlockPos/SectionPos/ChunkPos e layout SimpleBitStorage.
- Fila de ticks pendentes por chunk, com identidade, deduplicação e prioridades.
- Importação privada da referência oficial com verificação de hashes.
- Harness que invoca os métodos do JAR original por reflection, sem copiar código.
- Registro nativo de todos os block states/propriedades e transições equivalentes.
- Gerador privado de registries, world data pack e conversão de texturas pessoais.

**Paridade no host:** **941.623 resultados coincidentes** com chamadas ao servidor
original 1.21.1 em onze suítes: primitivas, block states, ImprovedNoise, factories,
Perlin/Normal, BlendedNoise, Simplex/End, operações, splines, dados de densidade e NoiseChunk.
Os testes numéricos comparam bits de float/double sem tolerância;
os testes de densidade/Simplex normalizam somente payloads de NaN.
Todos os 26.684 estados também foram conferidos contra o report do jogo.
Uma suíte adicional compara 65.536 registros de expressões Java 21 com a camada
binary32 nativa, cujo caminho compilado para EE é auditado para evitar depender
de operações single-precision do console nas splines e ilhas do End.
Isso não valida geração completa, redstone, física nem execução no console.
O ELF foi compilado para Emotion Engine; boot e desempenho em PS2/PCSX2 ainda
precisam ser medidos. Dezesseis probes pequenos executam no ELF e mostram sua máscara
de aprovação; não substituem a suíte completa.

```sh
make test
make ps2     # após configurar PS2DEV, PS2SDK e GSKIT
make parity  # após importar a referência e configurar JDK 21
make reference-data && make registry-parity
make noise-parity
make simplex-parity
make float-parity
make factory-parity
make octave-parity
make blended-parity
make density-parity
make density-spline-parity
make density-data-parity
make noise-chunk-parity
```

- [Build reproduzível](docs/BUILD.md)
- [Correspondência com a arquitetura original](docs/PORT_MAPPING.md)
- [Comportamentos analisados e limites dos testes](docs/PARITY.md)
- [Próximas etapas do port](docs/ROADMAP.md)
- [Pipeline privada de conteúdo e formatos](docs/CONTENT_PIPELINE.md)
- [Núcleo numérico da geração](docs/NUMERIC_WORLDGEN.md)
- [Operações e avaliação do grafo de densidade](docs/DENSITY_GRAPH.md)
- [Células, interpolação e caches de NoiseChunk](docs/NOISE_CHUNK.md)
- [Builds e ELF no GitHub Actions](https://github.com/Johnatafgfdgf/Minecraft-ps2-edition/actions)
- [Dependências e avisos de terceiros](docs/THIRD_PARTY.md)

O projeto não é afiliado à Mojang ou à Microsoft. Seus arquivos originais não
integram o repositório.
