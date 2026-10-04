# Build e referência local

## Ferramentas

O build verificado utiliza ps2dev **v2.0.0**, GCC **15.2.0** para
`mips64r5900el-ps2-elf`, PS2SDK e gsKit. O download e seu SHA-256 estão fixados em
`tools/toolchain.lock.json`. O instalador atual é para Linux x86_64; foi utilizado
em Ubuntu com glibc 2.39. Host tests exigem make, g++ com C++17 e Python 3.10+.
Os testes que executam a referência exigem **JDK 21** com `java` e `javac`.

```sh
python3 -m venv .local/venv
. .local/venv/bin/activate
python3 -m pip install -r requirements-tools.txt
python3 tools/bootstrap_ps2dev.py
export PS2DEV="$PWD/.local/ps2dev"
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH"
make test
make ps2
```

Saída: **`build/ps2/MinecraftPS2.elf`**, junto de um linker map. A compilação usa
o linkfile oficial do PS2SDK, C++17 sem RTTI/exceptions, precisão dupla de 64 bits
e flags que desativam fast-math e contração de operações. O build host utiliza o
mesmo núcleo em `src/core`, sem a camada PS2.

## Referência legítima

Forneça o bundle do servidor e os mappings oficiais **exatamente da 1.21.1**:

```sh
python3 tools/import_minecraft_reference.py \
  --server /caminho/legitimo/server-1.21.1.jar \
  --mappings /caminho/legitimo/server-1.21.1.txt
```

Alternativamente, baixe a distribuição pública oficial do servidor, disponibilizada
pela própria Mojang na [página da versão](https://www.minecraft.net/en-us/article/minecraft-java-edition-1-21-1):

```sh
python3 tools/import_minecraft_reference.py --download-official-server
```

O importador verifica SHA-1 do bundle e dos mappings, os SHA-256 dos JARs internos
e a identidade interna `1.21.1` / DataVersion `3955`. Ele não importa um cliente
de terceiros, não descompila arquivos e não modifica os JARs originais. A saída
fica em `.local/reference/`, ignorada pelo Git. Uma pasta existente não é sobrescrita.

```sh
export JAVA_HOME=/caminho/para/jdk-21
make parity
make reference-data
make registry-parity
make noise-parity
make simplex-parity
make float-parity
make factory-parity
make octave-parity
make blended-parity
make density-parity
make density-spline-parity
make density-data-parity
make density-batch-parity
make density-data-batch-parity
make noise-chunk-parity
make check-public
```

O harness Java tem um pacote próprio, pois as classes obfuscadas do JAR são
assinadas. Ele descobre símbolos pelos mappings locais e chama as classes
originais. Entradas, resultados e checksums ficam em `.local/parity/`,
`.local/registry-parity/`, `.local/noise-parity/`, `.local/simplex-parity/`, `.local/float-parity/`, `.local/factory-parity/`,
`.local/octave-parity/`, `.local/blended-parity/`, `.local/density-parity/`, `.local/density-spline-parity/` e
`.local/density-data-parity/`, `.local/density-batch-parity/`, `.local/density-data-batch-parity/`
e `.local/noise-chunk-parity/`. Grafos MCDG e inventário ficam em `.local/density-data/`. Uma falha
na referência não vira aprovação silenciosa; o comando falha.

## Console / emulador

O ELF atual é a inicialização da plataforma, com tela de diagnóstico. Ainda não
contém um mundo jogável. Use o carregador de homebrew de seu PS2 ou a execução de
ELF do PCSX2. BIOS/fontes não são incluídos no projeto. A leitura do controle usa
os módulos ROM `XSIO2MAN`/`XPADMAN`, com buffer DMA alinhado a 64 bytes e espera
não bloqueante por conexão. Módulos `sio2man`/`padman` já residentes são reutilizados
sem reset do IOP.

A tela e o stdout devem mostrar `Reference numeric vectors: PASS (1ffff / 1ffff)`.
Os dezessete bits verificam Legacy, Xoroshiro, os dois wrappers WorldgenRandom,
ImprovedNoise, factories/hash, PerlinNoise, NormalNoise, BlendedNoise, um grafo
de densidade, a ligação do clima Legacy, Simplex 2D, ilhas do End, spline e
compatibilidade binary32, interpolação/cache de NoiseChunk e avaliação em lote
de um grafo com input CacheOnce compartilhado, usando vetores pequenos.
Os vetores de regras vêm do JAR; os vetores da camada binary32 vêm de expressões
Java 21 próprias. Uma máscara diferente
indica divergência no target; preserve o valor ao reportar o boot. Esse probe
não demonstra geração de mundo completa nem valida toda a suíte no console.

Depois de `make ps2`, execute `python3 tools/check_ee_float.py` com o EE toolchain
no PATH. O CI também exige essa auditoria: os objetos de precisão, spline, End,
grafo, leitor e NoiseChunk e os helpers binary64 verificados devem conservar o caminho sem
instruções single-precision de cálculo/comparação. Isso não substitui execução
ou medição de custo no equipamento.

O dispositivo de arquivos é provido pela ponte POSIX do PS2SDK e pelos módulos do
carregador; o ELF preserva o IOP do carregador e verifica acesso a `rom0:ROMVER`.
Ele não instala drivers USB/memory card automaticamente nesta etapa. Passe um
caminho gravável como primeiro argumento do ELF e pressione SELECT para salvar
o diagnóstico. Isso testa escrita de arquivo, não o formato de save do Minecraft.

Validação a executar no equipamento: boot em NTSC/PAL, fonte FONTM, plug/unplug
do controle, timer, caminho de arquivo do carregador, custo do DMA e kernel
determinístico. Não há alegação de 30 FPS ou paridade em hardware antes dessas
medições.

## Build automático

O workflow [Native PS2 build and 1.21.1 parity](../.github/workflows/native.yml)
executa host tests, auditoria da árvore pública, build EE e as suítes contra o
servidor público oficial. Os jobs de build EE e paridade começam após host tests
aprovados. Os Actions são fixados por SHA e o ps2dev por versão/checksum.

Na [página Actions](https://github.com/Johnatafgfdgf/Minecraft-ps2-edition/actions),
abra uma execução aprovada e baixe o artifact **MinecraftPS2-elf**: ELF, linker
map, instruções e avisos de dependências. O artifact **parity-summary** contém
somente reports próprios com contagens, checksums e escopo. JARs, mappings,
reports originais, dados Minecraft e assets não são publicados como artifacts.
Os artifacts têm retenção de 90 dias e podem ser regenerados pelo workflow.
