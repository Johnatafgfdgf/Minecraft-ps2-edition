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
| `DensityFunction` usado como filler | Sampling do campo ligado à seed e avaliação em lote | `NoiseChunkFunction`, `NoiseChunkDensityField`, `NoiseChunkDensityInput` | Fill especializado e inputs explícitos de caches comparados; visitor automático pendente |
| `NoiseChunk.wrap` / visitor | Substitui markers, desempacota holders e compartilha wrappers por igualdade | Futuro visitor do router | Pendente; não confundir fixtures de wrappers com router vanilla completo |
| Aquifer, beardifier, Blender de saves, ore rule | Converte densidade em estados de blocos e mistura versões | Futuros estágios do worldgen | Pendentes; este runtime não produz chunks de blocos |

Os cinco wrappers conservam os limites mínimo/máximo do filler. Os valores
interpolados podem diferir dos valores de ponto do mesmo grafo. MCDG v2 continua
declarando domínio de **SinglePointContext**: carregar um pack e ignorar seus
markers não constitui o pipeline de chunks.

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
O visitor original ainda precisa aplicar markers/holders e compartilhar wrappers
por igualdade ao router vanilla. As fixtures fazem ligações explícitas.

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
