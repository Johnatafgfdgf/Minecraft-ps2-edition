# Análise e validação

## Proveniência

Referência: servidor Java **1.21.1**, obtido da distribuição pública oficial da
Mojang e mantido localmente. SHA-1 do bundle:
`59353fb40c36d304f2035d51e7d6e6baa98dc05c`. SHA-1 dos mappings:
`03f8985492bda0afc0898465341eb0acef35f570`. DataVersion 3955, protocolo 767.

Os mappings localizaram os componentes. `javap -p -c` foi usado localmente para
verificar chamadas, constantes e tipos de operações de `LegacyRandomSource`,
`BitRandomSource`, `Xoroshiro128PlusPlus`, `XoroshiroRandomSource`, `RandomSupport`,
`WorldgenRandom`, `BlockPos`, `SectionPos`, `SimpleBitStorage`, `ScheduledTick`,
`ImprovedNoise`, factories posicionais, `RandomSupport.seedFromHashOf`, a base de
gradientes de `SimplexNoise`, seed de coordenadas e a interpolação de `Mth`.
Esse bytecode/material de análise não é publicado. O oracle usa reflection para
executar os métodos originais, com todas as bibliotecas do bundle verificadas.

## Regras implementadas

- **Legacy:** estado de 48 bits; reseed com XOR/máscara; nextInt(bound) preserva
  o teste de rejeição com overflow Java de 32 bits. `nextLong` soma o segundo
  `int` com extensão de sinal, em vez de simplesmente concatenar duas palavras.
- **Xoroshiro:** upgrade de seed pelo mixer Stafford13, estado de 128 bits,
  fallback para estado zero, rotações e overflow de 64 bits. nextInt usa os bits
  baixos; float/double usam os bits altos. Bound usa produto/rejeição unsigned.
- **WorldgenRandom:** wrapper distingue Legacy de Xoroshiro; derivação preserva
  a largura das multiplicações antes da promoção para long, inclusive overflow
  do passo `10000 * step` e das parcelas de slime chunks.
- **ImprovedNoise:** offsets, permutação e consumo do PRNG preservados; piso Java
  explícito e mesma ordem das operações de gradientes/interpolação em binary64.
  A quantização vertical usa o epsilon binary32 original promovido para double.
  Ela muda o Y do gradiente, enquanto o fade usa a fração de Y não quantizada.
  Este kernel amostra valores; derivadas, octaves e density functions são pendentes.
- **Posições:** packing e sign extension são explícitos. BlockPos, SectionPos,
  índice local XZY e índice de storage YZX têm layouts distintos. Coordenadas
  negativas usam divisão por seção com piso, não truncamento para zero.
- **Storage:** valores não atravessam uma palavra de 64 bits. Os bits restantes
  de cada palavra são padding; essa diferença é relevante em larguras como 5/6/7.
  O núcleo usa memória externa de arena/pool e não aloca para get/set.
- **Fila por chunk:** identidade é (tipo canônico, posição). Um duplicado não
  antecipa/substitui um tick já existente. Ordem pending: trigger tick, prioridade,
  subTickOrder. Ao poll, a identidade fica disponível novamente. A coleta global
  `LevelTicks` possui outra ordem e ainda precisa ser portada.

## Testes executados — 2026-10-04

`make test`: verificações nativas de limites, dívidas do relógio, overflow,
armazenamento, capacidade, deduplicação, ordem e oito probes de boot; nove testes Python da pipeline
e proteção dos inputs. `make parity`: **1.313 cenários / 75.908 resultados iguais**
ao JAR original, incluindo sequences intercaladas, seeds negativas/extremas,
reseed, forks, limites com rejeição, coordenadas negativas/extremas, layout bruto
de storage de 1 a 32 bits e filas com duplicados/prioridades.

SHA-256 do stream de observações coincidentes:
`fa74a1976354f3a9a71faf5eab60952cfb2f33ac8be101dc215c0903a9a0a796`.
Inputs e resultados completos podem ser regenerados em `.local/parity/`.

`make reference-data`: o data generator original foi executado. Todos os **1.060
blocos e 26.684 states**, incluindo IDs, defaults e pares chave/valor, passaram
pelo leitor nativo e coincidiram integralmente com `blocks.json` original. O MCSR
gerado ocupa **607.985 bytes**, com 399 pares de propriedades e 118.970 referências.
Foram preservados mapas de IDs de 78 registries e 6.796 entradas de dados num pack
privado; estes dados ainda precisam dos interpreters de gameplay.

`make registry-parity`: **33.721 cenários/resultados iguais** a chamadas originais
de `StateHolder.setValue`, cobrindo todos os estados com propriedades, todos os
valores sobre default states e entradas inválidas. SHA-256 das observações:
`d8557171f49123d232e7f8515f3602c5cd8ab631dbb0ad1d26f5150ca7f10f47`.
Isso verifica transição de estado, não callbacks, updates ou mecânicas dos blocos.

`make noise-parity`: **64 cenários / 32.896 observações iguais**, incluindo os
offsets, próximo valor do PRNG após construção, amostragem de três argumentos e
amostragem de cinco argumentos com quantização Y. Compara os bits de double,
sem tolerância aproximada, para 16 seeds em quatro variantes de PRNG, 256 posições
por caso, limites de células e coordenadas negativas de grande magnitude.
SHA-256: `124c3c111edcb8b1cc09caac4d30d16fae7f1ea8cd1a6c0bfc1211080cc371dc`.
A conferência do report de estados é uma validação adicional distinta.

`make factory-parity`: **3.072 cenários / 35.200 resultados iguais**, incluindo
`at`, `fromSeed`, `fromHashOf`, consumo do parent e contador do Worldgen wrapper.
Cobre posições com overflow, MD5 e hash Java, strings ASCII, caracteres Unicode,
pares/units isoladas de surrogates, NUL e limites de padding de blocos MD5.
SHA-256: `32e7ea3ca6166f80b7c55ee46eb9f8be5ddc249969cf9a9d6acd7eb251793013`.

`make octave-parity`: **1.551 cenários / 172.139 observações iguais** no JAR.
Verifica construção, aceitação/rejeição, próximo valor do parent, contador dos
wrappers, offsets de octaves, bounds, wrapping e sampling 3/6 argumentos de
Perlin, além de sampling de NormalNoise. Inclui octaves esparsas, amplitudes zero,
negativas, lista vazia, positivos rejeitados pelo modo Legacy e todos os **60
parâmetros vanilla** importados privadamente. Comparação binary64 sem tolerância.
SHA-256: `df1988c3a02bc79aa89822c0f7a601f7c1f71e61731e65657c60d942505fb439`.
Total das cinco suítes: **349.864 resultados coincidentes**.

Testes Python adicionais verificam arquivos truncados, IDs/offsets inválidos,
CRC/offsets de packs e conversão de um PNG próprio com todas as linhas preservadas.
O cliente não foi fornecido: o importador de assets não teve teste end-to-end
contra um cliente legítimo nesta sessão.

Build EE concluído com ps2dev v2.0.0 / GCC 15.2.0. ELF 32-bit little-endian MIPS,
linkado com o startup/linkfile do PS2SDK. Essa evidência é **compilação**, não
execução ou medição de FPS. Tamanhos text/data/bss e linker map estão nos artifacts
e logs do build, sem representar o orçamento de um jogo completo. O código de
núcleo ainda deve ser medido no EE. O ELF executa oito probes de referência;
o resultado esperado é máscara `0xff`.
Sua aprovação verifica apenas esses vetores pequenos, não toda a suíte de host.

## O que os testes ainda não provam

Não provam igualdade de chunks, biomas, geração de estruturas, gaussianas,
física, drops, recipes, IA, redstone,
fluidos ou iluminação. Não provam ordering entre chunks nem serialização de
ticks carregados. O tratamento de buffer cheio retorna erro/backpressure;
o futuro world runtime deve garantir retenção do trabalho e nunca descartar o tick.
Os floats/doubles comparados aqui rodaram no host; binary64 no PS2 necessita da
mesma suíte com output capturado do ELF. Fast-math permanece desativado.

## Contrato para a próxima implementação

Registrar classes/dados, entradas/estado, ordem de chamadas e regras; executar
o original; implementar somente o comportamento observado; comparar o resultado;
registrar limitações e fronteira do teste. Reports/JSON servem como inputs de uma
pipeline privada, e não como arquivos públicos. Dados importados não contam como
subsistema de gameplay implementado.
