// =============================================================================
// tests.ino - Tests automaticos para el radar acustico v7
// =============================================================================
// Sketch de auto-test. Compila como sub-sketch desde tests/.
// NO compilar junto con radar.ino (Arduino CLI compila todos los .ino).
//
// Como usar:
//   1. Compilar y subir radar.ino (sketch principal). Verificar que mide.
//   2. Compilar y subir tests/tests.ino (este sketch).
//   3. Abrir Monitor Serial a 115200.
//   4. Elegir modo en los primeros 5 segundos:
//      - 0 = solo HW
//      - 1 = solo DSP sintetico (no requiere target fisico)
//      - 2 = solo LIVE (target fisico a distancias conocidas)
//      - 3 = todos
//
// Tests:
//   HW-1  dac emite DC     (verificacion manual con multimetro)
//   HW-2  adc DC offset    (esperado [1500, 2500])
//   HW-3  adc RMS ruido    (esperado <100 mV)
//   HW-4  OLED presente    (responde en 0x3C)
//   DSP-1 correlacion 50cm (captura sintetica)
//   DSP-2 correlacion 80cm (captura sintetica)
//   DSP-3 correlacion 100cm (captura sintetica)
//   DSP-4 anti-acople      (acople fuerte + eco debil -> prefiere eco)
//   LIVE-1 target 50 cm    (target fisico)
//   LIVE-2 target 80 cm    (target fisico)
//
// Salida:
//   [TEST] nombre -> PASS|FAIL: detalle
//   Resumen: X PASS, Y FAIL de Z tests
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ---------------------------------------------------------------------------
// 1. HARDWARE (debe coincidir con radar.ino v7)
// ---------------------------------------------------------------------------
static const uint8_t PIN_CHIRP_DAC = 25;
static const uint8_t PIN_MIC_ADC   = 34;
static const uint8_t PIN_OLED_SDA  = 21;
static const uint8_t PIN_OLED_SCL  = 22;
static const uint8_t OLED_ADDR     = 0x3C;

static const uint32_t FS_NOMINAL = 20000UL;
static const uint16_t N_CHIRP = (FS_NOMINAL * 4) / 1000;
static const uint16_t N_CAPTURE = (FS_NOMINAL * 12) / 1000;
static const float V_SONIDO = 343.0f;
static const uint16_t CHIRP_SKIP = 30;

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// ---------------------------------------------------------------------------
// 2. RESULTADOS DE TESTS
// ---------------------------------------------------------------------------
static uint16_t test_pass_cnt = 0;
static uint16_t test_fail_cnt = 0;

void reportar_test(const char *nombre, bool ok, const char *detalle) {
  Serial.print(F("[TEST] "));
  Serial.print(nombre);
  Serial.print(F(" -> "));
  if (ok) {
    Serial.print(F("PASS"));
    test_pass_cnt++;
  } else {
    Serial.print(F("FAIL"));
    test_fail_cnt++;
  }
  if (detalle != nullptr) {
    Serial.print(F(": "));
    Serial.print(detalle);
  }
  Serial.println();
}

// ---------------------------------------------------------------------------
// 3. TESTS DE HARDWARE (HW-x)
// ---------------------------------------------------------------------------

// HW-1: requiere multimetro. Solo reporta y devuelve true.
bool hw1_dac_emite() {
  Serial.println(F("  >> Mida P25 con multimetro: 1.65 V +/-0.1 V esperado"));
  return true;
}

// HW-2: DC offset del MAX4466 en [1500, 2500]
bool hw2_adc_dc_offset() {
  uint32_t suma = 0;
  const int N = 200;
  for (int i = 0; i < N; i++) {
    suma += analogRead(PIN_MIC_ADC);
    delayMicroseconds(50);
  }
  uint16_t dc = suma / N;
  char buf[64];
  snprintf(buf, sizeof(buf), "DC=%u (rango [1500,2500])", dc);
  bool ok = (dc >= 1500 && dc <= 2500);
  reportar_test("HW-2 adc DC offset", ok, buf);
  return ok;
}

// HW-3: RMS del ruido en mV
bool hw3_adc_rms_ruido() {
  const int N = 1000;
  uint16_t vmin = 4095, vmax = 0;
  uint32_t suma = 0;
  uint64_t suma_sq = 0;
  for (int i = 0; i < N; i++) {
    uint16_t v = analogRead(PIN_MIC_ADC);
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
    suma += v;
    suma_sq += (uint32_t)v * (uint32_t)v;
    delayMicroseconds(50);
  }
  float media = (float)suma / N;
  float var = ((float)suma_sq / N) - media * media;
  if (var < 0) var = 0;
  float rms_mV = sqrtf(var) * 3.3f / 4095.0f * 1000.0f;
  char buf[64];
  snprintf(buf, sizeof(buf), "RMS=%.1f mV pp=%u (esperado <100 mV)",
           rms_mV, vmax - vmin);
  bool ok = (rms_mV < 100.0f);
  reportar_test("HW-3 adc RMS ruido", ok, buf);
  return ok;
}

// HW-4: OLED responde
bool hw4_oled() {
  Wire.beginTransmission(OLED_ADDR);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "I2C err=%u", err);
    reportar_test("HW-4 OLED I2C", false, buf);
    return false;
  }
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    reportar_test("HW-4 OLED begin", false, "oled.begin() fallo");
    return false;
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println(F("Selftest OK"));
  oled.display();
  reportar_test("HW-4 OLED presente", true, "responde en 0x3C");
  return true;
}

// ---------------------------------------------------------------------------
// 4. TESTS DSP CON CAPTURA SINTETICA (DSP-x)
// ---------------------------------------------------------------------------
static uint16_t captura_test[N_CAPTURE];
static float    captura_filtrada_test[N_CAPTURE];
static uint8_t  chirp_test[N_CHIRP];

void generar_chirp_test(float fs) {
  const float k = (4000.0f - 1000.0f) / 0.004f;
  const float dt = 1.0f / fs;
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    float t = i * dt;
    float fase = 2.0f * PI * (1000.0f * t + 0.5f * k * t * t);
    float muestra = 0.5f * (1.0f + sinf(fase));
    float ventana = 1.0f;
    if (t > 0.003f) {
      ventana = 1.0f - 0.5f * (t - 0.003f) / 0.001f;
      if (ventana < 0.0f) ventana = 0.0f;
    }
    chirp_test[i] = (uint8_t)(muestra * ventana * 255.0f);
  }
}

void inyectar_eco(uint16_t lag, float amp) {
  for (uint16_t j = 0; j < N_CHIRP && (lag + j) < N_CAPTURE; j++) {
    captura_test[lag + j] += (uint16_t)(chirp_test[j] * amp);
  }
}

float distancia_cm_test(uint16_t lag, float fs) {
  return V_SONIDO * ((float)lag / fs) * 100.0f / 2.0f;
}

// Biquad RBJ bandpass 2.5 kHz, Q=0.7, fs=20000 (igual a radar.ino v7)
void filtrar_banda_test() {
  static float xp[2][2] = {{0,0}, {0,0}};
  static float yp[2][2] = {{0,0}, {0,0}};
  const float b0 = 0.505076272276105f;
  const float b1 = 0.0f;
  const float b2 = -0.505076272276105f;
  const float a1 = -1.414213562373095f;
  const float a2 = 0.494923727723895f;
  for (uint16_t stage = 0; stage < 2; stage++) {
    for (uint16_t i = 0; i < N_CAPTURE; i++) {
      float xi = (float)captura_test[i];
      float yi = b0 * xi + b1 * xp[stage][0] + b2 * xp[stage][1]
               - a1 * yp[stage][0] - a2 * yp[stage][1];
      xp[stage][1] = xp[stage][0]; xp[stage][0] = xi;
      yp[stage][1] = yp[stage][0]; yp[stage][0] = yi;
      captura_test[i] = yi;
    }
  }
  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    captura_filtrada_test[i] = captura_test[i];
  }
}

// Replica EXACTA del correlacion_tiempo() de radar.ino v7
uint16_t correlacion_test(float fs, float &pico_out, uint16_t &max_lag_out,
                          float &max_val_out, float &second_val_out) {
  float tpl[N_CHIRP];
  float tpl_media = 0.0f;
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    tpl[i] = (float)chirp_test[i];
    tpl_media += tpl[i];
  }
  tpl_media /= N_CHIRP;
  for (uint16_t i = 0; i < N_CHIRP; i++) tpl[i] -= tpl_media;

  // DC de primeras CHIRP_SKIP muestras (F1)
  float dc = 0.0f;
  for (uint16_t i = 0; i < CHIRP_SKIP; i++) dc += captura_filtrada_test[i];
  dc /= CHIRP_SKIP;
  for (uint16_t i = 0; i < N_CAPTURE; i++) captura_filtrada_test[i] -= dc;

  const uint16_t LAG_MIN = CHIRP_SKIP;
  const uint16_t LAG_MAX = N_CAPTURE - N_CHIRP;
  const uint16_t MID_LAG = (LAG_MIN + LAG_MAX) / 2;
  const uint16_t SEP = N_CHIRP / 2;
  const float RATIO = 0.5f;

  float max_val = -1e30f;
  uint16_t max_lag = 0;
  float second_val = -1e30f;
  uint16_t second_lag = 0;

  for (uint16_t lag = LAG_MIN; lag < LAG_MAX; lag++) {
    float acc = 0.0f;
    for (uint16_t j = 0; j < N_CHIRP; j++) {
      acc += captura_filtrada_test[lag + j] * tpl[j];
    }
    if (acc > max_val) {
      if ((lag >= max_lag + SEP) || (max_lag >= lag + SEP)) {
        if (max_val > second_val) {
          second_val = max_val;
          second_lag = max_lag;
        }
      }
      max_val = acc;
      max_lag = lag;
    } else {
      if ((lag >= max_lag + SEP) || (max_lag >= lag + SEP)) {
        if (acc > second_val) {
          second_val = acc;
          second_lag = lag;
        }
      }
    }
  }

  max_val_out = max_val;
  max_lag_out = max_lag;
  second_val_out = second_val;

  bool es_acople = (max_lag < MID_LAG) && (second_val >= RATIO * max_val) && (second_val > 0.0f);
  uint16_t mejor_lag = es_acople ? second_lag : max_lag;
  pico_out = es_acople ? second_val : max_val;
  return mejor_lag;
}

bool test_dsp_objetivo(uint16_t eco_lag, float eco_amp, float dist_esperada,
                       float tol_cm, const char *nombre) {
  for (uint16_t i = 0; i < N_CAPTURE; i++) captura_test[i] = 2048;
  inyectar_eco(CHIRP_SKIP, 40.0f);   // acople realista (foam)
  inyectar_eco(eco_lag, eco_amp);    // eco
  filtrar_banda_test();

  float pico, max_val, second_val;
  uint16_t max_lag;
  uint16_t lag = correlacion_test((float)FS_NOMINAL, pico, max_lag, max_val, second_val);
  float dist = distancia_cm_test(lag, (float)FS_NOMINAL);

  char buf[96];
  snprintf(buf, sizeof(buf), "eco_lag=%u amp=%.0f -> lag=%u pico=%.0f dist=%.1f cm (esp %.0f+/-%.0f)",
           eco_lag, eco_amp, lag, pico, dist, dist_esperada, tol_cm);
  bool ok = fabsf(dist - dist_esperada) <= tol_cm;
  reportar_test(nombre, ok, buf);
  return ok;
}

bool test_dsp_50cm() {
  uint16_t lag_50 = (uint16_t)(50.0f * 2.0f * FS_NOMINAL / V_SONIDO / 100.0f);
  return test_dsp_objetivo(lag_50, 150.0f, 50.0f, 8.0f, "DSP-1 correlacion 50cm");
}
bool test_dsp_80cm() {
  uint16_t lag_80 = (uint16_t)(80.0f * 2.0f * FS_NOMINAL / V_SONIDO / 100.0f);
  return test_dsp_objetivo(lag_80, 60.0f, 80.0f, 10.0f, "DSP-2 correlacion 80cm");
}
bool test_dsp_100cm() {
  uint16_t lag_100 = (uint16_t)(100.0f * 2.0f * FS_NOMINAL / V_SONIDO / 100.0f);
  return test_dsp_objetivo(lag_100, 80.0f, 100.0f, 10.0f, "DSP-3 correlacion 100cm");
}
bool test_dsp_anti_acople() {
  for (uint16_t i = 0; i < N_CAPTURE; i++) captura_test[i] = 2048;
  inyectar_eco(CHIRP_SKIP, 120.0f);  // acople muy fuerte
  uint16_t lag_75 = (uint16_t)(75.0f * 2.0f * FS_NOMINAL / V_SONIDO / 100.0f);
  inyectar_eco(lag_75, 60.0f);
  filtrar_banda_test();

  float pico, max_val, second_val;
  uint16_t max_lag;
  uint16_t lag = correlacion_test((float)FS_NOMINAL, pico, max_lag, max_val, second_val);
  float dist = distancia_cm_test(lag, (float)FS_NOMINAL);

  char buf[96];
  snprintf(buf, sizeof(buf), "acople_fuerte + eco -> lag=%u pico=%.0f dist=%.1f cm (esp 75+/-10)",
           lag, pico, dist);
  bool ok = fabsf(dist - 75.0f) <= 10.0f;
  reportar_test("DSP-4 anti-acople", ok, buf);
  return ok;
}

// ---------------------------------------------------------------------------
// 5. TESTS LIVE (con target fisico)
// ---------------------------------------------------------------------------
float fs_calibrado_live = (float)FS_NOMINAL;

void calibrar_fs_live() {
  const int N = 200;
  uint32_t t0 = micros();
  for (int i = 0; i < N; i++) {
    volatile uint16_t dummy = analogRead(PIN_MIC_ADC);
    (void)dummy;
    delayMicroseconds(50);
  }
  uint32_t t1 = micros();
  fs_calibrado_live = 1.0e6f / ((float)(t1 - t0) / N);
}

void emitir_chirp_live() {
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    dacWrite(PIN_CHIRP_DAC, chirp_test[i]);
    delayMicroseconds(50);
  }
  dacWrite(PIN_CHIRP_DAC, 128);
}

void capturar_adc_live() {
  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    captura_test[i] = analogRead(PIN_MIC_ADC);
    delayMicroseconds(50);
  }
  for (uint16_t i = 0; i < N_CAPTURE; i++) captura_filtrada_test[i] = (float)captura_test[i];
}

bool test_live_objetivo(float dist_esperada_cm, float tol_cm, const char *nombre) {
  Serial.print(F("  >> Coloque target a "));
  Serial.print(dist_esperada_cm, 0);
  Serial.print(F(" cm. Pulse ENTER para continuar..."));
  uint32_t t0 = millis();
  while (millis() - t0 < 30000) {
    if (Serial.available()) {
      while (Serial.available()) Serial.read();
      break;
    }
    delay(100);
  }

  emitir_chirp_live();
  capturar_adc_live();
  filtrar_banda_test();

  float pico, max_val, second_val;
  uint16_t max_lag;
  uint16_t lag = correlacion_test(fs_calibrado_live, pico, max_lag, max_val, second_val);
  float dist = distancia_cm_test(lag, fs_calibrado_live);

  char buf[96];
  snprintf(buf, sizeof(buf), "lag=%u pico=%.0f dist=%.1f cm (esp %.0f+/-%.0f)",
           lag, pico, dist, dist_esperada_cm, tol_cm);
  bool ok = fabsf(dist - dist_esperada_cm) <= tol_cm;
  reportar_test(nombre, ok, buf);
  return ok;
}

// ---------------------------------------------------------------------------
// 6. RUNNER
// ---------------------------------------------------------------------------
enum ModoTest {
  T_TEST_HW = 0,
  T_TEST_DSP_SYNTH = 1,
  T_TEST_LIVE = 2,
  T_TEST_ALL = 3
};

void correr_hw() {
  Serial.println(F("\n--- TESTS HARDWARE ---"));
  hw1_dac_emite();
  reportar_test("HW-1 dac emite (verif. manual)", true, "verificar con multimetro");
  hw2_adc_dc_offset();
  hw3_adc_rms_ruido();
  hw4_oled();
}

void correr_dsp_synth() {
  Serial.println(F("\n--- TESTS DSP (captura sintetica) ---"));
  generar_chirp_test((float)FS_NOMINAL);
  test_dsp_50cm();
  test_dsp_80cm();
  test_dsp_100cm();
  test_dsp_anti_acople();
}

void correr_live() {
  Serial.println(F("\n--- TESTS LIVE (target fisico) ---"));
  calibrar_fs_live();
  Serial.print(F("  fs calibrado: "));
  Serial.print(fs_calibrado_live, 0);
  Serial.println(F(" Hz"));
  generar_chirp_test(fs_calibrado_live);
  test_live_objetivo(50.0f, 10.0f, "LIVE-1 target 50 cm");
  test_live_objetivo(80.0f, 12.0f, "LIVE-2 target 80 cm");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC_ADC, ADC_11db);

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("  RADAR_SELFTEST - Tests automaticos v7"));
  Serial.println(F("================================================"));
  Serial.println(F("En 5 seg, envie: 0=HW, 1=DSPsint, 2=LIVE, 3=todos"));
  Serial.println();

  uint32_t t0 = millis();
  ModoTest modo = T_TEST_ALL;
  while (millis() - t0 < 5000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '0') modo = T_TEST_HW;
      else if (c == '1') modo = T_TEST_DSP_SYNTH;
      else if (c == '2') modo = T_TEST_LIVE;
      else if (c == '3') modo = T_TEST_ALL;
      while (Serial.available()) Serial.read();
      break;
    }
    delay(50);
  }

  if (modo == T_TEST_HW || modo == T_TEST_ALL) correr_hw();
  if (modo == T_TEST_DSP_SYNTH || modo == T_TEST_ALL) correr_dsp_synth();
  if (modo == T_TEST_LIVE || modo == T_TEST_ALL) correr_live();

  Serial.println();
  Serial.println(F("================================================"));
  Serial.print  (F("  Resumen: "));
  Serial.print(test_pass_cnt);
  Serial.print(F(" PASS, "));
  Serial.print(test_fail_cnt);
  Serial.print(F(" FAIL"));
  Serial.println();
  Serial.println(F("================================================"));
}

void loop() {
  delay(1000);
}
