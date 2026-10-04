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
| `PerlinNoise` | firstOctave, amplitudes, octaves | Modo moderno deriva cada octave de `octave_N`; modo Legacy cria octave zero primeiro e consome 262 ints por octave ausente; ordem e wrapping são relevantes | Próxima implementação |
| `NormalNoise` | Dois PerlinNoise e amplitudes | Construção sequencial no mesmo source; segunda entrada multiplicada por 1.0181268882175227; fator depende do intervalo entre amplitudes não nulas | Próxima implementação |

A transformação MD5 é uma implementação própria do algoritmo matemático descrito
no [RFC 1321](https://www.rfc-editor.org/rfc/rfc1321), com buffer de 64 bytes. O
texto Java é representado por uma view UTF-16: pares de surrogates viram um code
point; unidades isoladas usam a substituição `?` da codificação UTF-8 do Java.
Não há alocação de heap nas factories ou no hash; strings ASCII de nomes de
octaves usam um caminho direto.

A paridade das factories passou em 3.072 cenários / 35.200 observações: texto,
overflow, consumo do parent e dos wrappers conferidos no JAR original.
Esses componentes não produzem ainda chunks, biomas ou estruturas completos.
