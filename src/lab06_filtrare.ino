// Laboratorul 06 APD - Esantionare, Aliasing si Filtrare Numerica
// Include rezolvarea exercitiilor 2 (EMA rapid / lent) si 3 (cascada Median3 + EMA)
//
// Mod 0 (LED stins):  esantionare A0 (Function Generator) la fs = 20 Hz
// Mod 1 (LED aprins): TMP36 pe A1 + zgomot artificial, filtrat in timp real

const int PIN_BTN_MODE = 2;
const int PIN_LED_MODE = 8;
const int PIN_SIG_GEN  = A0;
const int PIN_TMP36    = A1;

const unsigned long SAMPLE_PERIOD_MS = 50; // fs = 20 Hz (f_Nyquist = 10 Hz)
const int   MA_WINDOW      = 8;      // fereastra Medie Mobila
const float EMA_ALPHA      = 0.22f;  // EMA din laborator
const float EMA_FAST_ALPHA = 0.6f;   // Ex. 2: EMA rapid
const float EMA_SLOW_ALPHA = 0.08f;  // Ex. 2: EMA lent

int mode = 0; // 0 = Esantionare/Aliasing (A0), 1 = Filtrare TMP36 (A1)
int lastBtn = HIGH, stableBtn = HIGH;
unsigned long lastDebounce = 0;
unsigned long lastSampleTime = 0;

// Starea filtrelor
float maBuffer[MA_WINDOW];
int   maIndex = 0;
float maSum   = 0.0f;
bool  filtersInitialized = false;
float emaValue  = 0.0f;
float emaFast   = 0.0f;
float emaSlow   = 0.0f;
float medEma    = 0.0f;   // Ex. 3: iesirea cascadei Median3 -> EMA
float rawPrev1  = 0.0f;   // x[n-1]
float rawPrev2  = 0.0f;   // x[n-2]

// Ex. 3: valoarea din mijloc a trei numere (fara sortare completa)
float median3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }  // acum a <= b
  if (b > c) { b = c; }                      // b = min(b, c)
  return (a > b) ? a : b;                    // max(a, min(b, c))
}

void initFilters(float initialVal) {
  maSum = 0.0f;
  for (int i = 0; i < MA_WINDOW; i++) {
    maBuffer[i] = initialVal;
    maSum += initialVal;
  }
  maIndex  = 0;
  emaValue = initialVal;
  emaFast  = initialVal;
  emaSlow  = initialVal;
  medEma   = initialVal;
  rawPrev1 = initialVal;
  rawPrev2 = initialVal;
  filtersInitialized = true;
}

void setup() {
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_LED_MODE, OUTPUT);
  Serial.begin(115200);
  randomSeed(analogRead(A5));
  lastSampleTime = millis();
}

void loop() {
  unsigned long now = millis();

  // 1. Buton D2 cu debounce (40 ms) pentru comutarea modului
  int reading = digitalRead(PIN_BTN_MODE);
  if (reading != lastBtn) lastDebounce = now;
  if ((now - lastDebounce) > 40) {
    if (reading != stableBtn) {
      stableBtn = reading;
      if (stableBtn == LOW) {
        mode = 1 - mode;
        filtersInitialized = false;
      }
    }
  }
  lastBtn = reading;
  digitalWrite(PIN_LED_MODE, mode == 1 ? HIGH : LOW);

  // 2. Esantionare cu pas fix Ts = 50 ms (fara delay)
  if (now - lastSampleTime >= SAMPLE_PERIOD_MS) {
    lastSampleTime += SAMPLE_PERIOD_MS;

    if (mode == 0) {
      // MOD 0: semnal sinusoidal de pe A0
      int rawSig = analogRead(PIN_SIG_GEN);
      float vSig = (rawSig * 5.0f) / 1023.0f;
      // Serial Plotter din Tinkercad citeste doar numere separate prin virgula
      // (nu accepta "Eticheta:valoare"). Coloane: LimitaMin, LimitaMax, Semnal_V
      Serial.print("0.0,5.0,");
      Serial.println(vSig, 3);
    } else {
      // MOD 1: TMP36 + zgomot artificial
      int rawTmp = analogRead(PIN_TMP36);
      float tempClean = (((rawTmp * 5.0f) / 1023.0f) - 0.5f) * 100.0f;

      // zgomot aleator +/- 2.5 C + un varf parazit rar (1 din 25)
      float noise = random(-250, 251) / 100.0f;
      if (random(0, 25) == 0) noise += 4.5f;
      float noisyTemp = tempClean + noise;

      if (!filtersInitialized) initFilters(noisyTemp);

      // Medie Mobila O(1) cu suma glisanta
      maSum -= maBuffer[maIndex];
      maBuffer[maIndex] = noisyTemp;
      maSum += noisyTemp;
      maIndex = (maIndex + 1) % MA_WINDOW;
      float maOut = maSum / MA_WINDOW;

      // EMA din laborator
      emaValue = EMA_ALPHA * noisyTemp + (1.0f - EMA_ALPHA) * emaValue;

      // Ex. 2: EMA rapid si EMA lent
      emaFast = EMA_FAST_ALPHA * noisyTemp + (1.0f - EMA_FAST_ALPHA) * emaFast;
      emaSlow = EMA_SLOW_ALPHA * noisyTemp + (1.0f - EMA_SLOW_ALPHA) * emaSlow;

      // Ex. 3: Median3 pe ultimele 3 citiri brute, apoi EMA
      float med = median3(rawPrev2, rawPrev1, noisyTemp);
      rawPrev2 = rawPrev1;
      rawPrev1 = noisyTemp;
      medEma = EMA_ALPHA * med + (1.0f - EMA_ALPHA) * medEma;

      // Serial Plotter: o linie = un esantion, valori separate prin virgula
      // Coloane: Brut, MedieMobila, FiltruEMA, EMA_Fast, EMA_Slow, Median_EMA
      Serial.print(noisyTemp, 2); Serial.print(",");
      Serial.print(maOut, 2);     Serial.print(",");
      Serial.print(emaValue, 2);  Serial.print(",");
      Serial.print(emaFast, 2);   Serial.print(",");
      Serial.print(emaSlow, 2);   Serial.print(",");
      Serial.println(medEma, 2);
    }
  }
}
