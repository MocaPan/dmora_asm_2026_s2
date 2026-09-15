"""
Compara el tiempo de ejecución de la DFT directa y la FFT radix-2.

Para varios tamaños de N (todos potencias de 2), mide cuánto tarda cada
uno sobre señales aleatorias. Imprime una tabla y guarda una gráfica
log-log.
"""

import time

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from dft import dft
from fft import fft


def medir_tiempo(funcion, x, repeticiones=3):
    """Mide el tiempo de ejecucion de funcion(x). Devuelve el mejor de
    varias corridas para reducir ruido."""
    tiempos = []
    for _ in range(repeticiones):
        t0 = time.perf_counter()
        funcion(x)
        t1 = time.perf_counter()
        tiempos.append(t1 - t0)
    return min(tiempos)


if __name__ == "__main__":
    tamaños = [64, 128, 256, 512, 1024, 2048, 4096]
    tiempos_dft = []
    tiempos_fft = []

    for N in tamaños:
        x = np.random.randn(N)
        t_dft = medir_tiempo(dft, x, repeticiones=2)
        t_fft = medir_tiempo(fft, x, repeticiones=5)
        tiempos_dft.append(t_dft)
        tiempos_fft.append(t_fft)
        speedup = t_dft / t_fft if t_fft > 0 else float("inf")
        print(f"N = {N:>5}: DFT = {t_dft*1000:>9.3f} ms, "
              f"FFT = {t_fft*1000:>9.3f} ms, speedup = {speedup:>7.1f}x")

    # Gráfica log-log.
    plt.figure(figsize=(8, 5))
    plt.loglog(tamaños, np.array(tiempos_dft) * 1000, "o-",
               label="DFT (O(N^2))")
    plt.loglog(tamaños, np.array(tiempos_fft) * 1000, "s-",
               label="FFT (O(N log N))")
    # Curvas de referencia teóricas.
    N_arr = np.array(tamaños, dtype=float)
    escala_dft = tiempos_dft[0] / (tamaños[0] ** 2) * (N_arr ** 2) * 1000
    escala_fft = tiempos_fft[0] / (tamaños[0] * np.log2(tamaños[0])) \
                 * (N_arr * np.log2(N_arr)) * 1000
    plt.loglog(tamaños, escala_dft, "--", alpha=0.4,
               label="O(N^2) de referencia")
    plt.loglog(tamaños, escala_fft, "--", alpha=0.4,
               label="O(N log N) de referencia")

    plt.xlabel("N (tamaño de la señal)")
    plt.ylabel("Tiempo (ms)")
    plt.title("Comparación de tiempos: DFT vs FFT")
    plt.legend()
    plt.grid(True, which="both", alpha=0.3)
    plt.tight_layout()

    ruta = "../imagenes/comparacion_tiempos.png"
    plt.savefig(ruta, dpi=150)
    print(f"\nGráfica guardada en {ruta}")
