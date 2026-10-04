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
Cada nó ocupa no máximo 64 bytes; cada frame de avaliação, 16 bytes. A arena e as
folhas externas devem permanecer válidas durante o uso do grafo. Inputs externos
declaram seus limites e precisam respeitá-los para manter os atalhos equivalentes.
