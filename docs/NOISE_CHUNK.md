# Células, interpolação e caches da 1.21.1

Referência: servidor oficial 1.21.1, SHA-1
`59353fb40c36d304f2035d51e7d6e6baa98dc05c`. Classes, chamadas e campos foram
localizados nos mappings e analisados no bytecode local com `javap`. Esse
material permanece privado em `.local/`; o código deste runtime é independente.

## Correspondências

| Componente original | Responsabilidade / dependências | Equivalente nativo | Estado / diferença |
| --- | --- | --- | --- |
| `NoiseChunk` — geometria e contextos | Origem de células, quart positions, coordenadas e contadores | `NoiseChunk`, `NoiseChunkContext`, `NoiseChunkProvider` | Comparado no host; arena externa e erros explícitos |
| `NoiseInterpolator` | Dois slices X, oito vértices e valores intermediários | `NoiseCacheKind::interpolated` | Ambas as ordens de interpolação comparadas |
| `FlatCache` | Grid X/Z em quart coordinates, calculado em Y=0 | `NoiseCacheKind::flat` | Preenchimento inicial ligado/desligado, limites e fallback comparados |
| `Cache2D` | Uma posição X/Z e seu último valor | `NoiseCacheKind::column` | Chave, sentinel e bypass no fillArray comparados |
| `CacheOnce` | Última amostra e último array por contador do proprietário | `NoiseCacheKind::once` | Identidade do contexto, prioridade do array, cópia e contadores comparados |
| `CacheAllInCell` | Valores de uma célula, indexados em Y descendente, X, Z | `NoiseCacheKind::cell` | Preenchimento, estado inativo e fallback comparados |
| `DensityFunction` usado como filler | Sampling do campo ligado à seed e avaliação em lote | `NoiseChunkFunction`, `NoiseChunkDensityField`, `NoiseChunkDensityInput` | Fill especializado, inputs de caches e ligação automática por campo comparados |
| `NoiseChunk.wrap` / visitor | Substitui markers, desempacota holders e compartilha wrappers por igualdade | `NoiseChunkGraph`, arenas de planning e execução | Comparado em 272 grafos próprios e 105 campos vanilla × 6 seeds; binding compartilhado dos 15 campos de um router ainda pendente |
| Aquifer, beardifier, Blender de saves, ore rule | Converte densidade em estados de blocos e mistura versões | Futuros estágios do worldgen | Pendentes; este runtime não produz chunks de blocos |

Os cinco wrappers conservam os limites mínimo/máximo do filler. Os valores
interpolados podem diferir dos valores de ponto do mesmo grafo. Um grafo MCDG v3
pode ser avaliado em **SinglePointContext** ou ligado explicitamente ao
`NoiseChunkGraph`. Somente a segunda opção substitui seus markers por caches.
Packs v2 continuam legíveis para pontos, mas são recusados pelo runner de ligação:
não conservam o tipo de uma spline constante sem ambiguidade.

## Ciclo analisado

1. A geometria deriva cell counts, origem com floor division e quart positions.
   Largura/altura de célula vêm dos noise settings; não de um tamanho inventado.
2. `initializeForFirstCellX` ativa o ciclo, zera somente o contador de amostras
   e preenche o primeiro slice. Inicializar duas vezes é erro.
3. `advanceCellX` preenche o próximo slice e reposiciona a origem X na célula
   corrente. Cada coluna Z tem uma época de arrays compartilhada pelos fillers.
4. `selectCellYZ` escolhe oito vértices, ativa fillingCell e preenche os caches
   da célula em sua ordem de registro. Há incrementos do contador de arrays
   antes e depois desses fills. O contador de amostras não avança nessa fase.
5. A passagem pelos blocos atualiza Y, X e Z, nessa ordem. Atualizar Z incrementa
   o contador de amostras antes de atualizar os interpoladores.
6. `swapSlices` troca os ponteiros dos slices; os valores não são copiados.
   `stopInterpolation` encerra o ciclo; chamar stop duas vezes é erro.

O slice provider visita Y crescente, avançando o contador de amostras por
índice. O cell provider visita Y decrescente, depois X e Z, sem avançar esse
contador. `forIndex(i)` conserva i como arrayIndex. O fill direto da célula
incrementa arrayIndex **antes** de chamar compute, por causa da avaliação de
`array[index++]` em Java. Essa diferença também é testada.

## Regras que afetam o resultado

- A passagem por blocos interpola Y, depois X, depois Z. Durante fillingCell,
  o caminho de `Mth.lerp3` interpola X, depois Y, depois Z. Reordenar essas
  operações pode mudar bits; não há FMA ou fast-math no build.
- Interpolador, cache once e cache de célula reconhecem o contexto pela identidade
  do proprietário. SinglePointContext chama o filler. FlatCache e Cache2D
  conservam seus comportamentos X/Z também com contextos externos.
- Cache2D ignora Y ao reconhecer sua última posição. A chave inicial corresponde
  a `(1875066,1875066)`; uma primeira consulta dessa chave retorna o valor inicial
  zero. O fillArray delega ao filler, não repete o cache por coluna.
- CacheOnce consulta primeiro o array com época correspondente, depois o valor
  com contador correspondente. Ambos os contadores começam em zero, assim como
  o valor; uma amostra inicial do proprietário pode retornar zero sem avaliar
  o filler. FillArray mantém uma cópia própria e não muda o contador da amostra.
- CacheAllInCell usa `((height-1-y)*width+x)*width+z`. Coordenadas fora da célula
  chamam o filler; contexto proprietário sem interpolação ativa gera erro.
- FlatCache amostra os quart grid points em Y=0 durante a construção. Dentro do
  grid, também ignora Y; fora dele, chama o filler com o contexto recebido.

## Representação e memória no PS2

O chamador fornece a arena de wrappers e os buffers. As funções usam callbacks
e IDs de grafos, sem depender de objetos Java. Construção/fill/sample não usam
heap, e buffers insuficientes geram erro; não desativam uma regra. Cada wrapper
ocupa **192 bytes no build EE fixado**; o controlador ocupa 168 bytes. A arena,
os buffers, fillers, limites e frames do grafo precisam conservar endereços estáveis.
Chunk não é copiável. Callbacks não devem criar ciclos nem reentrar no mesmo
workspace. Os buffers de wrappers distintos não devem se sobrepor.

| Buffer | Doubles necessários | Exemplo: width=4, height=8, 4 células X/Z, 384 blocos Y |
| --- | --- | --- |
| Dois slices de um interpolador | `2*(cellCountXZ+1)*(cellCountY+1)` | 490 / 3.920 bytes |
| Um FlatCache | `(quartSizeXZ+1)^2` | 25 / 200 bytes |
| Um CacheAllInCell | `width*width*height` | 128 / 1.024 bytes |
| Um CacheOnce | Capacidade do maior array usado pelo caller | Padrão recomendado: 128 / 1.024 bytes |
| Um Cache2D | Nenhum buffer de doubles externo | Chave/valor na arena |

`required_values(once)` recomenda o maior entre a célula e a coluna vertical.
Um caller que use arrays maiores deve reservar mais espaço explicitamente.
Buffers conservam binary64; compactar os valores mudaria a especificação.
Pool exhaustion exige backpressure/replanejamento de memória, não sampling com
uma regra diferente. Geometrias que estouram tamanhos do alvo são rejeitadas.

Os limites compartilham uma view estável de dois doubles. Transportar esses
doubles dentro da struct de callbacks por valor provocou um erro interno no
register reload do GCC 15.2.0 EE; a representação por referência compila no mesmo
toolchain e conserva os limites comparados ao original.

O callback `fill` é distinto de `sample`. A ponte `NoiseChunkDensityField`
executa os caminhos especializados de [fillArray](DENSITY_GRAPH.md), com frames
de lote e scratch externos. `NoiseChunkDensityInput` liga wrappers ao grafo em
sentido inverso, preservando identidade, valores em lote e erros. Inputs no EE
ocupam 32 bytes, e a ponte de input ocupa 16. Workspaces de fillers aninhados
precisam ser separados; a ponte de campo inclui apenas um frame para um grafo
folha. Grafos maiores devem fornecer os frames necessários explicitamente.
O `NoiseChunkGraph` faz essa ligação automaticamente para um campo. As fixtures
anteriores com wrappers explícitos continuam como testes independentes.


## Visitor automático — análise e implementação

O caminho original é `DensityFunction.mapAll(visitor)` → transformação dos
filhos → `NoiseChunk.wrap` → consulta do mapa de funções → `wrapNew` quando a
chave não existe. O oracle chama esse caminho no JAR, sem reproduzi-lo em Java.

| Componente / decisão original | Equivalente nativo | Estado e diferença |
| --- | --- | --- |
| MarkerOrMarked transforma o filho antes de construir um novo Marker | DFS em arena, na ordem dos filhos e pontos das splines | Sem recursão no traversal do grafo |
| wrapNew substitui os cinco tipos de Marker | Plano `NoiseGraphCache`, seguido por `NoiseChunk.wrap` | FlatCache é avaliado somente na ativação, na ordem de construção original |
| HolderHolder transforma seu valor e é desempacotado por wrapNew | Reference vira o ID do filho transformado | Não permanece como nó de encaminhamento no grafo ligado |
| Ap2 transforma filhos e reaplica a factory | `DensityGraph::append` após transformar os IDs | Um holder que revela uma Constant pode mudar a especialização |
| Mapped e MulOrAdd transformam filhos sem apply ao próprio nó | Recalcular bounds; `append_transformed` conserva o argumento de MulOrAdd | Não recriar MulOrAdd pela factory binária nem trocar operandos constantes |
| Igualdade de records inclui tipos, propriedades e filhos transformados | IDs canônicos e tabela hash em arena | +0 e -0 distintos; NaN comparado como Double Java |
| BlendedNoise e EndIslandDensityFunction usam identidade de objeto | Origem pelo ID do nó fonte | Valores/seeds iguais não autorizam compartilhar wrappers de objetos distintos |
| CubicSpline.Multipoint retém as arrays de locations/derivatives no mapAll | Identidade pelo descritor de spline fonte | Descritores com conteúdo igual e origens diferentes permanecem distintos |
| Spline de CubicSpline.Constant não é DensityFunctions.Constant | Opcode `spline_constant` do MCDG v3 | Bounds/value binary32 e fill direto; não usar o atalho da factory de Constant |

A ligação atual cobre **EmptyBlender e Beardifier vazio**. BlendAlpha,
BlendOffset e BeardifierMarker conservam o comportamento dessa referência.
As substituições por caches de blending de saves e pelo beardifier de estruturas
precisam de implementação própria antes de integrar essas condições.

O chamador constrói um controlador estável e executa:

1. `prepare(root, largest_array)`: percorre somente os nós alcançáveis, copia
   descritores de spline com IDs transformados, compartilha chaves iguais e
   calcula os tamanhos. Não executa fillers nem modifica o chunk.
2. `requirements()`: entrega quantidades de wrappers, doubles, frames de
   ponto/lote e temporários. A soma dos tamanhos e seus bytes é verificada para
   overflow de `size_t`, inclusive no EE de 32 bits.
3. Reservar todos os buffers e chamar `activate(workspace)`. Falta de memória
   ou de slots do chunk é recusada antes de avaliar FlatCache; é possível
   tentar novamente com buffers suficientes. Falha de callback durante a
   construção exige descartar o controlador/chunk parcialmente ativado.
4. Usar `function()` como campo para sample/fill, seguindo o ciclo do chunk.

Cada filler de cache recebe pilhas e scratch próprios para permitir chamadas
aninhadas. A função raiz recebe outro workspace. Os buffers não se sobrepõem e
seus endereços ficam estáveis; o controlador não é copiável. O grafo fonte e seus
recursos também devem conservar os tipos/origens: reutilizar um ID representa a
mesma função original, e reutilizar um descritor representa as mesmas arrays.
Inputs externos usam seu índice como identidade. Recursos de ruído são
canônicos dentro do grafo fonte ligado a uma seed.

No EE fixado: chave **64 B**, frame de planning **8 B**, plano de cache **88 B**,
controlador **224 B**, descritor de arena **72 B**. Esses valores não incluem os
nós, inputs, pontos, buffers e frames, nem os pools de ruído. A tabela hash usa
slots em potência de dois, no mínimo 2×N; as demais arenas de planning usam
capacidades documentadas no header. Não há alocação por ponto ou por fill.
São budgets estruturais; o custo e o orçamento de um mundo jogável ainda
precisam ser medidos no console.

## Comparação

`make noise-chunk-parity` usa 66 cenários próprios. O oracle constrói um
NoiseChunk **original** com RandomState/registries originais, remove apenas os
wrappers do router das listas de execução da fixture e registra os cinco tipos
originais sobre folhas instrumentadas. Não há implementação Java alternativa
dos caches. As folhas usam ImprovedNoise original com seeds explícitas,
YClampedGradient original ou padrões numéricos de teste.
Trinta cenários adicionais criam operadores originais add/mul/min/max/range
sobre wrappers, preenchendo caches de células e amostrando os grafos resultantes.
Comparam também arrays maiores que a célula, incluindo o tail inicialmente +0
do temporário de add. O grafo nativo usa as pontes nos dois sentidos.

A suíte compara valores binary64, limites, estado, contadores, erros e hash da
sequência de chamadas dos fillers. Cobre tamanhos de célula 4/8/16, altura 384,
coordenadas negativas/extremas, slices trocados, reinício, preenchimento direto
ou por índice, arrays compartilhados/copiados, bordas e estados inativos.
NaN é normalizado somente no registro de observação. Os relatórios e resultados
completos ficam privados em `.local/noise-chunk-parity/`.

Os testes de recursos verificam falta de arena/buffer antes de executar fillers,
canários, overflow de geometria, workspace e ponte com DensityGraph. Um probe
pequeno dessa ponte foi incluído no ELF; a suíte completa e seu custo ainda
precisam ser medidos no PS2 real. Build EE não comprova boot ou 30 FPS.

`make density-chunk-parity`: **272 cenários / 151.776 registros iguais**.
Inclui holders que mudam factories, MulOrAdd com dois operandos constantes,
Spline constante versus Constant, records iguais, zeros com sinal/NaN,
identidade de objetos End com a mesma seed, origem das arrays de splines,
markers não alcançáveis, composição dos cinco caches e 128 grafos aleatórios.
Compara contagens dos caches construídos, limites, valores, épocas e trace de
folhas próprias. Não compara o número de entradas internas do HashMap original.

`make density-data-chunk-parity`: **630 cenários / 351.540 registros iguais**.
Aplica o visitor original aos 105 campos dos sete noise settings, com seis
seeds; o oracle cria seus próprios RandomState/registries e não lê o MCDG.
O nativo lê o pack v3, prepara e ativa seus wrappers automaticamente. O ciclo
inclui dois níveis Y de célula, coordenadas negativas/distantes/extremas,
contextos externos, repetição, arrays de célula e célula+1, troca de slices,
parada e reinício. Os arrays fornecidos aos providers respeitam sua extensão;
guardas de buffers menores são verificadas pelos testes nativos.

Cada campo é ligado a um chunk de teste independente, com Blender/beardifier
vazios e geometria pequena. Compartilhar os 15 campos em uma ligação única,
aquifer, materiais, carvers, superfície, features e estruturas permanecem
etapas posteriores. Não são testes de chunks finais de blocos. Reports e
entradas completas ficam privados em `.local/density-chunk-parity/` e
`.local/density-data-chunk-parity/`; o CI publica somente os reports próprios.
