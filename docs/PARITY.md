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
  Este kernel amostra valores; derivadas são pendentes. Octaves e operações do
  grafo de densidade são verificadas em suítes distintas abaixo.
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
armazenamento, capacidade, deduplicação, ordem e quinze probes de boot; 17 testes Python da pipeline
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

`make blended-parity`: **160 cenários / 41.520 observações iguais** ao original:
40 octaves construídas na ordem original, consumo do source, contador do wrapper,
bounds, mudança do source via `withNewRandom` e amostragem de densidade nos limites
de coordenadas/escalas. Os parâmetros vanilla são localizados automaticamente nos
dados privados; configurações adicionais exercitam escalas extremas permitidas.
SHA-256: `bfb51330fcc5a0db132b5c595866dc6df835759d464f014a85f1cd323fa604aa`.

`make density-parity`: **1.857 cenários / 110.826 observações iguais** às factories
e ao `compute` original. Cobre constant, y_clamped_gradient, add/mul/min/max,
clamp/range_choice e seis mapas. Compara limites declarados, valores finitos por
bits, sinais de zero, semântica de NaN e hash da sequência de folhas chamadas.
Folhas instrumentadas comprovam atalhos de zero, min/max e seleção exclusiva de
ramos. A especialização com constantes mantém suas diferenças em relação a mul
genérico. Os testes de recursos avaliam uma cadeia de 2.049 níveis sem recursão e
rejeitam workspace insuficiente antes de modificar a saída.
SHA-256: `57bf1b0d01c5787575948d2e3fd9ba5504e7f5911186b1a8184189182132ab20`.
Os grafos desta suíte são sintéticos; não são chunks ou routers vanilla completos.

`make density-data-parity`: **630 cenários / 161.910 observações iguais** a
`RandomState.create`/router do JAR. Todos os 105 campos dos sete noise settings
vanilla são convertidos automaticamente de JSON privado para MCDG, lidos pelo
núcleo nativo e ligados a seis seeds. A referência cria os próprios registries e
noises: não usa os nós convertidos. Isso compara a conversão, o leitor, a ligação
de seeds e os valores/bounds dos campos no domínio de SinglePointContext.
SHA-256: `2a979e39f2f9b6c51720cff2979c89c8cc7b97b086e7da3dc24d74f9d94d9225`.
Inclui splines, weird_scaled_sampler e end_islands. Os packs MCDG v2 armazenam
locations/derivatives em binary32 e referências dos valores em pools privados.
Markers mantêm tipos e comportamento de ponto; wrappers/interpolação de NoiseChunk
e blending entre versões de saves ainda não foram implementados.
`make density-spline-parity`: **416 cenários / 32.073 observações iguais** às
factories de CubicSpline e ao compute/bounds da DensityFunctions.Spline original.
Os grafos sintéticos exercitam extrapolação, um ponto, knots repetidos, derivadas
zero/extremas, conversão binary32, valores aninhados e coordenadas NaN/inf.
O hash de folhas confirma a seleção e ordem de avaliação. O teste de recurso
avalia 1.025 splines aninhadas sem recursão na stack nativa.
SHA-256: `195f0e34f2a537c168dbf7fe7aa1c8c13b6d6d208c7b7affa986c93620b20e80`.

`make simplex-parity`: **70 cenários / 37.542 observações iguais** ao original.
Verifica construção/offsets/parent de SimplexNoise em quatro variantes de PRNG,
sampling 2D com empates/limites de piso e heights/density do End em seis seeds,
incluindo ilha central, ilhas externas, divisões negativas e overflow Java.
Simplex 3D e chunks completos não são cobertos por essa suíte.
SHA-256: `ba25ede9a980e64868fde971dafc59984fec6b214ce46e1250d24b44c5bbccd9`.
Total das dez suítes que invocam Minecraft: **733.735 resultados coincidentes**.

`make float-parity` é uma suíte adicional de **expressões Java 21 próprias**,
sem invocar métodos Minecraft. Compara a camada nativa de aritmética binary32:
operações, min/max, comparações, conversões double/int e sqrt via double, com
subnormais, zeros com sinal, NaN, infinidades e overflow. Os 16 cenários emitem
65.536 registros; payloads de NaN são normalizados somente na observação.
SHA-256: `978ca2607173016ae864b917c10831d049cd63c8fbebb082642dda996ed98ccb`.
O escopo fica em `.local/float-parity/report.json`. Esses registros
não são contabilizados como chamadas a mecânicas Minecraft.

`tools/check_ee_float.py` audita cinco objetos EE e quatro helpers binary64
linkados. Rejeita aritmética, conversão e comparação single-precision na camada
de precisão/spline/End, permitindo apenas movimentos de registradores. A camada
usa intermediários binary64, arredondamento explícito por bits para binary32 e
comparações por bits. A auditoria verifica geração de código; runtime e custo
no console ainda precisam de medição.

Testes Python adicionais verificam arquivos truncados, IDs/offsets inválidos,
CRC/offsets de packs e conversão de um PNG próprio com todas as linhas preservadas.
O cliente não foi fornecido: o importador de assets não teve teste end-to-end
contra um cliente legítimo nesta sessão.

Build EE concluído com ps2dev v2.0.0 / GCC 15.2.0. ELF 32-bit little-endian MIPS,
linkado com o startup/linkfile do PS2SDK. Essa evidência é **compilação**, não
execução ou medição de FPS. Tamanhos text/data/bss e linker map estão nos artifacts
e logs do build, sem representar o orçamento de um jogo completo. O código de
núcleo ainda deve ser medido no EE. O ELF executa quinze probes de referência;
o resultado esperado é máscara `0x7fff`, incluindo compatibilidade binary32.
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
