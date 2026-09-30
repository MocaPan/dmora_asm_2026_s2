# Microcontrolador — Radar acústico (ESP32D + MAX4466)

Implementación del radar acústico en ESP32 (NodeMCU-32S / ESP32-DevKitC).
Corresponde a la **etapa 4** del enunciado: generación, adquisición y
procesamiento en microcontrolador (sección 2.4 del PDF
`Proyectos_ASM_Proyecto_Individual_2026_S2.pdf`).

> **Nota sobre versiones históricas**: este README describe la versión
> actual (`radar/`, hardware MAX4466 + buzzer con +4V). Las carpetas
> `test_basico/`, `test_dac_adc/`, etc. son sketches de prototipado de
> iteraciones previas (LM358 + mic electret) y se conservan solo como
> bitácora. El sketch funcional vigente está en **`radar/radar.ino`** (v7.3).

## Estructura

```
microcontrolador/
├── README.md              ← este archivo
├── radar/                 ← versión vigente (MAX4466, fs_calibrado en runtime)
│   ├── radar.ino          ← sketch principal (cargar al ESP32D)
│   ├── tests/
│   │   └── tests.ino      ← sub-sketch de tests automáticos
│   ├── sim_radar.py       ← simulador offline del pipeline DSP
│   ├── validate_v7.py     ← batería de validación del algoritmo v7.3
│   ├── run_ci.ps1         ← CI: compila ambos sketches + corre simuladores
│   └── README.md          ← documentación específica de v7.3
├── test_basico/           ← (legacy) blink + DAC + ADC, sin radar
├── test_dac_adc/          ← (legacy) chirp por DAC, captura por ADC
├── test_sweep/            ← (legacy) barrido de frecuencia del buzzer
├── test_buzzer/           ← (legacy) prueba de buzzer con varios tonos
├── test_microfono/        ← (legacy) prueba del MAX4466 con tonos del buzzer
├── test_multi_adc/        ← (legacy) comparación de pines ADC
└── test_adc_nativo/       ← (legacy) analogRead sin delay (midió fs_real=10957)
```

## Hardware asumido (vigente)

| Componente | Conexión |
|---|---|
| ESP32 DevKitC / NodeMCU-32S | USB a PC |
| MAX4466 (amp de micrófono) | VCC → 3V3, GND → GND, AO → P34 (ADC1_CH6) |
| Micrófono electret | (+) → entrada MAX4466, (−) → GND (en el módulo) |
| Buzzer piezo pasivo | (−) → GND; (+) → nodo **audio** |
| Batería externa 4 V | (+) → nodo **audio**; (−) → GND común |
| ESP32 DAC1 (P25) | → nodo **audio** (en serie con el (+) del buzzer, vía resistencia ~100 Ω) |
| OLED SSD1306 | VCC → 3V3, GND → GND, SDA → P21, SCL → P22, addr 0x3C |

**Diagrama de polarización del buzzer**:

```
       ┌───────────── (+) Buzzer piezo
       │
   BAT 4V ─┤
       │   ┌───────── (−) P25 (DAC1 del ESP32, oscila 0..3.3 V)
       └─ R~100Ω ──┤
                   │
              GND común
```

Con esta topología, el piezo ve `4V + DAC_output` superpuestos. El
chirp del DAC (0..3.3 V) modula la polarización de 4 V del piezo,
produciendo presión acústica proporcional al DAC. La batería
proporciona el bias DC que evita que el piezo opere en la zona no
lineal de su curva voltaje-desplazamiento.

> Si agregás un capacitor de 100 µF en serie entre el DAC (P25) y el
> buzzer, bloqueás el DC de 4 V y protegés el pin del ESP32. Sin
> capacitor, el DAC ve la 4 V en su pin de salida, lo que está fuera
> del rango 0–3.3 V. **Recomendado** para hardware definitivo.

## Instalación

### Arduino IDE
1. Instalar Arduino IDE 2.x desde https://www.arduino.cc/en/software
2. Agregar URL de board: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   (Archivo → Preferencias → URLs adicionales de gestor de tarjetas)
3. Instalar placa: Herramientas → Placa → Gestor de tarjetas → buscar "esp32"
   → instalar "esp32 by Espressif Systems"
4. Instalar bibliotecas (Herramientas → Administrar bibliotecas):
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
   - `arduinoFFT` por Kosme — **usar versión `1.6.2`**

### Configuración de placa
- Placa: "ESP32 Dev Module" (o "NodeMCU-32S" si aparece)
- Upload Speed: 921600
- CPU Frequency: 240 MHz
- Flash Size: 4 MB (32 Mb)
- Port: el COM que aparezca al conectar el USB

## Uso del radar vigente

1. Conectar el ESP32 por USB.
2. Cargar `radar/radar.ino`.
3. Abrir Monitor Serial a **115200 baud**.
4. En el boot aparece:

   ```
   === CALIBRACION DE FS ===
     periodo medido = 91.3 us | fs real = 10957 Hz
     factor calibracion = 1.825 (FS nominal / FS real)

     N_CHIRP recalculado = 43
     N_CAPTURE recalculado = 131

   ================================================
     Radar acustico v7.3 — MAX4466 + buzzer con +4V
   ================================================
   ```

5. Después del diagnóstico inicial, cada 50 ms aparece:

   ```
   lag=<m> pico=<v> crudo=<d>cm | filt=<d>cm
   ```

   - `lag` — posición del pico de correlación (en muestras).
   - `pico` — magnitud del pico (unidades de correlación).
   - `crudo` — distancia sin filtrar.
   - `filt` — distancia después de mediana móvil de 3.
   - `---` — lectura descartada (umbral / fuera de rango).

6. **Comando `f` por Serial**: imprime el espectro FFT completo en
   formato CSV (`bin,frecuencia_Hz,magnitud`) listo para graficar.
   Ver `radar/README.md` sección "Análisis FFT" para el detalle.

7. **OLED**: muestra `d=XX.X cm` y `tau=XX.XX ms` (o "Sin objeto").

## Diseño del radar vigente

### Señal transmitida: chirp lineal 1–4 kHz, 4 ms

Elegimos chirp lineal porque:
- Tiene autocorrelación con un pico angosto (banda ancha → resolución fina en τ).
- La FFT del chirp es plana en [1, 4] kHz → no sesga el espectro del eco.
- Banda 1–4 kHz cae en la zona donde el buzzer piezo resuena y el MAX4466
  tiene SNR aceptable.

Parámetros clave:
- **fs_calibrado ≈ 11 kHz** (medida en `setup()` con 200 ciclos de
  `analogRead` consecutivos). En el ESP32 del usuario da 10957 Hz.
- **Duración 4 ms** → 43 muestras a fs=10957 → N_CHIRP=43.
- **Captura 12 ms** → 131 muestras → N_CAPTURE=131.
- **Frecuencias 1–4 kHz**: dentro de la zona de mejor sensibilidad
  del buzzer piezo y del MAX4466.

### Generación con DAC1 (P25)

DAC1 es de 8 bits (0–255). El DAC no soporta DMA directo sin ESP-IDF,
así que emitimos con un bucle bloqueante:

```cpp
emit_period_us = (1e6 / fs_calibrado) - 20;   // ~71 us @ fs=10957
for (i = 0; i < N_CHIRP; i++) {
    dacWrite(P25, chirp[i]);
    delayMicroseconds(emit_period_us);
}
dacWrite(P25, 128);   // vuelve a DC = 128
```

Cada muestra del chirp tiene offset DC = 128 para que el DAC oscile
entre 0 y 3.3 V. Al final se aplica una ventana Tukey (cola de 1 ms)
para suavizar el corte y reducir lóbulos laterales de la correlación.

### Captura con ADC1_CH6 (P34)

`analogRead()` en ESP32D toma ~91 µs (medido en runtime), lo que define
fs_calibrado ≈ 11 kHz:

```cpp
for (i = 0; i < N_CAPTURE; i++) {
    captura[i] = analogRead(P34);   // ~91 us natural
}
```

Sin `delayMicroseconds` artificial: la fs del ADC es la que el hardware
permite. Esto se calibra al inicio (`calibrar_fs()`) y se usa
consistentemente para el template, el emit y la captura.

### Pipeline de procesamiento (v7.3)

```
captura ADC
   │
   ▼
filtrar_banda  (biquad RBJ bandpass 2.5 kHz Q=0.7, 2 etapas en cascada)
   │
   ▼
quitar_dc       (solo primeras CHIRP_SKIP=30 muestras, ventana sin eco)
   │
   ▼
captura_centrada
   │
   ├──► correlacion_tiempo  (single-pass, max + second + local_max tracking)
   │       │
   │       ▼
   │    closest_object  (F16/F17: primer pico local >= 50% max con lag >= 35)
   │       │
   │       ▼
   │    τ = lag / fs_calibrado
   │    d = v_sonido · τ / 2
   │       │
   │       ▼
   │    filtrar_medicion  (mediana móvil N=3 + umbral adaptativo)
   │
   └──► compute_fft_peak   (cada 1 s, ítem 2.4.d)
           │
           ▼
        arduinoFFT 1.6.2, N_FFT=256, ventana Hann
           │
           ▼
        [FFT] pico=2450 Hz mag=12.3 (0 ms)
```

### Estimación de distancia

Ecuación monostática:
```
d = v_sonido · τ / 2
```
con `v_sonido = 343 m/s` (≈ 20 °C).

**Rango válido** con fs=10957 Hz y N_CAPTURE=131:
- τ_max = 12 ms
- d_max = 343 · 0.012 / 2 ≈ **2.06 m**
- En la práctica, el rango útil se reduce a **45–150 cm** por SNR.

**Resolución** (1 muestra = 1/10957 = 91 µs):
- Δd = 343 · 91e-6 / 2 ≈ **1.56 cm** entre muestras
- Sin interpolación del pico.

**Distancia mínima**: ~47 cm (limitada por `CHIRP_SKIP=30` y la zona
ciega del acople directo buzzer→mic).

## Validación contra la especificación

| Punto enunciado §2.4 | Implementación | Cumplido |
|---|---|---|
| (a) Generación de señal conocida | Chirp lineal 1–4 kHz, 4 ms, ventana Tukey | ✓ |
| (b) Reproducción por parlante | DAC1 + R~100 Ω + buzzer (con +4V bias) | ✓ |
| (c) Adquisición por ADC | ADC1_CH6 @ fs_calibrado, 12 ms | ✓ |
| **(d) Procesamiento mediante FFT** | **arduinoFFT 1.6.2, N_FFT=256, ventana Hann, peak cada 1 s + espectro completo bajo demanda (`f`)** | **✓** |
| (e) Detección de eco por correlación o convolución | Correlación TD single-pass + closest_object v7.3 | ✓ |
| (f) Estimación de τ | Pico de la correlación (lag / fs_calibrado) | ✓ |
| (g) Cálculo de distancia | `d = v·τ/2` con v=343 m/s | ✓ |
| (h) Visualización | Serial 115200 + OLED SSD1306 | ✓ |

## Cómo verificar que funciona

1. Compilar: `arduino-cli compile --warnings all --fqbn esp32:esp32:esp32 radar/`
2. Subir al ESP32D.
3. Abrir Monitor Serial a 115200 baud.
4. Verificar el boot:
   - `fs real = 10957 Hz` (o cercano, depende del chip)
   - `RMS ruido < 100 mV` (si es mayor, revisar conexiones)
   - `Umbral adaptativo = ...`
5. Poner un objeto a 50 cm al frente del buzzer/mic.
6. Verificar que aparece `lag=... crudo=50.x cm` cada 50 ms.
7. Enviar `f` por Serial para ver el espectro FFT: el pico debe estar
   entre 1 y 4 kHz (banda del chirp).

## Pendientes / Trabajo futuro

- [ ] Calibración fina con varias distancias (30, 50, 75, 100 cm) y
      tabla de corrección.
- [ ] Interpolación parabólica del pico de correlación (mejora
      resolución a < 1 cm).
- [ ] Compensación por temperatura (DS18B20) en `v_sonido`.
- [ ] Display OLED de la forma de onda en tiempo real (bonus).
- [ ] Reducir acople directo buzzer→mic con baffle + espuma
      open-cell ≥ 25 mm (Fase A del plan).