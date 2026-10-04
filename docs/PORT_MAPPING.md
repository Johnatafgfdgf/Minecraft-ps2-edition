# Correspondência de port — Java Edition 1.21.1

Nomes abaixo usam os mappings oficiais da Mojang. `MinecraftClient`, `World`,
`PlayerEntity` e `WorldRenderer` do pedido correspondem, respectivamente, aos
nomes Mojang `Minecraft`, `Level`, `Player` e `LevelRenderer`. Os componentes de
cliente ainda precisam de análise de uma instalação legítima do cliente; não se
atribui validação do servidor a renderização/GUI.

Status: **host verificado** = comparação executada contra o JAR oficial;
**compilado PS2** = build EE concluído, execução real pendente;
**analisado** = símbolos/dados localizados, equivalente pendente;
**planejado** = correspondência proposta, sem paridade reivindicada.

| Minecraft 1.21.1 component | Responsabilidade | Dependências | Implementação PS2 correspondente | Status | Diferenças conhecidas |
|---|---|---|---|---|---|
| `net.minecraft.client.Minecraft` | Ciclo do cliente, serviços, telas | janela, input, Level, GUI | `src/platform/ps2/main.cpp` e futuro client runtime | Compilado PS2 somente para plataforma | Não implementa ainda o ciclo do cliente original |
| `MinecraftServer` / `ServerLevel` | Simulação autoritativa | chunks, entidades, ticks | Futuro world runtime | Planejado | Nenhuma simulação de mundo integrada |
| `Level` | Acesso/alteração do mundo | dimensões, chunks, updates | Futuro world runtime | Planejado | Sem mundo jogável |
| `ChunkAccess` / `LevelChunk` | Conteúdo e estado de chunks | sections, altura, luz, entidades | Futuro chunk runtime e arenas | Planejado | Streaming e lifecycle pendentes |
| `LevelChunkSection` | Estados e biomas de uma seção | paletas, light, counters | Futuro section runtime | Planejado | BitStorage é apenas sua infraestrutura |
| `PalettedContainer` / `Palette` | IDs e paletas | IdMap, BitStorage | `BitStorage` e futura palette container | Parcial | Crescimento/remapeamento de paleta pendente |
| `SimpleBitStorage` / `ZeroBitStorage` | Layout de IDs compactados | operações inteiras | `include/mcps2/bit_storage.hpp` | Host verificado para SimpleBitStorage | Buffer externo; erros explícitos; zero storage testado localmente |
| `BlockPos` | Coordenada de bloco compactada | Vec3i | `include/mcps2/position.hpp` | Host verificado para packing | Iteradores/vetores ainda pendentes |
| `SectionPos` / `ChunkPos` | Coordenadas de seção/chunk | BlockPos | `position.hpp` | Host verificado para packing e coord. local | APIs de iteração pendentes |
| `LegacyRandomSource` / `BitRandomSource` | PRNG de 48 bits e primitivas | bits, gaussian source | `LegacyRandom` / `BitRandom` | Host verificado para primitivas/fork/seed | Gaussian pendente; uso único por thread |
| `Xoroshiro128PlusPlus` / `XoroshiroRandomSource` | PRNG de 128 bits | RandomSupport | `XoroshiroRandom` | Host verificado para primitivas/fork/seed | Gaussian pendente |
| `RandomSupport` | Expansão/mistura/hash de seed | Stafford13, MD5 | `mix_stafford13`, inicialização Xoro e `seed_hash.hpp` | Host verificado para expansão e string seeds | Seed única não determinística pendente |
| `PositionalRandomFactory` e implementações | Seeds por posição/string/seed | Mth, RandomSupport, RandomSource | `LegacyPositionalFactory` / `XoroshiroPositionalFactory` | Host verificado: 35.200 observações | UTF-16 representado por view; wrappers delegam conforme o original |
| `WorldgenRandom` | PRNG wrapper e seeds de features | RandomSource | `WorldgenRandom<Source>` | Host verificado para primitivas e seeds | Estado Gaussian/API restante pendente |
| `ScheduledTick` / `TickPriority` | Tempo, prioridade e subordem | BlockPos, tipo canônico | `ScheduledTick`, comparadores | Host verificado na fila por chunk | Tipo é ID canônico; APIs extras pendentes |
| `LevelChunkTicks` | Pendências/deduplicação por chunk | ScheduledTick, pending saves | `ChunkTickQueue` | Host verificado para schedule/poll | Pool finito com backpressure; persistência/unpack pendentes |
| `LevelTicks` | Coleta e ordem entre chunks | chunk ticking, INTRA_TICK_DRAIN_ORDER | Futuro tick coordinator | Planejado | Fila global não é substituída por ordenação simples |
| `TickRateManager` | Ritmo, freeze, sprint, steps | runtime e comandos | `TickClock` + futuro manager | Relógio local implementado | Ainda sem freeze/sprint/step originais |
| `Block` / `Blocks` | Identidade e comportamento | states, shapes, updates | Futuro block registry/runtime | Analisado por reports | Registry de estados não equivale a comportamentos |
| `BlockState` / `StateDefinition` / `StateHolder` | Propriedades e transições | Property, Block | `BlockStates` / MCSR gerado privadamente | Host verificado: 26.684 estados e 33.721 transições | Consulta imutável; transição por busca equivalente; comportamento dos blocos pendente |
| `Item` / `ItemStack` / components | Itens, contagem, dados | registries, components | Futuro item runtime | Analisado por reports | Sem inventário funcional |
| `BuiltInRegistries` / `RegistryAccess` | IDs, nomes e acesso | dados estáticos/dinâmicos | MCSR para blocks/states e mapas privados para 78 registries | Dados exportados; blocks/states nativos | Registries dinâmicos/runtime dos demais ainda pendentes |
| `RecipeManager` / `ShapedRecipe` / `ShapelessRecipe` | Matching e crafting | itens, tags, components | Futuro recipe runtime | Planejado | Não reduzir matching shapeless a contagem ingênua |
| `AbstractFurnaceBlockEntity` | Combustível e processamento | recipes, inventário, ticks | Futuro block entity runtime | Planejado | Sem regras de fornalha integradas |
| `Enchantment` / `EnchantmentHelper` | Efeitos e custos | components, RNG, itens | Futuro enchantment runtime | Planejado | Conteúdo data-driven e efeitos pendentes |
| `Potion` / `MobEffect` | Poções e efeitos | living entities, components | Futuro effects runtime | Planejado | Combate/atributos ainda pendentes |
| `RedStoneWireBlock`, repeaters/comparators | Sinais e propagação | neighbor updates, tick order | Futuro redstone runtime | Planejado | Sem aproximação visual apresentada como paridade |
| pistons / observers | Updates, movimento de blocos | colisão, states, tick order | Futuro block runtime | Planejado | Quasi-connectivity e ordem devem ter fixtures originais |
| `FlowingFluid` / `LiquidBlock` | Simulação de fluidos | shapes, states, scheduled ticks | Futuro fluid runtime | Planejado | Fila de fluidos será separada da fila de blocos |
| `LevelLightEngine` / light engines | Skylight e blocklight | chunks, opacity, emissão | Futuro light runtime | Planejado | Iluminação não é cor fixa do renderer |
| `Entity` / `LivingEntity` | Estado, física e combate | colisão, atributos, effects | Futuro SoA/entity pools | Planejado | Nenhuma física/IA aproximada implementada |
| `Player` / `Inventory` / `AbstractContainerMenu` | Jogador e inventário | entidades, slots, items | Futuro player/menu runtime | Planejado | Input já existe; regras do jogador ainda não |
| `GoalSelector` / `Brain` / navigation | IA/goals/memórias/caminhos | world, RNG, entidades | Futuro behavior/runtime | Planejado | Cada mob precisa de análise e testes próprios |
| `LootTable` / loot functions / predicates | Drops e recompensas | RNG, contexts, registries | Futuro loot runtime | Planejado | JSON importado não será marcado como lógica implementada |
| `NoiseBasedChunkGenerator` / `RandomState` | Geração procedural | noises, biome source, density | Futuro worldgen runtime | Planejado | PRNG verificado não garante os mesmos chunks |
| `ImprovedNoise` | Ruído 3D e quantização vertical | RandomSource, gradientes, Mth | `include/mcps2/improved_noise.hpp` | Host verificado: 32.896 observações binary64 | Amostragem de valor implementada; derivadas e suíte completa no EE pendentes |
| `PerlinNoise` / `NormalNoise` | Octaves e campos de ruído | ImprovedNoise, positional factories, doubles | `octave_noise.hpp` | Host verificado: 172.139 observações; 60 parâmetros vanilla | Buffers externos; derivados/codec/runtime e suíte completa no EE pendentes |
| `BlendedNoise` / density functions | Mistura de densidades e grafo do terreno | noise, contexts, codecs | Futuro numeric/worldgen kernel | Planejado | Exige validação de binary64/StrictMath no EE |
| biome source / carvers / surface rules | Biomas, cavernas, superfície | noise, registries | Futuro worldgen stages | Planejado | Sem gerador alternativo inventado |
| features / structures / placement | Árvores, estruturas, decorators | seeds, worldgen, dados | Futuro worldgen stages | Planejado | Dados estruturais originais permanecem privados |
| `DimensionType` / `LevelStem` | Overworld, Nether, End | registries dinâmicos, geração | Futuro dimension runtime | Planejado | As três dimensões continuam no escopo |
| `LevelRenderer` / chunk mesh | Apresentação do mundo | states, models, texturas, GS | gsKit + futuro mesher/GS renderer | Plataforma compilada PS2 | Ainda sem passes de mundo, entidades, céu ou partículas |
| GUI / HUD / screens | Menus e apresentação | fontes, items, jogador | Futuro GS GUI runtime | Diagnóstico de plataforma somente | Não reivindica HUD/menu originais |
| resource manager / sound engine | Recursos e áudio | assets, SPU2 | `import_minecraft_assets.py`, MCPK/MCPT; futuro SPU2 | Pipeline escrita; PNG verificado com fixture própria | Import completo requer cliente legítimo ainda não fornecido; renderer/audio pendentes |
| chunk serialization / region files / NBT | Saves | world state, registries, filesystem | Ponte POSIX + futuro save codec | Plataforma compilada PS2 | Escrever diagnóstico não equivale a salvar mundo |
| `Commands` / Brigadier / `GameRules` | Comandos e regras | runtime e registries | Futuro command runtime | Analisado: command syntax report | Comandos relevantes/regra de ticks pendentes |
| conexão / protocolo | Networking | packets, sockets, IOP | Futuro PS2 network runtime | Planejado | Sem alegação de interoperabilidade Java |

Todos os sistemas planejados permanecem no escopo. Limites de RAM/CPU geram
tarefas de representação, streaming e cálculo equivalente, não autorização para
remover regras. Uma impossibilidade observada deve ser documentada com orçamento
e medições, nunca mascarada como paridade.
