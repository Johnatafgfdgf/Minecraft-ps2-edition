# Third-party dependencies

The native ELF links open-source PS2 development libraries. Minecraft files are
not linked, embedded or redistributed by the public build.

| Dependency | Used for | Upstream source / license |
|---|---|---|
| PS2SDK | EE startup, kernel, controller, timer and filesystem bridge | [Source](https://github.com/ps2dev/ps2sdk), [Academic Free License 2.0](https://github.com/ps2dev/ps2sdk/blob/master/LICENSE); individual files retain their notices |
| gsKit / dmaKit | Graphics Synthesizer / GIF DMA / ROM font rendering | [Source](https://github.com/ps2dev/gsKit), [Academic Free License 2.0](https://github.com/ps2dev/gsKit/blob/master/LICENSE), copyright Chris Gilbert and contributors |
| GCC / libgcc / libstdc++ | Toolchain and compiler runtime | [GCC source](https://gcc.gnu.org/releases.html), [runtime exception](https://www.gnu.org/licenses/gcc-exception-3.1.html), notices in upstream sources |
| Newlib | Native C/POSIX runtime | [Source and component notices](https://sourceware.org/newlib/) |
| Pillow | Optional host-only PNG conversion | [Source/license](https://github.com/python-pillow/Pillow); not part of the ELF |

The exact binary toolchain distribution is identified by version and SHA-256 in
`tools/toolchain.lock.json`. Its [build repository and dependency scripts](https://github.com/ps2dev/ps2dev/tree/v2.0.0)
provide the corresponding development sources. The public ELF package includes
this notice, the [AFL 2.0 license text](licenses/AFL-2.0.txt) and the build instructions.

The FONTM font is read from the user's console BIOS at runtime. No BIOS dump or
Minecraft proprietary resource is included. The reference oracle runs a local
official 1.21.1 server; its JARs, mappings and reports are excluded from the repository
and from ELF artifacts. Published parity summaries contain only authored counts,
checksums, provenance identifiers and test scope.
