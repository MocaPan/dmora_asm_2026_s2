# Detección de ecos

Experimentos de detección de ecos por correlación, en dominio del tiempo y
en dominio de la frecuencia. Corresponde a la sección 2.3 del enunciado del
proyecto.

## Contenido

- Generación de una señal acústica conocida (chirp) y de ecos simulados con
  retardo y atenuación.
- Correlación directa (en tiempo) para estimar el retardo.
- Correlación vía FFT, usando la relación convolución ↔ multiplicación en
  frecuencia.
- Comparación entre ambas implementaciones.

## Organización

- `src/` — scripts de Python.
- `imagenes/` — figuras generadas.
- `README.md` — este archivo.

## Cómo reproducir

```bash
python src/generar_senal_y_ecos.py
python src/correlacion_directa.py
python src/correlacion_fft.py
python src/comparacion.py
```

## Resultado esperado

Para cada retardo simulado, el pico de la correlación debe ubicarse en
`τ = retardo_simulado / fs`, con error menor a una muestra si la señal
transmitida tiene buena autocorrelación.
