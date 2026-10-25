# Randomizer source and licence audit, and in-launcher generation feasibility (board items T4/T5)

Date: 2026-10-17. Companion documents: `docs/companion-cli-20261017.md` (CLI, proof and in-launcher design),
`docs/companion-ancientcave-20261017.md` (Ancient Cave on the companion). Code: `tools/companion/randomizer/`,
`tools/companion/cli/`. Evidence and logs: `/home/cidwel/.local/state/som-decomp/companion-cli-20261017/`
(upstream clone in `repo-moppu/`, builds in `build/` and `aot/`, ROMs in `out/` and `cli-out/`, reports in `reports/`).
Nothing from the upstream tree is copied into this repository.

## Verdicts

| Question | Verdict |
| --- | --- |
| Is the source public? | Yes: `https://github.com/Moppu/SecretOfManaRandomizer`, all five modes, v1.49 (tag, 2026-02-13). Ancient Cave generator, map generators, event maker and all hack code are in the repository. |
| Licence | LGPL-2.1 (full text in `LICENSE`, GitHub detects `LGPL-2.1`; per-file header only in `Program.cs`, "version 2.1 or any later version"). |
| Can we port it to C / Kotlin and ship it in the launcher? | Yes, as a derivative work licensed LGPL-2.1-or-later (or GPL, LGPL section 3), with source and notices. The rest of the launcher may keep its own licence if the port stays a separable library. |
| Does it need anything besides the user's ROM? | The US v1.0 ROM (2 MiB) and about 12 KB of embedded resources for Ancient Cave. The optional music pack is a separate download. |
| In-launcher generation, Thor (Android) | Feasible with a shared C port. Running the original .NET code on device is not verifiable today (details below). |
| In-launcher generation, 3DS | Feasible only through a C port (no .NET target exists). Time on an old 3DS is an estimate, not a measurement. |
| Available today | `som-dualscreen randomize` on the PC (headless build of the upstream source, byte-identical to the release exe on the two seeds compared), then `package` for each device. |

## 1. Source audit

### Repository facts (cloned 2026-10-17, commit `1404b7b` = tag `v1.49`)

| Item | Evidence |
| --- | --- |
| URL, owner | `github.com/Moppu/SecretOfManaRandomizer`, created 2024-08-11 on GitHub ("Initial commit of Mana Rando v1.37 - the first github drop" 2024-08-10), last push 2026-02-13. 104 commits: Moppleton 47, Mopples 24, Cyb3R 21, black-sliver 11, Sacko 1. |
| Tags and releases | `v1.40` .. `v1.49`; release `v1.49` carries one asset, `SoMRandomizer_v1.49.zip` (the earlier agent used it). The README says a music-pack build is published separately on `secretofmanaancientcave.blogspot.com`. |
| Size | 329 C# files, 92,407 lines; GUI (`forms/`, `config/ui/`, `help/`) about 12 k lines; `processing/` 74 k lines. |
| Modes | `processing/ancientcave` (13,111 lines), `openworld` (10,851), `chaos` (2,787), `vanillarando` (2,239), `bossrush` (1,247), plus `processing/hacks` (35,850, shared) and `processing/common` (9,624). |
| Build requirement | `SoMRandomizer.csproj`: `net462`, WinForms (`WinExe`), PackageReferences `U8XmlParser 1.6.1`, `System.Resources.Extensions 9.0.0`, `System.Text.Json` and compatibility packages. README: "Visual Studio 2017 or later". No build script, no CI file other than a GitHub build action added by a contributor (PR 59). |
| Command line | `Program.cs` already has `srcRom= dstRom= seed= options=`, but only for Open World ("it wouldn't be too hard to make it run for any mode"). |
| Contribution policy | README "Contributing: Blerp". 24 pull requests, 22 merged (black-sliver, Cyb3R, Sacko), so outside code is accepted. PR 95 (x3ro, "attempt to make a cross-platform build happen", a shared DLL for non-Windows front ends) was closed without merging; PR 85 replaced `System.Xml` by `U8Xml` (merged). No issue mentions a C, Kotlin or mobile port. Forks: 6 (black-sliver, Cyb3RGER, Sacko-AuSdL, Evilmachine, sbzappa), none with a new licence. A Discord server is the support channel; the author's address is in `MainForm.cs`. |
| Headless build proof | `tools/companion/randomizer/headless/` compiles the upstream `processing/`, `config/` (without UI), `util/`, `logging/` and the resources as `net10.0` on Linux with no WinForms: 0 errors (`dotnet build`, 6 s). |

### Licence evidence

- `LICENSE`: GNU Lesser General Public License v2.1, February 1999, 504 lines, unmodified FSF text.
- `SoMRandomizer/Program.cs` header: "Copyright (C) 2024 Moppleton ... This library is free software ... GNU Lesser General Public License ... version 2.1 of the License, or (at your option) any later version." No other source file has a header; every class carries `Author: Moppleton`, so the copyright notice has to be kept in the files we derive from.
- Third-party material inside the repository:
  - `util/rng/DotNet110Random.cs`: copied from .NET Foundation `Random.cs` (coreclr 1.1.0), MIT, header kept in the file.
  - `util/rng/HashcodeUtil.cs`: "Author: Unknown - this came off the internet somewhere" (the usual deterministic string hash).
  - `U8XmlParser 1.6.1` (NuGet, MIT) for the music XML.
  - `Resources/pokedata3.xml` (344 KB, Pokemon data, Open World Pokedex event) and `Resources/nesoberi/*.bmp` (about 82 KB, fan character pictures): provenance not stated. Not used by Ancient Cave.
  - `processing/ancientcave/MitchHedbergQuotes.cs` (341 lines) and `DemetriMartinQuotes.cs` (438 lines): quotations of two comedians used as Ancient Cave dialogue (`acDialogue`). The LGPL grant cannot cover other people's text; the upstream project ships it anyway. A port that keeps seed compatibility has to keep the tables, so this is a residual third-party-rights risk for any public redistribution; a private build is unaffected in practice. The alternative is to ship the port without the tables (dialogue choice then differs from the original, ROM bytes change in text only).
- Game data: the repository contains no Square or Nintendo data apart from the structure of what it patches. The generator reads the vanilla ROM at run time and writes a new file.

### Inputs the generator needs from the user's ROM

`RomGenerator.initGeneration` (upstream `processing/common/RomGenerator.cs`) checks, in this order:

1. The file is 2,097,152 bytes, or 2,097,152 + 512 (copier header, stripped).
2. Bytes `0xFFC0..0xFFCD` equal `"Secret of MANA"`; byte `0xFFD9` is 1 (US); byte `0xFFDB` is 0 (version 1.0). No hash or checksum is checked.

Consequences measured with the CLI: the Magno Spanish ROM (title `Secret of Mana (ESP)`, version byte 2) is refused ("Name of game did not match"); the randomizer cannot be run on the translation, and re-applying the translation on top of a randomized ROM is not possible (Ancient Cave rewrites events and text).

After that the whole 2 MiB is copied to the first half of a 4 MiB output (`outFile`), the code is appended from `0x200000`, and tables of the vanilla image are read: 19 documented offsets in `VanillaRomOffsets.cs` (music pointers `0x33D39..0x33EE0`, world map rows `0x68000`, compressed title code `0x77C00` and tiles `0x7B480`, doors `0x83000`, map trigger, piece and object lists `0x84000..0x87000`, events `0x90000` and `0xA0000`, collision `0xB0400`, tilesets `0xB4000`, map palette sets `0xC8000`, map pieces `0xD0004`) plus enemy, text, weapon and spell tables through 411 `origRom` references in `processing/`. Nothing is read from the ROM at a position that depends on the seed before the seed is applied; the input is the whole vanilla image.

### Embedded resources used by Ancient Cave (everything else is code)

| Resource | Bytes | Use |
| --- | ---: | --- |
| `customMaps/acManafortBossArenaL1.bin`, `acManafortNpcRoomL1.bin` | 1,024 + 624 | Manafort interior biome rooms |
| `mapPalettes/accave`, `acforest`, `acisland`, `acruins` (13 files) | 210 each (2,730) | palette sets per biome |
| `vineshroom_tiles.bin` | 3,584 | common hack |
| `difficulty_casual/hard/reallyhard.properties` | 1,360 / 1,358 / 1,324 | the three difficulty presets (the GUI loads them and passes the values as `difficulty_*` settings; the CLI does the same) |

Total about 12 KB. Not needed: `giftbox_tiles.bin` (winter theme), boss rush rest maps, `pokedata3.xml`, `nesoberi/`, the PNG icons. The optional `./music/` folder (`vanillamusictypes.xml`, samples, songs) is loaded with `File` calls and only when present; without it generation logs "Couldn't load vanilla music types" and continues (observed, `log_777.txt`), producing a valid ROM.

Settings: each mode's defaults are in `config/settings/*Settings.cs` (Ancient Cave: `acBoy/acGirl/acSprite`, `acMusic`, `acLength` short/medium/long = 8/16/24 floors, `BossFrequency`, `acDialogue`, `acDifficulty`, `acBiomeTypes`, `acFilter`) plus about 70 common QoL hack toggles. `somrando --mode M --list-options` prints them all.

### Determinism (seed sharing)

- RNG: `DotNet110Random` (Knuth subtractive generator, integer only, 222 lines) seeded by `HashcodeUtil.GetDeterministicHashCode(seed)` (integer hash with 32-bit wrap); a second generator seeded with `seed + "_cosmetic"` is used for cosmetics. Both are platform independent by construction (the file comments say so).
- Floating point: `Math.Pow` and similar in 26 places (`DamageCalculations.cs` 21, `CaveGenerator.cs` 10, `ForestGenerator.cs` 6, `StatusGlow.cs` 6, others 1 to 3). A C port must reproduce the results of .NET `Math.Pow` bit for bit where they feed `(int)` casts; differences between libm implementations are at most 1 ulp and only matter near integer boundaries. This is the main seed-compatibility risk and is caught by differential testing.
- Measured: the same seed and options give the same ROM across builds.

| Comparison | Result |
| --- | --- |
| Source-built headless (`net10.0`, CLR) vs the v1.49 release exe run through the earlier `ancientcave_gen`, same fixed options (`--flat-difficulty`, `acMusic=false`) | byte-identical on seed `777` and on seed `12345` (the file earlier called `ac_seed1.smc` is seed `12345`, not `1`) |
| NativeAOT executable vs CLR, five modes, seed 42, default settings | byte-identical in all five (ancientcave, open, rando, bossrush, chaos) |
| NativeAOT shared library called from C vs CLR, seed 777 | byte-identical (`test_native.c`) |
| Two different seeds | 290,193 of about 307,000 non-zero expansion bytes differ (code is relocated by seed-dependent sizes): there is no seed-independent core that could be shipped as a precomputed patch. |

Not measured: a run of the original GUI with the same seed (the GUI calls the same classes with the same default settings; the release exe comparison above is the closest we can do without Windows).

## 2. Feasibility of in-launcher generation

Measured on lumina (x86-64, under load): Ancient Cave default (medium, 16 floors) 2.4 s and 79 MB RSS on the CLR (includes the runtime), 1.9 s and 35 MB as NativeAOT; open 1.4 s, rando 0.4 s, bossrush 0.9 s, chaos 0.4 s. The work itself is small; the question is only how to run it on each device.

| Option | Thor (Android) | 3DS | Verdict |
| --- | --- | --- | --- |
| A. Embed the original .NET | NativeAOT for Android needs the `linux-bionic` ILCompiler/runtime packs; `dotnet publish -r linux-bionic-arm64 -p:PublishAot=true` with the NDK failed here: `microsoft.netcore.app.runtime.nativeaot.linux-bionic-arm64` is not published and `PrivateSdkAssemblies ItemGroup is required` (log `aot/android.log`). The supported route in .NET 10 is the `android` workload (preview), which needs `dotnet workload install` with root; not allowed here. Mono embedding means shipping a 20 to 60 MB runtime and a Xamarin-style build. | Impossible (no runtime for devkitARM/libctru). | Not recommended, not proven for Android. The x86-64 NativeAOT build does work (5.0 MB executable, C ABI `.so` 4.8 MB) and is the PC/Linux engine. |
| B. Bundle a runtime and run the original | glibc arm64 runtime cannot run on bionic; a musl static NativeAOT binary launched from `nativeLibraryDir` is conceivable but needs a musl cross toolchain (not present, not tested). | Impossible. | Rejected. |
| C. C port shared by Android (NDK/JNI) and 3DS (devkitARM), tested against the C# as oracle | Yes. | Yes. | Recommended. |
| D. Kotlin port | Yes, Android only, no 3DS reuse. Unsigned byte arithmetic (`UByte`, masks) is a bug source in about 20 k lines of byte pokes. | No. | Rejected in favour of C. |
| E. Generate on a PC and copy the ROM | Works today (`som-dualscreen randomize` then `package`). | Works today. | Phase 0, keep as fallback. |

### Size of the C port

Reachable for Ancient Cave with the shipped hack set: `processing/ancientcave` 13.1 k lines, the 89 hack classes registered by `RomGenerator` and `AncientCaveGenerator` about 20.5 k, `processing/common` 9.6 k, `util` 1.7 k, `config` (no UI) 1.7 k, `logging` 0.2 k: about 46 k lines of C#. About 60 % of it is code that writes 65816 machine code byte by byte (`outRom[context.workingOffset++] = 0xA9;` style), which ports mechanically; the map generators (`mapgen/`, 9 k lines) use `List`, `Dictionary` and the RNG and need real care. Dependencies to replace: `List<T>`, `Dictionary`, `HashSet`, string formatting for the dialogue wrap, `Math.Pow`. No threads, no reflection beyond resource lookup, no LINQ in a hot path.

Estimate (not a measurement): about 55 to 65 k lines of C. For a coding agent working module by module with the C# as oracle, 4 to 8 weeks of work, factor-2 uncertainty; for one human expert 3 to 5 months. The risk is in volume, not in algorithms: every module can be checked by `cmp` against the C# output for hundreds of seeds, and the CLI built here is that oracle. Dropping the toggles Ancient Cave never enables (fixing the profile to the defaults) removes roughly a quarter of the hack code; seeds with non-default common options then would not match, so keep the toggles as a second step.

### Memory and time on the 3DS

- RAM: input ROM 2 MiB, output 4 MiB, working tables and maps under 10 MiB in C (the NativeAOT build peaks at 35 MB including its runtime and GC). Generation runs before the emulator core starts, so it can use the whole application budget (64 MB on old 3DS, 124 MB with New 3DS memory mode), unlike gameplay where the core needs most of it.
- Time (estimate): lumina needs about 1.9 s for Ancient Cave managed and AOT-compiled. A native C version should be on the same order on a desktop, so 1 to 3 s there; an ARM11 at 268 MHz is roughly 40 to 100 times slower for this kind of integer and branchy code, which gives 1 to 4 minutes on an old 3DS and 20 to 60 s on a New 3DS at 804 MHz. Plan a progress bar (the generator has about 90 named steps, `Adding: [name]` in the log) and run it once per seed, with the cache below. Measuring is the first task after the C core runs on a device.
- Storage: 4 MiB per generated ROM; an LRU cache of 8 ROMs is 32 MiB.

### Plan

| Phase | Work | Output |
| --- | --- | --- |
| 0 (done) | Headless build, CLI, packaging, verify, NativeAOT executable and C ABI library on PC | `tools/companion/cli`, `tools/companion/randomizer` |
| 1 | Launcher lists ROMs from a folder with `som_variant.json`; "Randomized" entries open pre-generated ROMs; Android and 3DS pick the variant record at load (`inspect` in C, about 80 lines) | needs the other agent's launcher work |
| 2 | C port of the shared core (`util`, RNG, hash, config, `RomGenerator` pipeline, resource tables as C arrays, the 25 hack classes in the order they appear in the log), differential test harness (`cmp` against `somrando` for N seeds and for all setting permutations that exist in the GUI) | `libsomrando` (C, no allocation after init, progress callback) |
| 3 | Ancient Cave generation in C: event maker, `mapgen` biomes, enemy/drop/shop changes, then the remaining modes in this order: open world, boss rush, chaos, rando | seeds identical to the C# |
| 4 | In-launcher UI (design in `docs/companion-cli-20261017.md`), cache, progress, errors; measure old/new 3DS and the Thor | device numbers replace the estimates |

Interim shortcut for Android only: if a future `android` workload build of NativeAOT works, `NativeExports.cs` already exposes `somrando_generate`/`somrando_version` with a C ABI, so JNI could call it directly and phase 2 would be needed for the 3DS only. This is untested on Android; the x86-64 shared library is tested.

## 3. Licence obligations for a port (LGPL-2.1)

Text read from `LICENSE`; this is the project's reading, not legal advice.

| Topic | What follows |
| --- | --- |
| A C or Kotlin translation is a "work based on the Library" | Section 0 of the LGPL defines a derivative as a work containing the Library "either verbatim or with modifications and/or translated straightforwardly into another language". The port is therefore derived and must be distributed under the LGPL (section 2c), or under the GPL v2 or later (section 3), or LGPL 2.1 "or any later version" as the header allows. A line-by-line translation cannot claim to be clean-room. |
| Section 2a "the modified work must itself be a software library" | Ship the port as a library (`libsomrando`, C API, progress callback) and keep UI and launcher outside it. |
| Notices | Keep the copyright notice and the LGPL text, mark changed files with the date and what changed (2b), keep `Author: Moppleton` comments, add a notice that the work is derived from `Moppu/SecretOfManaRandomizer` v1.49 and from the MIT-licensed .NET `Random`. |
| Source | Distribute the source of the port with the binary or a written offer (sections 3 and 4); the repository itself can be that source if it is public. |
| Linking the port into the launcher | Section 6: the combined work may carry our terms provided the user may modify the library and relink. Use a separate shared library on Android (`libsomrando.so` replaceable in the build), and on the 3DS provide the object files or the full app source (the project is open source anyway, so this is automatic). |
| What we may not do | Strip the notices, relicense the port as proprietary or non-commercial-only, or ship a closed binary without the source. |
| What we may do | Port it, embed it, change it (including a different seed derivation, with the loss of compatibility), distribute it with the app, add modes, and keep our own code outside the library under any licence compatible with section 6. |
| The ROMs | The licence covers the program, not what it outputs; the output contains Square/Nintendo code and data from the user's ROM. Never distribute ROMs or patches made from them; generation always happens on the user's device from the user's file. This is the same rule the repository already follows. |
| Interplay with Snes9x | The companion already packs the Snes9x core, whose licence is non-commercial. That is a different problem (a local build, never a store APK) and is not changed by this port. |
| Third-party tables | The comedian quotes and the Pokemon/nesoberi data have an unclear provenance; leave out what Ancient Cave does not need (Pokemon, nesoberi); decide on the quotes before any public release. |
| Courtesy, not a requirement | The project accepts pull requests; ask the author (Discord, address in `MainForm.cs`) before publishing a derived library, and offer upstream the headless project (it makes the generator buildable on Linux, which PR 95 tried). |

## 4. What was not verified

- Android NativeAOT (`linux-bionic-arm64`): fails to restore the runtime pack; no workaround without root.
- Any run on a Thor or a 3DS; the 3DS time and memory figures are estimates.
- The other randomizer modes (open, rando, boss rush, chaos) were generated and recognised by `inspect`, but not played: the companion macros are marked `read_only_until_verified` for them.
- A real GUI run of the original app for the byte comparison; the release exe path through `ancientcave_gen` was compared instead.
- Seeds with non-default options or the music pack present.
