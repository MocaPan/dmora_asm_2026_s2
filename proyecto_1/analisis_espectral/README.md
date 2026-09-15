# Análisis espectral

Experimentos con la Transformada Discreta de Fourier (DFT) y la Transformada
Rápida de Fourier (FFT). Corresponde a la sección 2.2 del enunciado del
proyecto.

## Contenido

- Implementación de la DFT por definición.
- Implementación de la FFT (algoritmo radix-2 por decimación en frecuencia).
- Comparación de tiempos de ejecución entre DFT y FFT para varios N.
- Representación de magnitud y fase de señales de prueba (senoidales, suma
  de senoidales, señal cuadrada, chirp).

## Organización

- `src/` — scripts de Python.
- `imagenes/` — figuras generadas (referenciadas en el paper).
- `README.md` — este archivo.

## Cómo reproducir

```bash
python src/dft.py
python src/fft.py
python src/comparacion_tiempos.py
python src/magnitud_fase.py
```
