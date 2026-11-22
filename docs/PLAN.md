# Plan

OriginPsy je lanac od četiri slota u koje ide bilo koji od šest modula, plus Fraza modulacija koja zna gdje si u 8, 16 ili 32 takta.

| Modul | Šta radi | Stanje |
| --- | --- | --- |
| Fraktal Echo | Delay čiji se ehi granaju kao drvo, svaka grana pomjerena za stepen skale | urađeno (v0.1) |
| Vokal Acid | Formant filter čiji samoglasnik prati notu koju acid linija svira | urađeno (v0.1) |
| Polimetar Gate | Tri pojasa, svaki sa svojim gate ritmom i dužinom (npr. 16, 7, 13 koraka) | urađeno (v0.1) |
| Fraza + Drop | Build-up vezan za poziciju u frazi, bez automatizacije | urađeno (v0.1) |
| Zap Rezonator | Svaki udarac na ulazu okida kratki "laser" zvižduk na noti iz skale | sledeće |
| Orbit Doppler | Zvuk kruži oko slušaoca po nacrtanoj putanji, sa pravim Doppler efektom | sledeće |
| Spektral Ratchet | Na svaki korak zamrzne spektar i ponavlja ga sa drugim intervalom ili maskom | poslije |

## Faze

1. Kostur, lanac od 4 slota, tempo iz hosta, CTest i GitHub Actions (urađeno)
2. Fraktal Echo (urađeno)
3. Vokal Acid (urađeno)
4. Polimetar Gate (urađeno)
5. Fraza i Drop (urađeno); drag and drop modulacija i preseti (sledeće)
6. Zap Rezonator i Orbit Doppler
7. Spektral Ratchet (FFT, latencija prijavljena hostu)
8. Završni krug: CPU, preseti za sve module, uputstvo, AU build

## Pravila za DSP

- Bez alokacije memorije i zaključavanja u `processBlock`; sve se priprema u `prepareToPlay`.
- DSP jezgro (`src/dsp`) je čisti C++ bez JUCE-a i testira se bez DAW-a.
- Parametri su glatki, promjena slotova ide preko kratkog fade-a.
- Oversampling samo gdje ima nelinearnosti (Drive u Vokal Acidu).
