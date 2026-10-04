# Continuidade do port

Esta lista organiza trabalho pendente; não reduz o escopo da Java 1.21.1.

Etapas concluídas no host: PRNGs/seeds/coordenadas/storage/fila por chunk;
representação de todos os block states com transições; amostragem de valores
ImprovedNoise exata nos casos comparativos. A plataforma compila para EE e ainda
precisa de boot em equipamento. Dados exportados não substituem gameplay.

1. Integrar o registro de states ao runtime e carregar os demais registries; as
   propriedades, IDs, defaults e transições já foram conferidos. Implementar
   callbacks e mecânicas de stairs, doors, furnace, crops e demais blocos após
   análise própria. Validar a pipeline de assets com um cliente legítimo fornecido.
2. Completar paletas, seções, cache/streaming, arenas e save codec. Testar eviction
   sem perda de estado, scheduled ticks, fluid ticks e block entities.
3. Completar random factories, string seed hashing, gaussian e núcleo numérico
   exato. Portar noises/density functions; comparar seeds/coordenadas antes de
   integrar biome source, carvers, surface rules, features e estruturas.
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
