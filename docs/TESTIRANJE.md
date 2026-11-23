# Testiranje u Bitwigu

Za svaku stavku: zvuči li dobro, ima li klikova, prati li tempo. Napiši šta ne valja i na kojem tempu.

## Opšte

- [ ] Plugin se učita kao VST3 na audio kanalu, otvori i zatvori bez problema
- [ ] Prozor se može povećati vučenjem ugla
- [ ] Projekat sačuvan i ponovo otvoren vraća sva podešavanja, i nacrtane gate korake
- [ ] Promjena modula u slotu ili ON/OFF ne pravi klik
- [ ] Promjena tempa u Bitwigu (138, 145, 150) i 7/8 takt: efekti ostaju u ritmu
- [ ] CPU u Bitwigu sa sva tri modula uključena

## Fraktal Echo

- [ ] Na jednom hi-hatu ili zapu: prvi eho tačno na izabranoj dužini (1/8, 1/16D...)
- [ ] Pitch Spread 0: obični eho bez promjene tona; 2 ili 3: rep se penje po skali
- [ ] Negativan Pitch Spread: rep se spušta
- [ ] Feedback do kraja: rep dugo traje ali ne eksplodira
- [ ] Scale: Phrygian, Hijaz, Harmonic Minor zvuče u tonalitetu lead-a

## Vokal Acid

- [ ] Na 303 liniji: prikaz pokazuje tačnu notu i samoglasnik se mijenja s notom
- [ ] Glide kratko (10 ms) i dugo (300 ms)
- [ ] Accent: akcentovane note se ističu
- [ ] Robot: metalan zvuk, bez pucketanja
- [ ] Na basu (niske note, 40 do 80 Hz) prepoznaje tonove

## Polimetar Gate

- [ ] Default (16, 7, 13): pad postaje pokretna tekstura, kick i bas (Low) ostaju mirni
- [ ] Steps mode: klik na prsten dodaje i briše korake
- [ ] Rate 1/16T i 1/8D
- [ ] Release 0,1 ms: oštro sjeckanje; da li klikće?
- [ ] Mix 50 %: bez faznog "šupljeg" zvuka

## Fraza

- [ ] Pozicija (takt X / 16) se poklapa sa Bitwigom
- [ ] DROP: kroz zadnja 4 takta fraze eho raste, gate se pojačava, na 1. dobi nove fraze sve se vrati
- [ ] Offset: početak fraze se pomjeri kad pjesma ne počinje na taktu 1
