# OriginPsy

FX plugin (VST3, AU na macOS-u i Standalone) sa efektima napravljenim za psytrance, kojih nema kao gotovih pluginova. Prva verzija ima tri modula i Fraza modulaciju.

## Kako radi

Gore su četiri **slota**. U svaki biraš modul, a zvuk prolazi kroz slotove s lijeva na desno. Dugme **ON** na slotu ga uključuje ili preskače. Isti modul radi samo u jednom slotu; ako ga izabereš i u drugom, taj drugi se preskače i to piše na kartici. Klik na slot otvara njegov modul u sredini.

Tempo i pozicija takta dolaze iz DAW-a. Kad DAW ne svira (ili u Standalone verziji), plugin ima svoj sat na tempu iz DAW-a ili 140 BPM, pa se efekti čuju i dok podešavaš.

### Fraktal Echo

Delay čiji se svaki eho grana na dva, kao drvo: jedna grana nastavlja istim korakom, druga je kraća (**Branch**, 0,618 je zlatni rez) i pomjerena za nekoliko stepeni skale (**Pitch Spread**, u izabranoj **Scale**). Rep zvuči kao melodija u tonalitetu, a ne kao isti zvuk koji tone. Crtež lijevo pokazuje drvo: vrijeme ide udesno, visina tona gore i dolje.

- **Time**: korak glavne grane (1/32 do 1 takt, triplete i tačkaste)
- **Generations**: koliko puta se grana (1 do 4, odnosno 1 do 15 eha)
- **Feedback**: cijelo drvo se vraća na ulaz, pa se tonovi nastavljaju penjati ili spuštati
- **Damp**: tamniji ehi, **Width**: koliko se grane šire lijevo i desno, **Mix**

### Vokal Acid

Formant filter koji prati visinu tona acid ili lead linije i svakoj noti (C do B) daje samoglasnik iz mape (A, E, I, O, U). Linija tako "govori" po melodiji, bez automatizacije.

- **Glide**: koliko brzo zvuk klizi s jednog samoglasnika na drugi
- **Formant**: pomjera sve formante gore ili dolje (polutonovi)
- **Resonance**, **Accent** (nagli skok jačine na tren pojača rezonancu i glasnoću), **Drive** (2x oversampling), **Mix**
- **Character**: Male, Female ili Robot (formanti se zaključaju na harmonike tona, pa je zvuk metalan)

Lijevo se vidi koji samoglasnik trenutno svira i koju notu plugin čuje.

### Polimetar Gate

Zvuk se dijeli na tri pojasa (**Low | Mid** i **Mid | High**), a svaki pojas ima svoj gate ritam i svoju dužinu. Na primjer 16, 7 i 13 koraka: ritmovi se ponovo poklope tek posle mnogo taktova, pa jedan pad postane tekstura koja se ne ponavlja.

- **Mode**: Euclid (koraci **Hits** raspoređeni što ravnomjernije kroz **Steps**, **Rotate** ih pomjera) ili Steps (klikom na prsten crtaš korake)
- **Rate**: dužina jednog koraka
- **Depth**: koliko se pojas utišava između udaraca (Low je po defaultu 0 %, da kick i bas ostanu mirni)
- **Length**: koliko dugo je gate otvoren unutar koraka, **Attack** i **Release** da nema klikova

### Fraza

Kriva koja zna gdje si u frazi od 8, 16 ili 32 takta. Knobovi desno određuju koliko kriva pomjera koji parametar (Echo Mix, Echo Feedback, Echo Damp, Vokal Formant, Vokal Resonance, Gate Depth), pa se build-up pravi sam, bez linija automatizacije. **Offset** pomjera početak fraze za nekoliko taktova. Dugme **DROP** jednim klikom postavi gotov build-up kroz zadnja 4 takta fraze.

## Build

Treba CMake 3.22 ili noviji i C++20 kompajler (Visual Studio 2022 na Windowsu, Xcode na macOS-u). JUCE 8.0.9 se skida sam.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Plugin je u `build/OriginPsy_artefacts/Release/VST3/OriginPsy.vst3`. Sa `-DORIGINPSY_COPY_AFTER_BUILD=ON` se sam kopira u sistemski folder za pluginove.

GitHub Actions pravi Windows i macOS build na svaki push i PR; gotov plugin se skida iz taba **Actions**, pod **Artifacts**.

## Testovi

- `originpsy_dsp_tests`: DSP jezgro bez JUCE-a (vrijeme eha, pitch tracker, euklidski ritmovi, polimetar, Fraza, null testovi, stabilnost pri maksimalnim podešavanjima, CPU)
- `originpsy_plugin_tests`: cijeli plugin bez DAW-a (sample rate 44,1 do 192 kHz, razne veličine bloka, čuvanje stanja, Drop)
- pluginval na nivou 5 u GitHub Actions (Windows)
- `originpsy_plugin_tests --screenshot slika.png 1` snimi izgled plugina (1 = Echo, 2 = Vokal, 3 = Gate)

Lista za ručno testiranje u Bitwigu je u [docs/TESTIRANJE.md](docs/TESTIRANJE.md).

## Plan

Cijeli plan sa svih šest modula je u [docs/PLAN.md](docs/PLAN.md). Slijede: drag and drop modulacija i preseti, pa Zap Rezonator, Orbit Doppler i Spektral Ratchet.
