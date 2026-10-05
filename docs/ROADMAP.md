# Continuidade do port

Esta lista organiza trabalho pendente; não reduz o escopo da Java 1.21.1.

Etapas concluídas no host: PRNGs/seeds/coordenadas/storage/fila por chunk;
representação de todos os block states com transições; factories de posição e
strings; ImprovedNoise/PerlinNoise/NormalNoise/BlendedNoise exatos nos casos comparativos.
Aritmética, gradientes, limites e seleção de ramos do grafo de densidade conferidos
em grafos sintéticos; todos os 105 campos de routers vanilla, splines e ilhas do
End conferidos no domínio de pontos. A camada binary32 usa arredondamento e
comparações explícitos por bits; o build EE audita seu caminho numérico.
Os cinco wrappers de NoiseChunk, o ciclo de células e fillArray especializado
dos grafos foram conferidos em fixtures originais. Inputs explícitos ligam
operadores aos caches. O visitor automático por campo, com compartilhamento por
igualdade/identidade, foi comparado aos 105 campos vanilla em seis seeds. A
ligação compartilhada de todos os campos e geração de chunks completos estão pendentes.
A plataforma compila para EE e ainda
precisa de boot em equipamento. Dados exportados não substituem gameplay.

1. Integrar o registro de states ao runtime e carregar os demais registries; as
   propriedades, IDs, defaults e transições já foram conferidos. Implementar
   callbacks e mecânicas de stairs, doors, furnace, crops e demais blocos após
   análise própria. Validar a pipeline de assets com um cliente legítimo fornecido.
2. Completar paletas, seções, cache/streaming, arenas e save codec. Testar eviction
   sem perda de estado, scheduled ticks, fluid ticks e block entities.
3. Integrar os 15 campos do router em uma ligação compartilhada, preservando
   identidades entre campos; o visitor por campo já foi conferido. Portar aquifers.
   Completar gaussian, demais noises e núcleo numérico
   necessário; integrar biome source, carvers, surface rules, features e
   estruturas, comparando chunks com o original em cada fase.
4. Integrar world runtime, coordenador global de ticks, block updates e random
   ticks. Usar fixtures observadas no original para redstone (incluindo ordem e
   quasi-connectivity), fluidos, iluminação e alterações de estado.
5. Portar item components, tags, crafting, fornalhas, inventário, loot,
   encantamentos, poções, combate e progressão; evitar implementações incompletas
   marcadas como equivalentes.
6. Portar física, entidades, spawn, goals/brain/pathfinding por entidade, com
   testes de atributos, movimento, ações, drops e AI. Integrar Overworld/Nether/End,
   game rules e comandos relevantes.
7. Desenvolver meshing/atlases, passes opaque/cutout/translucent, entities,
   particles/block entities, céu/fog, HUD/menus e SPU2; transferir trabalho ao
   VU/DMA quando dados e medições justificarem. A apresentação não muda regras.
8. Rodar a suíte no PS2/PCSX2; capturar comportamento, RSS/arenas, VRAM, DMA e
   tempos por fase. A meta inicial é 30 FPS, e ainda não há evidência de atingi-la.

Gates de conclusão: paridade dos sistemas listados, budgets medidos, saves
compatíveis com as regras documentadas, pipeline pessoal reproduzível e teste em
hardware real. Menu/um chunk/andar entre cubos não satisfaz esses gates.
