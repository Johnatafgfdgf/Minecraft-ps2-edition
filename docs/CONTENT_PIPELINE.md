# Conteúdo privado e block states

## Geração pelo original

Após importar o servidor legítimo e configurar JDK 21:

```sh
make reference-data
make registry-parity
```

`generate_reference_data.py` executa `net.minecraft.data.Main --reports` do JAR
original. O compilador processa os reports gerados, sem milhares de constantes
manuais. Um formato inválido/ID faltando aborta o processo, sem substituir blocos.
São criados apenas abaixo de `.local/`:

| Arquivo | Uso |
|---|---|
| `content/block_states.bin` | Registry MCSR consumido pelo C++ |
| `content/registry_ids.json` | Correspondência Java ID → PS2 ID dos 78 registries do report |
| `content/world_data.pack` | Dados do JAR original, incluindo recipes, loot, worldgen e estruturas |
| `content/state_cases.txt` | Inputs para comparar transições com o JAR |
| `content/provenance.json` | Versão, checksums, contagens e validação |
| `reports/` | Reports de análise do jogo original |

Os IDs de blocks/states são preservados nesta versão do formato. O runtime usa
IDs numéricos e uma view imutável, sem alocar em consultas/transições. O registry
verificado tem 26.684 estados e ocupa 607.985 bytes. Ao alterar uma propriedade,
`BlockStates::with_property` encontra o estado com todas as outras propriedades
iguais. Não perde `waterlogged`, `shape`, `hinge`, `powered`, `age`, etc. Uma
propriedade/valor inválido retorna falha; não vira um estado genérico.

## Formato MCSR v1

Little-endian; sem ponteiros serializados. A tabela de strings usa UTF-8 com NUL.

| Parte | Layout |
|---|---|
| Header, 32 bytes | magic `MCSR`, version, blockCount, stateCount, pairCount, refCount, stringBytes, DataVersion |
| Block records, 16 bytes cada | nameOffset, defaultState, firstState, stateCount (uint32) |
| State records, 12 bytes cada | blockId, firstPropertyRef, propertyCount (uint32) |
| Property pairs, 8 bytes cada | nameOffset, valueOffset (uint32) |
| Property refs | Índices uint16 para os pares |
| Strings | Bytes UTF-8 terminados em NUL |

O leitor valida versão 1/DataVersion 3955, tamanhos, ranges, nomes terminados,
IDs de estados/blocos e referências antes de expor a view. O arquivo é gerado
para uso pessoal e não integra o build público nem o Git.

## Cliente e texturas

O cliente precisa vir da instalação legítima do usuário. Ele ainda não foi
fornecido/testado integralmente nesta sessão:

```sh
python3 -m pip install -r requirements-tools.txt
python3 tools/import_minecraft_assets.py \
  --client /caminho/legitimo/versions/1.21.1/1.21.1.jar \
  --assets-root /caminho/legitimo/assets
```

O hash do cliente deve corresponder ao vanilla 1.21.1. O index 17 e os resource
objects são verificados por hashes. Sem `--assets-root`, o relatório marca que
os recursos externos (por exemplo sons) não foram fornecidos. Não há download
de um cliente ou assets de terceiros.

O pack mantém as entradas originais e suas metadata. PNGs também geram MCPT RGBA
para o GS: header `MCPT`, version, width, height, channels=4, flags=1 (24 bytes),
seguido por todos os pixels. Alpha é convertido de 0..255 para 0..128 do GS. A
altura completa dos strips de animação é mantida; o futuro renderer deverá
interpretar a metadata e construir atlases/passes. Conversão não equivale a
renderer ou áudio implementados.

## Formato MCPK v1

Header de 32 bytes: magic `MCPK`, version, kind (1=assets, 2=world data), entryCount
(uint32); indexBytes e payloadBytes (uint64). Cada entrada do índice contém
pathBytes/reserved (uint16), CRC32 (uint32), payloadOffset/payloadSize (uint64),
seguido do path UTF-8. O offset é relativo ao começo do payload. Os caminhos são
ordenados e não podem escapar por `..`. Objetos grandes de uma instalação são
copiados em streaming pela ferramenta, sem exigir uma cópia gigante em memória.

O objetivo do pack é leitura/streaming no PS2. O decoder e os interpreters de
recipes, loot, noise/density e outros dados ainda estão pendentes. Importar esses
dados não satisfaz as regras de gameplay, nem prova paridade de mundo.
