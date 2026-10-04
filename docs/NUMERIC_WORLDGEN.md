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
| `CubicSpline` / `DensityFunctions.Spline` | Coordenada e pontos com derivadas e valores aninhados | Hermite/extrapolação e limites em binary32; seleção de segmento e ordem de filhos preservadas | `density_spline.hpp`, host verificado |
| `SimplexNoise` 2D | PRNG, permutação, coordenadas | Consumo de construção, skew/unskew, gradientes e desempate iguais; offsets não entram no sampling | `simplex_noise.hpp`, host verificado |
| `EndIslandDensityFunction` | World seed, X/Z | 17.292 draws antes do Simplex; vizinhança 25×25; divisões inteiras e overflow Java preservados | `EndIslandDensity`, host verificado |
| `WeirdScaledSampler` | Filho, mapper, NormalNoise | Rarezas e comparações estritas; divide coordenadas e multiplica a magnitude pela rareza | `DensityGraph`, routers vanilla verificados |
| Aritmética `float` do Java | Padrões binary32, casts e operações | Round-to-nearest/ties-to-even explícito, subnormais/NaN/inf/zeros com sinal e comparações por bits | `java_float.hpp`, suíte Java 21 e auditoria de instruções EE |

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
aquifers, carvers, surface rules, features ou estruturas. A ligação de ruídos e
a conversão privada comparam todos os 105 campos dos routers vanilla no domínio
de SinglePointContext. Os wrappers de NoiseChunk ainda devem ser portados antes
das etapas que produzem chunks completos. Os cálculos float/double foram
comparados no host; os novos probes do ELF permitem detectar diferenças no EE,
mas ainda não houve execução desses testes em PS2/PCSX2.

## Precisão de floats no alvo

O primeiro build de spline/End emitia `add.s`, `mul.s`, `div.s`, conversões
inteiras e comparações COP1. A versão atual usa `java_float`: promove operands
binary32 para binary64, calcula cada operação e arredonda novamente por
significand/expoente inteiros, com ties-to-even. Float é um formato de storage e
ABI; as operações que definem regras de geração usam essa camada em ambos os
builds. Comparações/min/max usam padrões de bits, incluindo ±0 e NaN.

Essa camada foi comparada com expressões Java 21 próprias, separadamente das
chamadas a métodos Minecraft. `tools/check_ee_float.py` verifica os objetos de
precisão/spline/End/grafo/leitor e quatro helpers binary64 linkados, rejeitando
instruções single-precision de cálculo/comparação. A auditoria integra o CI.
Ela não mede performance nem comprova runtime no console. O custo desse caminho
precisa ser medido no EE antes de otimizar; qualquer otimização deverá manter
os mesmos resultados, incluindo arredondamento a cada operação binary32.
