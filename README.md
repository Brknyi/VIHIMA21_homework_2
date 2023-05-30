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

**Tesztelés**
A tesztelés kapcsán 3 fájl került felhasználásra, amely nem helyes.
1. incorrect_size_with_incorrect_width_height.CAFF:
    A fájl mérete nem konzisztens a megadottal.
2. incorrect_width_height.CAFF: 
    A CIFF fájl width és height értéke negatív. Ha nem unsigned-ként lennének tárolva érdekesebb lefutás.
3. width_height_overflow.CAFF:
    A width és height értéke nagy így buffer overflow-t okoz.
    
Satikus

Code Review

A CAFF és CIFF leírás alapján az érintett részeken szélső esetek lekezelésének vizsgálata, valamint fájl és blokk méretek ellenörzése.

Valgrind

Memóriaszivárgás vizsgálata során Valgrind került használatra. Ennek során a felsorolt invalid tesztfájlok és egyes lefutási szálak kerültek tesztelésre. A lefutási szálak az egyes elágazások kézi kikényszerítésével történt.

További tesztelési lehetőségek
- Unit tesztek készítése
- fuzzer használata (AFL?)