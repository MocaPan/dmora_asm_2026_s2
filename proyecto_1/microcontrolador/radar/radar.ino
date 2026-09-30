// radar.ino — Radar acústico (ESP32 + MAX4466 + buzzer con +4V serie)
// ESP32D        DAC1 -> P25,  ADC -> P34 (ADC1_CH6),  I2C -> P21 SDA / P22 SCL
// MAX4466       VCC -> 3V3, GND -> GND, AO -> P34
// Buzzer        (-) -> GND, (+) -> nodo audio
// Bateria 4V    (+) -> nodo audio, (-) -> GND comun
// OLED SSD1306  VCC -> 3V3, GND -> GND, SDA -> P21, SCL -> P22, addr 0x3C

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduinoFFT.h>

// Pines
static const uint8_t PIN_CHIRP_DAC = 25;
static const uint8_t PIN_MIC_ADC   = 34;
static const uint8_t PIN_OLED_SDA  = 21;
static const uint8_t PIN_OLED_SCL  = 22;
static const uint8_t OLED_ADDR     = 0x3C;
static const int8_t  OLED_RESET    = -1;

Adafruit_SSD1306 oled(128, 64, &Wire, OLED_RESET);

// Parametros de la senal
static const uint32_t FS_NOMINAL = 20000UL;
static float fs_calibrado = (float)FS_NOMINAL;

static const uint16_t CHIRP_DURATION_MS = 4;
static const float    CHIRP_F0_HZ       = 1000.0f;
static const float    CHIRP_F1_HZ       = 4000.0f;

// Buffers dimensionados al peor caso (fs_nominal); en runtime se usan solo
// los primeros N_CHIRP/N_CAPTURE recalculados.
static const uint16_t N_CHIRP_MAX = (FS_NOMINAL * CHIRP_DURATION_MS) / 1000;
static uint16_t N_CHIRP = N_CHIRP_MAX;

static const uint16_t CAPTURE_DURATION_MS = 12;
static const uint16_t N_CAPTURE_MAX = (FS_NOMINAL * CAPTURE_DURATION_MS) / 1000;
static uint16_t N_CAPTURE = N_CAPTURE_MAX;

static const float V_SONIDO = 343.0f;

// Rango valido (cm). Con fs_calibrado ~11000 Hz, CHIRP_SKIP=30 -> ~47 cm
// de zona ciega (acople directo buzzer->mic).
static const float DISTANCIA_MIN_CM = 45.0f;
static const float DISTANCIA_MAX_CM = 150.0f;

static const float UMBRAL_PICO_MIN          = 30000.0f;
static const float UMBRAL_PICO_FACTOR_NOISE = 5.0f;

// Tamano del buffer de mediana movil
static const uint8_t N_FILTRO = 3;

// Lags iniciales a descartar (zona ciega del acople directo)
static const uint16_t CHIRP_SKIP = 30;

// Debug de la curva de correlacion. Poner en 1 para imprimir top-5 picos.
#define DEBUG_CORR 0

// FFT (potencia de 2, >= N_CAPTURE_MAX)
static const uint16_t FFT_SIZE = 256;

// Cada cuantos loops del loop() se imprime el peak del espectro
static const uint8_t FFT_EVERY_N_LOOPS = 20;

// Comando Serial para forzar espectro completo
static const char FFT_TRIGGER_CMD = 'f';

// Buffers
uint8_t  chirp_template[N_CHIRP_MAX];
uint16_t captura[N_CAPTURE_MAX];
static float captura_filtrada[N_CAPTURE_MAX];
static float captura_dc = 0.0f;
static float captura_centrada[N_CAPTURE_MAX];

// arduinoFFT usa double (no float)
static double fft_vReal[FFT_SIZE];
static double fft_vImag[FFT_SIZE];

// Buffer del filtro mediana movil
static float   buffer_d[N_FILTRO];
static uint8_t buffer_count = 0;
static uint8_t buffer_idx   = 0;

// Ruido de fondo medido en diagnostico_inicial() (RMS en mV)
static float ruido_piso_mV = 0.0f;

// Generacion del chirp (1-4 kHz, ventana Tukey)
void generar_chirp() {
  const float k  = (CHIRP_F1_HZ - CHIRP_F0_HZ) / (CHIRP_DURATION_MS * 0.001f);
  const float dt = 1.0f / fs_calibrado;
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    float t = i * dt;
    float fase    = 2.0f * PI * (CHIRP_F0_HZ * t + 0.5f * k * t * t);
    float muestra = 0.5f * (1.0f + (float)sinf(fase));
    float ventana = 1.0f;
    if (t > 0.003f) {
      ventana = 1.0f - 0.5f * (t - 0.003f) / 0.001f;
      if (ventana < 0.0f) ventana = 0.0f;
    }
    chirp_template[i] = (uint8_t)(muestra * ventana * 255.0f);
    if ((i & 0x1F) == 0) yield();
  }
}

// Emision del chirp (DAC1 -> nodo audio del buzzer)
void emitir_chirp() {
  uint32_t emit_period_us = (uint32_t)(1e6f / fs_calibrado);
  if (emit_period_us > 20) emit_period_us -= 20;
  else emit_period_us = 0;
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    dacWrite(PIN_CHIRP_DAC, chirp_template[i]);
    if (emit_period_us > 0) delayMicroseconds(emit_period_us);
  }
  dacWrite(PIN_CHIRP_DAC, 128);
}

// Captura del ADC (fs_calibrado, P34)
void capturar_adc() {
  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    captura[i] = analogRead(PIN_MIC_ADC);
  }
}

// Pre-filtrado: biquad RBJ bandpass 2.5 kHz, Q=0.7, 2 etapas en cascada
static float bq_b0 = 0.5051f, bq_b1 = 0.0f, bq_b2 = -0.5051f;
static float bq_a1 = -1.4142f, bq_a2 = 0.4949f;

static void compute_biquad_coefs(float fs, float f0, float Q,
                                 float &b0, float &b1, float &b2,
                                 float &a1, float &a2) {
  float w0 = 2.0f * (float)PI * f0 / fs;
  float cosw = (float)cosf(w0);
  float sinw = (float)sinf(w0);
  float alpha = sinw / (2.0f * Q);
  b0 = alpha;
  b1 = 0.0f;
  b2 = -alpha;
  a1 = -2.0f * cosw;
  a2 = 1.0f - alpha;
}

void filtrar_banda(uint16_t *x, float *y, uint16_t n) {
  static float x_prev[2][2] = {{0,0}, {0,0}};
  static float y_prev[2][2] = {{0,0}, {0,0}};
  // Stage 1: x -> y
  for (uint16_t i = 0; i < n; i++) {
    float xi = (float)x[i];
    float yi = bq_b0 * xi + bq_b1 * x_prev[0][0] + bq_b2 * x_prev[0][1]
             - bq_a1 * y_prev[0][0] - bq_a2 * y_prev[0][1];
    x_prev[0][1] = x_prev[0][0];
    x_prev[0][0] = xi;
    y_prev[0][1] = y_prev[0][0];
    y_prev[0][0] = yi;
    y[i] = yi;
  }
  // Stage 2: y -> y (in-place)
  for (uint16_t i = 0; i < n; i++) {
    float xi = y[i];
    float yi = bq_b0 * xi + bq_b1 * x_prev[1][0] + bq_b2 * x_prev[1][1]
             - bq_a1 * y_prev[1][0] - bq_a2 * y_prev[1][1];
    x_prev[1][1] = x_prev[1][0];
    x_prev[1][0] = xi;
    y_prev[1][1] = y_prev[1][0];
    y_prev[1][0] = yi;
    y[i] = yi;
  }
}

// Analisis espectral (FFT, N_FFT=256, ventana Hann)
void compute_and_print_fft_peak() {
  uint32_t t0 = micros();

  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    fft_vReal[i] = (double)captura_centrada[i];
  }
  for (uint16_t i = N_CAPTURE; i < FFT_SIZE; i++) {
    fft_vReal[i] = 0.0;
  }
  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    fft_vImag[i] = 0.0;
  }

  arduinoFFT FFT = arduinoFFT(fft_vReal, fft_vImag, FFT_SIZE,
                              (double)fs_calibrado);
  FFT.Windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  double major_f = 0.0, major_v = 0.0;
  FFT.MajorPeak(&major_f, &major_v);

  uint32_t t1 = micros();
  Serial.print(F("[FFT] pico="));
  Serial.print(major_f, 1);
  Serial.print(F(" Hz mag="));
  Serial.print(major_v, 2);
  Serial.print(F(" ("));
  Serial.print((t1 - t0) / 1000);
  Serial.println(F(" ms)"));
}

// Espectro completo en formato CSV (bin, freq_Hz, magnitud)
void print_full_spectrum() {
  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    fft_vReal[i] = (double)captura_centrada[i];
  }
  for (uint16_t i = N_CAPTURE; i < FFT_SIZE; i++) {
    fft_vReal[i] = 0.0;
  }
  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    fft_vImag[i] = 0.0;
  }

  arduinoFFT FFT = arduinoFFT(fft_vReal, fft_vImag, FFT_SIZE,
                              (double)fs_calibrado);
  FFT.Windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  Serial.println(F("=== ESPECTRO FFT (bin,freq_Hz,magnitud) ==="));
  const double df = (double)fs_calibrado / (double)FFT_SIZE;
  for (uint16_t k = 1; k < FFT_SIZE / 2; k++) {
    double f = (double)k * df;
    Serial.print(k);
    Serial.print(F(","));
    Serial.print(f, 2);
    Serial.print(F(","));
    Serial.println(fft_vReal[k], 3);
  }
  Serial.println(F("=== FIN ESPECTRO ==="));
}

// Quitar DC: solo las primeras CHIRP_SKIP muestras (ventana sin eco posible)
void quitar_dc(const float *captura_f, uint16_t n, float &dc_out) {
  uint32_t suma = 0;
  uint16_t k = (CHIRP_SKIP < n) ? CHIRP_SKIP : n;
  for (uint16_t i = 0; i < k; i++) suma += (uint32_t)captura_f[i];
  dc_out = (float)suma / (float)k;
}

// Correlacion single-pass con heuristica anti-acople y closest_object
static const float RATIO_ACOPLE = 0.50f;
static const uint16_t SEPARACION_LAGS = N_CHIRP / 2;

float correlacion_tiempo(const float *captura_c, uint16_t n_captura,
                         uint16_t &mejor_lag_out, float &mejor_val_out,
                         float &max_val_out, uint16_t &max_lag_out) {
  // Quitar DC del template (para correlacion AC-only)
  float chirp_media = 0.0f;
  for (uint16_t i = 0; i < N_CHIRP; i++) chirp_media += chirp_template[i];
  chirp_media /= (float)N_CHIRP;
  // Tamano fijo = N_CHIRP_MAX (compiletime const) para evitar VLA en stack
  float chirp_norm[N_CHIRP_MAX];
  for (uint16_t i = 0; i < N_CHIRP; i++) {
    chirp_norm[i] = (float)chirp_template[i] - chirp_media;
  }

  const uint16_t LAG_MIN = CHIRP_SKIP;
  const uint16_t LAG_MAX = n_captura - N_CHIRP;
  const uint16_t MID_LAG = (LAG_MIN + LAG_MAX) / 2;
  const uint16_t SEP = N_CHIRP / 2;

  float    max_val = -1e30f;
  uint16_t max_lag = 0;
  float    second_val = -1e30f;
  uint16_t second_lag = 0;

  // Local max tracking para closest_object
  static float local_max_val_buf[N_CAPTURE_MAX];
  static uint16_t local_max_lag_buf[N_CAPTURE_MAX];
  uint16_t local_max_count = 0;

  for (uint16_t lag = LAG_MIN; lag < LAG_MAX; lag++) {
    float acc = 0.0f;
    for (uint16_t j = 0; j < N_CHIRP; j++) {
      acc += captura_c[lag + j] * chirp_norm[j];
    }
    if (acc > max_val) {
      // Demote old max a second si esta lejos
      if ((uint16_t)(lag - max_lag) >= SEP ||
          (uint16_t)(max_lag - lag) >= SEP) {
        if (max_val > second_val) {
          second_val = max_val;
          second_lag = max_lag;
        }
      }
      max_val = acc;
      max_lag = lag;
    } else {
      // Candidato a second si esta lejos del max
      if ((uint16_t)(lag - max_lag) >= SEP ||
          (uint16_t)(max_lag - lag) >= SEP) {
        if (acc > second_val) {
          second_val = acc;
          second_lag = lag;
        }
      }
    }
    // Local max: lag con valor >= 30% del max global
    if (acc > 0.30f * max_val) {
      bool es_nuevo = true;
      if (local_max_count > 0) {
        uint16_t ultimo_lag = local_max_lag_buf[local_max_count - 1];
        if ((uint16_t)(lag - ultimo_lag) < SEP) {
          if (acc > local_max_val_buf[local_max_count - 1]) {
            local_max_val_buf[local_max_count - 1] = acc;
            local_max_lag_buf[local_max_count - 1] = lag;
          }
          es_nuevo = false;
        }
      }
      if (es_nuevo && local_max_count < N_CAPTURE_MAX) {
        local_max_val_buf[local_max_count] = acc;
        local_max_lag_buf[local_max_count] = lag;
        local_max_count++;
      }
    }
    if ((lag & 0x0F) == 0) yield();
  }

  max_val_out = max_val;
  max_lag_out = max_lag;

  // Closest object: primer pico local con lag >= ZONA_ACOPLE_FIN y val >= 50% max
  const uint16_t ZONA_ACOPLE_FIN = CHIRP_SKIP + 5;
  const float RATIO_CLOSEST = 0.50f;
  uint16_t closest_lag = max_lag;
  float    closest_val = max_val;
  bool     closest_encontrado = false;
  for (uint16_t i = 0; i < local_max_count; i++) {
    uint16_t lag = local_max_lag_buf[i];
    float    val = local_max_val_buf[i];
    if (lag >= ZONA_ACOPLE_FIN && val >= RATIO_CLOSEST * max_val) {
      closest_lag = lag;
      closest_val = val;
      closest_encontrado = true;
      break;  // local_max esta ordenado por lag
    }
  }

  if (closest_encontrado) {
    mejor_lag_out = closest_lag;
    mejor_val_out = closest_val;
  } else {
    // Fallback: heuristica v7 (second max si max esta en zona de acople)
    bool es_acople = (max_lag < MID_LAG) &&
                     (second_val >= RATIO_ACOPLE * max_val) &&
                     (second_val > 0.0f);
    if (es_acople) {
      mejor_lag_out = second_lag;
      mejor_val_out = second_val;
    } else {
      mejor_lag_out = max_lag;
      mejor_val_out = max_val;
    }
  }

  return (float)mejor_lag_out / fs_calibrado;
}

// Filtro: mediana movil + reset por cambio brusco
static float mediana_buffer(const float *b, uint8_t n) {
  float tmp[N_FILTRO];
  for (uint8_t i = 0; i < n; i++) tmp[i] = b[i];
  // insertion sort
  for (uint8_t i = 1; i < n; i++) {
    float key = tmp[i];
    uint8_t j = i;
    while (j > 0 && tmp[j - 1] > key) {
      tmp[j] = tmp[j - 1];
      j--;
    }
    tmp[j] = key;
  }
  return tmp[n / 2];
}

float filtrar_medicion(float d_cm_raw, float magnitud_pico) {
  // Umbral adaptativo: max(MIN, factor * ruido_piso * 60)
  float correlacion_ruido = ruido_piso_mV * 60.0f;
  float umbral_efectivo = UMBRAL_PICO_MIN;
  if (UMBRAL_PICO_FACTOR_NOISE * correlacion_ruido > umbral_efectivo) {
    umbral_efectivo = UMBRAL_PICO_FACTOR_NOISE * correlacion_ruido;
  }
  if (magnitud_pico < umbral_efectivo) return NAN;

  // Fuera de rango -> glitch, descartar
  if (d_cm_raw < DISTANCIA_MIN_CM || d_cm_raw > DISTANCIA_MAX_CM) return NAN;

  // Outlier vs mediana previa
  if (buffer_count >= 3) {
    float mediana_prev = mediana_buffer(buffer_d, buffer_count);
    if (mediana_prev > 0.0f) {
      float desv = fabsf(d_cm_raw - mediana_prev) / mediana_prev;
      if (desv > 0.5f) {
        return NAN;
      }
    }
  }

  buffer_d[buffer_idx] = d_cm_raw;
  buffer_idx = (buffer_idx + 1) % N_FILTRO;
  if (buffer_count < N_FILTRO) buffer_count++;

  return mediana_buffer(buffer_d, buffer_count);
}

// OLED
void display_init() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("ERROR: OLED no responde en 0x3C"));
    return;
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println(F("Radar acustico v7.3"));
  oled.println(F("MAX4466 + 4V"));
  oled.println(F("Iniciando..."));
  oled.display();
}

void display_distancia(float d_cm_filtrada, bool valida) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println(F("Radar acustico v7.3"));
  oled.println();

  oled.setTextSize(2);
  if (valida) {
    oled.print(F("d="));
    oled.print(d_cm_filtrada, 1);
    oled.println(F(" cm"));
  } else {
    oled.print(F("--.-- cm"));
  }

  oled.setTextSize(1);
  oled.setCursor(0, 50);
  if (valida) {
    oled.print(F("tau="));
    oled.print(d_cm_filtrada * 2.0f / (V_SONIDO * 100.0f) * 1000.0f, 2);
    oled.println(F(" ms"));
  } else {
    oled.println(F("Sin objeto"));
  }
  oled.display();
}

// Calibracion de fs + recalculo de N_CHIRP, N_CAPTURE y biquad
void calibrar_fs() {
  Serial.println(F("=== CALIBRACION DE FS ==="));
  Serial.println(F("Midiendo tiempo de 200 ciclos analogRead (sin delay)..."));
  const int N = 200;
  uint32_t t0 = micros();
  for (int i = 0; i < N; i++) {
    volatile uint16_t dummy = analogRead(PIN_MIC_ADC);
    (void)dummy;
  }
  uint32_t t1 = micros();
  float periodo_us = (float)(t1 - t0) / (float)N;
  fs_calibrado = 1.0e6f / periodo_us;
  Serial.print  (F("  periodo medido = "));
  Serial.print  (periodo_us, 1);
  Serial.print  (F(" us | fs real = "));
  Serial.print  (fs_calibrado, 0);
  Serial.println(F(" Hz"));
  Serial.print  (F("  factor calibracion = "));
  Serial.print  ((float)FS_NOMINAL / fs_calibrado, 3);
  Serial.println(F(" (FS nominal / FS real)"));
  Serial.println();

  // Recalcular N_CHIRP y N_CAPTURE segun fs_calibrado
  N_CHIRP = (uint16_t)(fs_calibrado * CHIRP_DURATION_MS / 1000.0f + 0.5f);
  if (N_CHIRP < 8) N_CHIRP = 8;
  if (N_CHIRP > N_CHIRP_MAX) N_CHIRP = N_CHIRP_MAX;
  N_CAPTURE = (uint16_t)(fs_calibrado * CAPTURE_DURATION_MS / 1000.0f + 0.5f);
  if (N_CAPTURE < N_CHIRP + 16) N_CAPTURE = N_CHIRP + 16;
  if (N_CAPTURE > N_CAPTURE_MAX) N_CAPTURE = N_CAPTURE_MAX;
  Serial.print(F("  N_CHIRP recalculado = ")); Serial.println(N_CHIRP);
  Serial.print(F("  N_CAPTURE recalculado = ")); Serial.println(N_CAPTURE);

  // Recalcular coeficientes del biquad para fs_calibrado
  compute_biquad_coefs(fs_calibrado, 2500.0f, 0.7f,
                       bq_b0, bq_b1, bq_b2, bq_a1, bq_a2);
  Serial.print(F("  biquad fs=")); Serial.print(fs_calibrado, 0);
  Serial.print(F(" f0=2500 Q=0.7 -> b0=")); Serial.print(bq_b0, 4);
  Serial.print(F(" b1=")); Serial.print(bq_b1, 4);
  Serial.print(F(" b2=")); Serial.print(bq_b2, 4);
  Serial.print(F(" a1=")); Serial.print(bq_a1, 4);
  Serial.print(F(" a2=")); Serial.println(bq_a2, 4);
  Serial.println();
}

// Diagnostico inicial
void diagnostico_inicial() {
  Serial.println(F("=== DIAGNOSTICO INICIAL (MAX4466 en P34) ==="));

  const int N = 2000;
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
  float media    = (float)suma / N;
  float var      = ((float)suma_sq / N) - media * media;
  if (var < 0) var = 0;
  float rms_adc  = sqrtf(var);
  float rms_mV   = rms_adc * 3.3f / 4095.0f * 1000.0f;
  ruido_piso_mV  = rms_mV;

  Serial.print(F("  DC offset (media)  = ")); Serial.println(media, 0);
  Serial.print(F("  min / max          = ")); Serial.print(vmin);
  Serial.print(F(" / ")); Serial.println(vmax);
  Serial.print(F("  RMS ruido          = ")); Serial.print(rms_mV, 1);
  Serial.println(F(" mV"));
  Serial.print(F("  Umbral adaptativo  = "));
  float correlacion_ruido = rms_mV * 60.0f;
  float umbral = UMBRAL_PICO_MIN;
  if (UMBRAL_PICO_FACTOR_NOISE * correlacion_ruido > umbral) {
    umbral = UMBRAL_PICO_FACTOR_NOISE * correlacion_ruido;
  }
  Serial.println(umbral, 0);

  if (media < 100.0f || media > 4000.0f) {
    Serial.println(F("  >>> PROBLEMA: DC fuera de rango."));
  } else if (media > 1500.0f && media < 2500.0f) {
    Serial.println(F("  >>> OK: DC offset normal (~2048 esperado)."));
  } else {
    Serial.println(F("  >>> REVISAR: DC inesperado."));
  }
  if (rms_mV < 5.0f) {
    Serial.println(F("  >>> OK: silencio total."));
  } else if (rms_mV < 50.0f) {
    Serial.println(F("  >>> OK: ruido bajo."));
  } else {
    Serial.println(F("  >>> INFO: ruido ambiente alto (ventilador? voz?)."));
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Reconfigurar WDT (TWDT ya esta inicializado por FreeRTOS)
  const esp_task_wdt_config_t twdt_config = {
    .timeout_ms     = 60000,
    .idle_core_mask = 0,
    .trigger_panic  = false
  };
  esp_task_wdt_reconfigure(&twdt_config);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC_ADC, ADC_11db);

  calibrar_fs();

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("  Radar acustico v7.3 — MAX4466 + buzzer con +4V"));
  Serial.println(F("================================================"));
  Serial.print  (F("Chirp: "));
  Serial.print  ((int)CHIRP_F0_HZ); Serial.print('-');
  Serial.print  ((int)CHIRP_F1_HZ);
  Serial.print  (F(" Hz, "));
  Serial.print  (CHIRP_DURATION_MS);
  Serial.print  (F(" ms (N=")); Serial.print(N_CHIRP); Serial.println(F(")"));
  Serial.print  (F("Captura: "));
  Serial.print  (CAPTURE_DURATION_MS);
  Serial.print  (F(" ms (N=")); Serial.print(N_CAPTURE); Serial.println(F(")"));
  Serial.print  (F("fs nominal: "));      Serial.print((int)FS_NOMINAL);
  Serial.print  (F(" Hz | fs real: "));   Serial.print(fs_calibrado, 0);
  Serial.println(F(" Hz"));
  Serial.print  (F("Rango valido: "));
  Serial.print  (DISTANCIA_MIN_CM); Serial.print('-');
  Serial.print  (DISTANCIA_MAX_CM); Serial.println(F(" cm"));
  Serial.print  (F("Umbral pico MIN: ")); Serial.println(UMBRAL_PICO_MIN);
  Serial.print  (F("CHIRP_SKIP: "));      Serial.println(CHIRP_SKIP);
  Serial.println(F("------------------------------------------------"));

  generar_chirp();
  display_init();
  diagnostico_inicial();

  Serial.println(F("Loop activo — cada ~50 ms una medicion:"));
  Serial.println(F("  lag=<m> pico=<v> crudo=<d>cm | filt=<d>cm"));
  Serial.println(F("  Envia 'f' por Serial para ver el espectro FFT completo."));
  Serial.println();
}

void loop() {
  emitir_chirp();
  capturar_adc();

  // Prefiltrar pasabanda antes de quitar DC
  filtrar_banda(captura, captura_filtrada, N_CAPTURE);

  // Quitar DC solo de la ventana sin eco (primeras CHIRP_SKIP)
  quitar_dc(captura_filtrada, N_CAPTURE, captura_dc);
  yield();

  for (uint16_t i = 0; i < N_CAPTURE; i++) {
    captura_centrada[i] = captura_filtrada[i] - captura_dc;
  }

  uint16_t mejor_lag = 0, max_lag = 0;
  float    mejor_val = 0.0f, max_val = 0.0f;
  float    tau = correlacion_tiempo(captura_centrada, N_CAPTURE,
                                    mejor_lag, mejor_val,
                                    max_val, max_lag);

  float d_cm_raw = V_SONIDO * tau * 100.0f / 2.0f;
  float d_cm_filtrada = filtrar_medicion(d_cm_raw, mejor_val);
  bool  valida = !isnan(d_cm_filtrada);

  #if DEBUG_CORR
  static uint32_t dbg_count = 0;
  if ((dbg_count++ % 20) == 0) {
    Serial.print(F("[DEBUG] max_lag=")); Serial.print(max_lag);
    Serial.print(F(" max_val=")); Serial.print(max_val, 0);
    Serial.print(F(" sel_lag=")); Serial.print(mejor_lag);
    Serial.print(F(" sel_val=")); Serial.print(mejor_val, 0);
    Serial.print(F(" raw=")); Serial.print(d_cm_raw, 1);
    Serial.println(F(" cm"));
  }
  #endif

  Serial.print(F("lag="));     Serial.print(mejor_lag);
  Serial.print(F(" pico="));   Serial.print(mejor_val, 0);
  Serial.print(F(" crudo="));  Serial.print(d_cm_raw, 1);
  Serial.print(F("cm | filt="));
  if (valida) Serial.print(d_cm_filtrada, 1);
  else        Serial.print(F("---"));
  Serial.println(F(" cm"));

  display_distancia(d_cm_filtrada, valida);

  // FFT periodico
  static uint8_t fft_counter = 0;
  if (++fft_counter >= FFT_EVERY_N_LOOPS) {
    fft_counter = 0;
    compute_and_print_fft_peak();
  }

  // Comando 'f' o 'F' -> espectro completo
  if (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == FFT_TRIGGER_CMD || c == (char)toupper(FFT_TRIGGER_CMD)) {
      print_full_spectrum();
    }
  }

  vTaskDelay(50 / portTICK_PERIOD_MS);
}