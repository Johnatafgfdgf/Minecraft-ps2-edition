# Grafo de densidade da 1.21.1

Referência: bundle público oficial do servidor **1.21.1**, SHA-1
`59353fb40c36d304f2035d51e7d6e6baa98dc05c`. Os mappings, o bytecode inspecionado
com `javap` e os dados originais permanecem em `.local/`. Este documento descreve
comportamento; não contém código da Mojang.

## Operações analisadas

| Componente original | Estado / entradas | Resultado e dependências |
| --- | --- | --- |
| `DensityFunction` | Contexto de coordenadas inteiras; limites mínimo/máximo | `compute` de um ponto; transformações criam novos nós |
| `Constant` | Um `double` | O mesmo valor e os mesmos limites, incluindo o sinal de zero |
| `YClampedGradient` | Duas alturas e dois valores | Fração `(y-fromY)/(toY-fromY)`, depois interpolação limitada; alturas são promovidas antes da subtração |
| `TwoArgumentSimpleFunction.create` | Operação e limites dos dois filhos | Calcula limites antes de especializar add/mul com um filho constante |
| `Ap2` | Dois filhos em ordem | Add avalia ambos; mul retorna **+0** se o primeiro valor compara igual a zero; min/max podem omitir o segundo filho usando seus limites |
| `MulOrAdd` | Filho e constante | Avalia o filho antes de multiplicar/somar a constante; não usa o atalho de zero de `Ap2` |
| `Mapped` | Filho e tipo de transformação | abs, square, cube, half_negative, quarter_negative e squeeze; também transforma os limites |
| `Clamp` | Filho, min/max explícitos | Limites declarados são os parâmetros; transformação segue `Mth.clamp` |
| `RangeChoice` | Entrada, intervalo, dois ramos | Testa `valor >= minInclusive && valor < maxExclusive`; avalia somente o ramo escolhido |

O caminho estudado é `DensityFunction.compute(FunctionContext)` e as factories
usadas para construir seu grafo. `fillArray`, o visitor de ligação de ruídos e os
wrappers de `NoiseChunk` precisam ser analisados separadamente antes de serem
usados na geração de chunks.

## Regras numéricas e de avaliação

- Os nós usam binary64, sem fast-math nem fusão de operações. Cube calcula
  `(v*v)*v`; squeeze primeiro limita a `[-1,1]`, depois calcula
  `v/2 - ((v*v)*v)/24` nessa ordem.
- Half/quarter negative testam **`v > 0`**. O outro ramo multiplica por 0.5/0.25,
  inclusive para zero e valores não ordenados.
- `Math.min/max` preservam NaN e distinguem +0 de -0. `Mth.clamp(v,a,b)` retorna
  `a` quando `v<a`; caso contrário retorna `Math.min(v,b)`.
- O gradiente usa extremos estritos para a fração: `<0` e `>1`. Nos extremos
  exatos 0/1 ainda realiza a interpolação. Isso pode afetar o sinal de zero.
- Min omite o segundo filho somente quando `primeiro < segundo.minValue()`;
  max usa `primeiro > segundo.maxValue()`. Igualdade exige avaliar ambos.
- Os limites de mul dependem dos sinais dos dois intervalos. Não substituir
  por outra fórmula só porque ela produz limites matematicamente mais apertados.
- Para abs/square, `Mapped.create` declara o mínimo como
  `max(0, mínimo original)` e o máximo como o maior dos extremos transformados.
  Os outros mapas mantêm a ordem dos extremos transformados. Esses limites são
  parte do comportamento usado pelos atalhos.
- Os limites de range_choice dependem dos dois ramos, independentemente do
  intervalo da entrada. Clamp declara seus limites, sem intersectá-los com os
  limites do filho.

## Implementação nativa

Uma arena de nós imutáveis usa IDs em ordem de dependência. O construtor calcula
os limites equivalentes e rejeita referências futuras, ciclos e falta de
capacidade. A avaliação usa uma pilha fornecida pelo chamador, com profundidade
calculada antes da amostragem. Não há alocação por ponto nem recursão na pilha do
Emotion Engine. Falta de memória retorna um erro explícito; não troca o grafo
por um terreno simplificado.

Os testes constroem os mesmos grafos por chamadas às factories originais e à
implementação nativa. Comparam valores, limites e ordem/quantidade de avaliações
de folhas instrumentadas, incluindo os ramos que não devem ser chamados.
Resultados finitos e zeros são comparados por bits. NaN é normalizado apenas no
protocolo de observação, pois seu payload não define uma regra de geração.

Isso estabelece operações do grafo. Ainda não estabelece paridade de chunks,
biomas, splines, aquifers, carvers, superfície, features ou estruturas.

`make density-parity` verifica 1.857 grafos e 110.826 observações coincidentes.
Cada nó ocupa no máximo 64 bytes; cada frame de avaliação, 24 bytes. A arena e as
folhas externas devem permanecer válidas durante o uso do grafo. Inputs externos
declaram seus limites e precisam respeitá-los para manter os atalhos equivalentes.

## Ligação de ruídos e dados — análise

`RandomState` cria uma factory posicional a partir da seed e do algoritmo dos
noise settings. `Noises.instantiate` usa o nome completo do recurso na factory,
então cria NormalNoise moderno com os parâmetros do registry. O visitor mantém
instâncias compartilhadas por recurso. BlendedNoise usa `minecraft:terrain` no
modo moderno e um LegacyRandomSource com a seed do mundo no modo Legacy.

No modo Legacy, o visitor substitui temperature/vegetation por NormalNoise
Legacy com firstOctave -7 e duas amplitudes unitárias, usando seed/seed+1. Para
shift, usa NormalNoise moderno, firstOctave 0 e uma amplitude zero, ainda com
a factory do nome `minecraft:offset`. Isso é uma regra do código analisado,
independente dos parâmetros externos de noise JSON.

| Nó | Coordenadas usadas no NormalNoise | Limites |
| --- | --- | --- |
| noise | `(x*xzScale, y*yScale, z*xzScale)` | `±noise.maxValue()` |
| shift | `(x/4,y/4,z/4)`, saída multiplicada por 4 | `±(noise.maxValue()*4)` |
| shift_a | `(x/4,0,z/4)`, saída multiplicada por 4 | Idem |
| shift_b | `(z/4,x/4,0)`, saída multiplicada por 4 | Idem |
| shifted_noise | Escala de cada eixo, depois soma do filho correspondente, em ordem X/Y/Z | `±noise.maxValue()` |

As referências de registry precisam conservar um nó HolderHolder: seu valor e
limites delegam ao filho, mas ele não é uma Constant para a especialização de
add/mul. Os markers interpolated/flat_cache/cache_2d/cache_once/cache_all_in_cell
também conservam seus tipos. Seu `compute` de ponto ainda delega ao filho;
`NoiseChunk` troca esses markers por wrappers com outra semântica e isso permanece
uma etapa própria. `BlendDensity` declara limites infinitos e, no contexto
SinglePointContext com Blender vazio, devolve o valor recebido. O sampler de
pontos não pode ser tomado como o interpolador de chunks nem blending de saves.

A conversão lê somente o JAR verificado, resolve referências, retém propriedades
e parâmetros e escreve um formato próprio em `.local/`. Um tipo não implementado
deve impedir a compilação daquele grafo, com diagnóstico da dependência que
faltou. Não gerar uma substituição zero para uma função desconhecida.

## Pipeline implementada

`tools/compile_density_graph.py --inventory` gera **MCDG v1** little-endian com
header de 40 bytes, nós de 40 bytes no disco, descritores de ruídos de 64 bytes,
nomes e amplitudes em pools. CRC-32 cobre o payload. O leitor `DensityPack`
valida versão/domínio, CRC, tamanho, offsets, referências anteriores, tipos de
recursos e números antes de expor views. O runtime recebe pools separados para
os objetos de ruído, octaves, nós e frames; não precisa de JSON no EE.

Foram convertidos **93 dos 105 campos** dos sete noise settings vanilla. Somam
411 nós e 28.144 bytes de packs; isso não inclui os pools de ruído construídos
por seed. O inventário nomeia os 12 campos pendentes: depth, initial density e
final density dos três Overworld settings dependem de spline; três campos do
End dependem de end_islands. O compilador não emite grafos incompletos para eles.

`make density-data-parity` lê esses packs pelo leitor nativo e compara **558
cenários / 143.406 observações** com `RandomState.create` e `router` no próprio
JAR, usando seis seeds. A referência não lê o MCDG nem reconstrói o resultado a
partir do conversor; usa seus próprios registries e factories. Cobre os campos
compilados, inclusive densidade final de Nether/caves/floating_islands, ainda
no domínio de pontos anterior aos wrappers de NoiseChunk. Não comprova o layout
dos blocos de um chunk nem o comportamento das etapas posteriores da geração.
