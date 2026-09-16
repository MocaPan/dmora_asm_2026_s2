"""
Magnitud y fase de varias señales de prueba.

Genera tres señales (senoidal pura, suma de senoidales, cuadrada) y
grafica su magnitud (en dB) y fase vs frecuencia usando la FFT.
"""

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from fft import fft


def graficar_espectro(x, fs, titulo, ruta_salida):
    """Calcula la FFT de x, grafica magnitud (dB) y fase.

    Parámetros
    ----------
    x : array_like
        Señal en el tiempo.
    fs : float
        Frecuencia de muestreo.
    titulo : str
        Título para la figura.
    ruta_salida : str
        Dónde guardar la imagen PNG.
    """
    N = len(x)
    X = fft(x)
    frecs = np.arange(N // 2) * fs / N
    mag = np.abs(X[: N // 2])
    fase = np.angle(X[: N // 2])

    mag_db = 20 * np.log10(mag + 1e-12)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 6), sharex=True)
    ax1.plot(frecs, mag_db)
    ax1.set_ylabel("Magnitud (dB)")
    ax1.set_title(titulo)
    ax1.grid(True, alpha=0.3)

    ax2.plot(frecs, fase)
    ax2.set_xlabel("Frecuencia (Hz)")
    ax2.set_ylabel("Fase (rad)")
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(ruta_salida, dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    fs = 1000.0
    N = 1024
    t = np.arange(N) / fs

    # 1) Senoidal pura de 50 Hz. 
    x1 = np.sin(2 * np.pi * 50 * t)
    graficar_espectro(
        x1, fs,
        "Espectro de seno de 50 Hz (N = 1024, fs = 1000 Hz)",
        "../imagenes/magnitud_fase_senoidal.png",
    )

    # 2) Suma de senoidales: 50 Hz + 120 Hz. 
    x2 = np.sin(2 * np.pi * 50 * t) + 0.5 * np.sin(2 * np.pi * 120 * t)
    graficar_espectro(
        x2, fs,
        "Espectro de seno 50 Hz + 120 Hz",
        "../imagenes/magnitud_fase_suma.png",
    )

    # 3) Cuadrada de 100 Hz. 
    x3 = np.sign(np.sin(2 * np.pi * 100 * t))
    graficar_espectro(
        x3, fs,
        "Espectro de onda cuadrada de 100 Hz",
        "../imagenes/magnitud_fase_cuadrada.png",
    )

    print("Tres gráficas guardadas en ../imagenes/")
