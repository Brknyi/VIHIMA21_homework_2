# Szoftverbiztonság (VIHIMA21)
## Házi feladat 2.
### Szoftver implementáció

C/C++ nyelven írt open source alkalmazás, amely képes CIFF és CAFF formátumú képekből egy jpg vagy
webp formátumú előnézetet generálni.

**Használat**
`./parser –caff [path-to-caff].caff`
`./parser –ciff [path-to-ciff].ciff`

Példafájlok a *test_files* alatt találhatóak.

**Használt fájlformátumok**
1. CIFF
A CrySyS Image File Format egy tömörítés nélküli képformátum, amely a képhez tartozó metaadatok mellett közvetlen pixel információkat tartalmaz. A formátum specifikációja [itt](https://www.crysys.hu/downloads/vihima06/2020/CIFF.txt) található.
2. CAFF
A CrySyS Animation File Format egy tömörítés nélküli animációformátum, amely az animált képekhez tartozó metaadatok mellett CIFF képek tárolására alkalmas. A formátum specifikációja [itt](https://www.crysys.hu/downloads/vihima06/2020/CAFF.txt) található.
