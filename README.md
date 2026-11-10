# APD Laborator 6 – Eșantionarea semnalelor și filtrarea numerică

Banc de test în Tinkercad (`APD_Lab06_Esantionare_Filtrare`) cu două moduri, comutate din butonul de pe `D2`:

- **Mod 0** (LED `D8` stins): eșantionează sinusoida de la Function Generator pe `A0` cu `fs = 20 Hz` (`Ts = 50 ms`).
- **Mod 1** (LED `D8` aprins): citește TMP36 pe `A1`, adaugă zgomot artificial și afișează filtrele numerice în Serial Plotter.

Codul complet: [`src/lab06_filtrare.ino`](src/lab06_filtrare.ino).

> **Notă Tinkercad:** Serial Plotter-ul din Tinkercad nu înțelege formatul `Eticheta:valoare` (cel din Arduino IDE) și rămâne gol. De aceea programul trimite doar numere separate prin virgulă, în ordinea fixă din tabelele de mai jos.

În Mod 0 coloanele sunt: `LimitaMin (0.0)`, `LimitaMax (5.0)`, `Semnal_V`. În Mod 1, Serial Plotter afișează curbele (în această ordine):

| Curbă | Ce este |
|---|---|
| `Brut` | temperatura + zgomot ±2.5 °C + vârf parazit +4.5 °C (1 din 25) |
| `MedieMobila` | medie mobilă, `M = 8` |
| `FiltruEMA` | EMA, `α = 0.22` |
| `EMA_Fast` | EMA, `α = 0.6` (Ex. 2) |
| `EMA_Slow` | EMA, `α = 0.08` (Ex. 2) |
| `Median_EMA` | median pe 3 eșantioane → EMA `α = 0.22` (Ex. 3) |

## Exercițiul 1 – Frecvența critică Nyquist

`fs = 20 Hz`, `f_Nyquist = 10 Hz`, `f_alias = |f_semnal − k·fs|` (se alege `k` care dă valoarea din intervalul `[0, 10] Hz`).

| f_semnal | k | f_alias calculat | Ce se vede în Serial Plotter |
|---|---|---|---|
| 5 Hz  | 0 | 5 Hz  | sinusoidă corectă de 5 Hz (4 eșantioane pe perioadă) |
| 10 Hz | 0 sau 1 | 10 Hz | exact limita Nyquist: doar 2 eșantioane pe perioadă, semnalul apare ca un „zig-zag” sus-jos; amplitudinea depinde de fază (dacă eșantioanele cad pe trecerile prin offset, se vede o linie aproape constantă la 2.5 V) |
| 15 Hz | 1 | \|15 − 20\| = 5 Hz | sinusoidă **falsă** de 5 Hz (aliasing) |
| 20 Hz | 1 | \|20 − 20\| = 0 Hz | linie **constantă** (DC): la fiecare eșantion semnalul e în același punct al perioadei; nivelul depinde de fază (între 0.5 V și 4.5 V) |

Concluzie: pentru `f_semnal > fs/2` frecvența observată se „repliază” în intervalul `[0, fs/2]`. La `f_semnal = fs` informația dispare complet (se vede DC), iar la `f_semnal = fs/2` reconstrucția nu mai e sigură. De aceea în practică se ia `fs` de mai mult decât dublul lui `f_max` și se pune un filtru analogic trece-jos înainte de ADC.

## Exercițiul 2 – Netezire vs. timp de răspuns la EMA

Treaptă TMP36 20 °C → 60 °C. Pragul de 90% este `20 + 0.9·40 = 56 °C`.

Teoretic, după `n` eșantioane de la treaptă: `y[n] = y_final − (1 − α)^n · Δ`, deci 90% se atinge când `(1 − α)^n ≤ 0.1`:

```
n ≥ ln(0.1) / ln(1 − α)
```

| Filtru | α | n teoretic | n măsurat* | Timp (Ts = 50 ms) | Zgomot rezidual (abatere std.)* |
|---|---|---|---|---|---|
| `EMA_Fast` | 0.6  | 2.51 → **3**  | 3  | 150 ms | 1.06 °C |
| `FiltruEMA` | 0.22 | 9.3 → **10** | 10 | 500 ms | 0.52 °C |
| `EMA_Slow` | 0.08 | 27.6 → **28** | 28 | 1.4 s  | 0.37 °C |
| `MedieMobila` | M = 8 | **8** | 8 | 400 ms | 0.52 °C |

\* măsurat rulând aceeași logică de filtrare pe calculator (fără zgomot pentru numărul de eșantioane, cu zgomot pentru abaterea standard; zgomotul brut are ≈ 1.72 °C). În Tinkercad, din cauza zgomotului, numărul poate varia cu ±1–2 eșantioane.

Concluzie: `α` mare → răspuns rapid, dar trece mult zgomot; `α` mic → semnal foarte neted, dar întârziere mare. Alegerea lui `α` este un compromis între cele două.

## Exercițiul 3 – Filtru median pe 3 eșantioane

```cpp
float median3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }  // a <= b
  if (b > c) { b = c; }                      // b = min(b, c)
  return (a > b) ? a : b;                    // max(a, min(b, c))
}
```

În `loop()` se păstrează ultimele două citiri brute (`rawPrev1`, `rawPrev2`), se calculează `median3(rawPrev2, rawPrev1, noisyTemp)` și rezultatul intră în EMA (`Median_EMA`).

Un vârf izolat (+4.5 °C într-un singur eșantion) nu poate fi niciodată valoarea din mijloc a 3 eșantioane, deci medianul îl elimină complet înainte să ajungă în EMA. În simulare, valoarea maximă după treaptă a fost 61.76 °C la `FiltruEMA` față de 60.81 °C la `Median_EMA` (aceeași valoare `α = 0.22`). Costul: medianul adaugă încă ~1 eșantion de întârziere (11 în loc de 10 până la 90%).

## Verificare finală – răspunsuri

**De ce `lastSampleTime += SAMPLE_PERIOD_MS` cu `millis()` și nu `delay()`?**
Cu `delay(50)` perioada reală devine `50 ms + t_ADC + t_calcul + t_UART`, deci `fs` scade și variază (jitter). Cu `millis()` momentul următoarei citiri este fixat la `n·Ts`, indiferent cât a durat restul codului, iar `loop()` nu este blocat (butonul e citit în continuare). Adunarea pasului fix (în loc de `lastSampleTime = now`) nu lasă erorile să se acumuleze.

**Ce impune teorema Nyquist–Shannon? De ce apare sinusoida falsă de 2 Hz la 18 Hz?**
Un semnal poate fi reconstruit doar dacă `fs ≥ 2·f_max`. La `fs = 20 Hz` limita este 10 Hz. Semnalul de 18 Hz face 0.9 perioade între două eșantioane, adică „pare” că a rămas în urmă cu 0.1 perioade pe eșantion → 0.1 · 20 Hz = 2 Hz = `|18 − 20|`. Eșantioanele sunt identice cu ale unui semnal real de 2 Hz, deci nu mai pot fi deosebite software.

**Cum funcționează suma glisantă în bufferul circular?**
Se păstrează ultimele `M` valori și suma lor. La fiecare eșantion nou: se scade din sumă valoarea cea mai veche (`maBuffer[maIndex]`), se scrie valoarea nouă în locul ei, se adună la sumă și indexul avansează modulo `M`. Ieșirea este `suma / M`, deci costul e `O(1)` indiferent de `M`.

**Diferențe între Medie Mobilă, EMA și Median:**

| Filtru | Memorie | Rezistență la vârfuri izolate |
|---|---|---|
| Medie mobilă | `M` valori (+ suma) | slabă: vârful intră în medie cu ponderea `1/M` timp de `M` eșantioane |
| EMA | 1 valoare | slabă: vârful intră cu ponderea `α` și se stinge exponențial |
| Median (3–5) | 3–5 valori | foarte bună: un vârf izolat este eliminat complet |

În practică se combină: medianul elimină vârfurile, apoi media mobilă sau EMA netezește zgomotul mic.
