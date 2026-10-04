# Núcleo numérico da geração — análise 1.21.1

Esta etapa usa os mappings e o bytecode local do servidor legítimo para localizar
chamadas e operações; a validação invoca o próprio JAR. Não publica o material
original. O núcleo será integrado ao gerador de chunks após comparar cada camada.

| Componente original | Entradas e estado | Regras identificadas | Equivalente |
|---|---|---|---|
| `Mth.getSeed(int,int,int)` | Coordenadas inteiras | Produto de X em 32 bits antes da promoção; operações posteriores em 64 bits; shift aritmético de 16 bits | `positional_seed` |
| `LegacyPositionalRandomFactory` | Seed de 64 bits | `at` mistura posição; texto usa hash Java dos code units UTF-16; `fromSeed` cria um PRNG somente com a seed recebida | `LegacyPositionalFactory` |
| `XoroshiroPositionalRandomFactory` | Dois longs | `at` mistura somente a metade baixa; `fromSeed` faz XOR nas duas metades; texto usa MD5 de UTF-8 e duas metades big-endian | `XoroshiroPositionalFactory` |
| `RandomSource.forkPositional` | Estado atual do PRNG | Legacy consome um long; Xoroshiro consome dois; wrapper Worldgen delega ao source sem incrementar seu contador de bits | `fork_positional` |
| `PerlinNoise` | firstOctave, amplitudes, octaves | Modo moderno deriva cada octave de `octave_N`; modo Legacy cria octave zero primeiro e consome 262 ints por octave ausente; ordem e wrapping são relevantes | `octave_noise.hpp`, host verificado |
| `NormalNoise` | Dois PerlinNoise e amplitudes | Construção sequencial no mesmo source; segunda entrada multiplicada por 1.0181268882175227; fator depende do intervalo entre amplitudes não nulas | `octave_noise.hpp`, host verificado |
| `BlendedNoise` | Source, cinco parâmetros, coordenadas inteiras | 16 octaves para cada limite e 8 para o selector; wrapping, quantização Y e clamped lerp na ordem original | `blended_noise.hpp`, host verificado |
| `DensityFunctions` aritméticas | IDs de filhos, parâmetros e limites | Limites e especialização de constantes equivalentes; avaliação ordenada e preguiçosa, sem recursão | `density_graph.hpp`, host verificado |
| `RandomState` noise wiring | World seed, modo Legacy, recursos | Factory por nome; casos específicos de clima/offset/terrain; instâncias compartilháveis | `worldgen_noise.hpp`, routers no host verificados |

A transformação MD5 é uma implementação própria do algoritmo matemático descrito
no [RFC 1321](https://www.rfc-editor.org/rfc/rfc1321), com buffer de 64 bytes. O
texto Java é representado por uma view UTF-16: pares de surrogates viram um code
point; unidades isoladas usam a substituição `?` da codificação UTF-8 do Java.
Não há alocação de heap nas factories ou no hash; strings ASCII de nomes de
octaves usam um caminho direto.

A paridade das factories passou em 3.072 cenários / 35.200 observações: texto,
overflow, consumo do parent e dos wrappers conferidos no JAR original.
Esses componentes não produzem ainda chunks, biomas ou estruturas completos.

Perlin/Normal usam arrays de `NoiseOctave` fornecidos pelo caller. Cada entrada
ocupa `sizeof(NoiseOctave)`, medido no compilador alvo; a view não possui o buffer.
Não há limite arbitrário fixo de octaves: a capacidade externa retorna erro antes
de consumir o PRNG se o pool for insuficiente. O caminho Legacy mantém o consumo
original também ao rejeitar parâmetros de octaves positivos. Parâmetros de dados
originais são lidos do JAR verificado e permanecem em fixtures privadas.

BlendedNoise recebe um pool de 40 entradas e referencia três regiões desse pool.
Constrói os dois campos de limites antes do selector; reseeding preserva os cinco
parâmetros originais. O selector decide quando o campo inferior/superior pode ser
ignorado sem mudar o resultado. A saída ainda é uma função de densidade: não inclui
aquifers, carvers, surface rules, features, estruturas ou o grafo completo de density
functions. A ligação de ruídos e a conversão privada já comparam 93 campos de
routers vanilla; faltam splines, ilhas do End e os wrappers de NoiseChunk antes
das etapas que produzem chunks completos.
