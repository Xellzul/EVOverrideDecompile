# Build kit: input files

Files needed to build this game.
Check a copy with `sha256sum -c --ignore-missing SHA256SUMS`, or `7z h FILE` for the CRC32.

## EV Override 1.0.2 (both forks)

| file | source | bytes | CRC32 | what the build takes from it |
|---|---|---|---|---|
| `CWPro3Tools.cdr_.zip` | https://macintoshgarden.org/apps/codewarrior-pro-3 ("Mac Tools CD") | 277210155 | 8118847A | the Mac compiler plugins MW C/C++ PPC 2.1 b0014 and MW C/C++ 68K 2.1 b0013, the MacOS 68K Linker plugin, the MSL C headers and libraries, runtime, MacOS and MathLib68K libraries, the PPC stub libraries |
| `CW_PRO5.bin` | https://archive.org/download/cwpro5/Tools%20-%20Windows%2095%20NT/CW_PRO5.bin (archive.org item `cwpro5`, SHA-1 a9ca2123d1447ba13d6e49a6af2ba519d9fe2168) | 629929104 | F5FBF93B | Universal Interfaces 3.2, MWCPPC.exe 2.3 (SATAddOns.c), MWLinkPPC.exe 2.3 (the PPC linker), FlexLM's lmgr326b.dll and the disc's license.dat |
| `CW7_Gold_Tools.iso_.zip` | https://macintoshgarden.org/apps/codewarrior-7-gold-academic ("CW7 Tools, Retail edition") | 295269502 | 3C7C35F6 | MPW MWC68K 1.3, MWCPPC 1.3 and their CIncludes (MonitorTool, 68k and PPC) |
| `CWPro1Tools.sit` | https://www.macintoshrepository.org/327-codewarrior-professional-release-1 ("CWPro1Tools.sit", 131.69 MB, SHA-1 e9a3ffd18adce76b03995f3fbbd781769cf1a375 as the page lists it; StuffIt 5 holding `CWPro 1 Tools.toast`) | 131685719 | 295A078B | MPW MWCPPC 1.8 and MWC68K and their CIncludes (RegTool, Search, MoreFilesExtras, both CPUs; the Universal Interfaces of MacFiles, 68k) |
| `CWPro_2_Tools_199710.toast` | https://macintoshgarden.org/apps/codewarrior-pro-2 | 602869760 | C9450FCC | MPW MWCPPC 2.0 and MWC68K 2.0 and the CIncludes (SoundMixer, both CPUs) |
| `sat260b-libs.hqx` | https://hina.lysator.liu.se/pub/mac/sat/sat260b-libs.hqx | 372286 | 00162751 | Sprite Animation Toolkit 2.6.0b, `SAT(PPC).lib` and `SAT(68k).lib` |
| `SAT-240-libdoc.hqx` | https://hina.lysator.liu.se/pub/mac/sat/SAT-240-libdoc.hqx | 968588 | D474C113 | the SAT 2.4.0 Add-ons libraries |
| `EV_Override_1.0.2_f.sit` | https://macintoshgarden.org/games/escape-velocity-override | 6906541 | 1D750F80 | your copy of the game: the original data fork, resource fork and Finder info to compare with (exactly as the archive holds them); the resource fork the 68k build substitutes CODE and DATA into (every other resource and the header's reserved bytes are taken from it), and the Finder info, copied |