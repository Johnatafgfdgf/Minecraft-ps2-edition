# Build e referência local

## Ferramentas

O build verificado utiliza ps2dev **v2.0.0**, GCC **15.2.0** para
`mips64r5900el-ps2-elf`, PS2SDK e gsKit. O download e seu SHA-256 estão fixados em
`tools/toolchain.lock.json`. O instalador atual é para Linux x86_64; foi utilizado
em Ubuntu com glibc 2.39. Host tests exigem make, g++ com C++17 e Python 3.10+.
Os testes que executam a referência exigem **JDK 21** com `java` e `javac`.

```sh
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
make check-public
```

O harness Java tem um pacote próprio, pois as classes obfuscadas do JAR são
assinadas. Ele descobre símbolos pelos mappings locais e chama as classes
originais. Entradas, resultados e checksums ficam em `.local/parity/`. Uma falha
na referência não vira aprovação silenciosa; o comando falha.

## Console / emulador

O ELF atual é a inicialização da plataforma, com tela de diagnóstico. Ainda não
contém um mundo jogável. Use o carregador de homebrew de seu PS2 ou a execução de
ELF do PCSX2. BIOS/fontes não são incluídos no projeto. A leitura do controle usa
os módulos ROM `XSIO2MAN`/`XPADMAN`, com buffer DMA alinhado a 64 bytes e espera
não bloqueante por conexão.

O dispositivo de arquivos é provido pela ponte POSIX do PS2SDK e pelos módulos do
carregador; o ELF preserva o IOP do carregador e verifica acesso a `rom0:ROMVER`.
Ele não instala drivers USB/memory card automaticamente nesta etapa. Passe um
caminho gravável como primeiro argumento do ELF e pressione SELECT para salvar
o diagnóstico. Isso testa escrita de arquivo, não o formato de save do Minecraft.

Validação a executar no equipamento: boot em NTSC/PAL, fonte FONTM, plug/unplug
do controle, timer, caminho de arquivo do carregador, custo do DMA e kernel
determinístico. Não há alegação de 30 FPS ou paridade em hardware antes dessas
medições.
