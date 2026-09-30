"""
Recopilación de imágenes con los efectos principales del análisis
espectral. Genera una sola figura 2x2 con cuatro paneles:

    1. Leakage espectral (senoidal no múltiplo exacto de Δf).
    2. Ventana rectangular vs Hann.
    3. Aliasing (frecuencia arriba de Nyquist).
    4. Resolución espectral (dos picos cercanos, N chico vs grande).
"""

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from fft import fft


def magnitud_db(X, eps=1e-12):
    """Magnitud en decibelios, con epsilon para evitar log(0)."""
    return 20 * np.log10(np.abs(X) + eps)


def leakage(fs, N, f_señal):
    """Devuelve (magnitudes_dB, freqs_Hz) para una senoidal a f_señal."""
    t = np.arange(N) / fs
    x = np.sin(2 * np.pi * f_señal * t)
    X = fft(x)
    frecs = np.arange(N // 2) * fs / N
    return magnitud_db(X[: N // 2]), frecs


def con_ventana(senal, ventana):
    return senal * ventana


if __name__ == "__main__":
    fs = 1000.0

    fig, axes = plt.subplots(2, 2, figsize=(11, 7))

    # Panel 1: Leakage. Senoidal de 50.5 Hz con N = 1000 (Δf = 1 Hz).
    # El pico debería estar limpio en k = 50 si f = 50.0, pero al no ser
    # múltiplo exacto de Δf, la energía se derrama.
    N = 1000
    mag, frecs = leakage(fs, N, 50.5)
    axes[0, 0].plot(frecs, mag)
    axes[0, 0].set_xlim(30, 70)
    axes[0, 0].set_ylim(-20, 60)
    axes[0, 0].set_title("1. Leakage espectral")
    axes[0, 0].set_xlabel("Frecuencia (Hz)")
    axes[0, 0].set_ylabel("Magnitud (dB)")
    axes[0, 0].grid(True, alpha=0.3)

    # Panel 2: Ventana rectangular vs Hann sobre la misma senoidal.
    N = 1000
    t = np.arange(N) / fs
    x = np.sin(2 * np.pi * 50.5 * t)
    w_hann = np.hanning(N)
    X_rect = fft(x)
    X_hann = fft(x * w_hann)
    frecs = np.arange(N // 2) * fs / N
    axes[0, 1].plot(frecs, magnitud_db(X_rect[: N // 2]),
                    label="Ventana rectangular", alpha=0.7)
    axes[0, 1].plot(frecs, magnitud_db(X_hann[: N // 2]),
                    label="Ventana Hann", alpha=0.7)
    axes[0, 1].set_xlim(30, 70)
    axes[0, 1].set_ylim(-40, 60)
    axes[0, 1].set_title("2. Ventana rectangular vs Hann")
    axes[0, 1].set_xlabel("Frecuencia (Hz)")
    axes[0, 1].set_ylabel("Magnitud (dB)")
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)

    # Panel 3: Aliasing. Senoidal a 600 Hz con fs = 1000 Hz.
    # Nyquist = 500 Hz, así que 600 Hz aparece como 400 Hz (espejo).
    N = 1000
    t = np.arange(N) / fs
    x = np.sin(2 * np.pi * 600 * t)
    X = fft(x)
    frecs = np.arange(N // 2) * fs / N
    axes[1, 0].plot(frecs, magnitud_db(X[: N // 2]))
    axes[1, 0].set_xlim(0, 500)
    axes[1, 0].set_ylim(-20, 60)
    axes[1, 0].axvline(400, color="red", linestyle="--", alpha=0.6,
                       label="Espejo: 1000 - 600 = 400 Hz")
    axes[1, 0].set_title("3. Aliasing: señal de 600 Hz a fs = 1000 Hz")
    axes[1, 0].set_xlabel("Frecuencia (Hz)")
    axes[1, 0].set_ylabel("Magnitud (dB)")
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)

    # Panel 4: Resolución espectral. Dos senoidales a 50 y 55 Hz.
    # Con N chico (Δf grande) no se distinguen los picos.
    # Con N grande (Δf chica) se separan.
    for N, ax_label in [(64, "N = 64 (Δf = 15.6 Hz)"),
                        (1024, "N = 1024 (Δf = 0.98 Hz)")]:
        t = np.arange(N) / fs
        x = np.sin(2 * np.pi * 50 * t) + np.sin(2 * np.pi * 55 * t)
        X = fft(x)
        frecs = np.arange(N // 2) * fs / N
        if N == 64:
            ax = axes[1, 1]
            ax.plot(frecs, magnitud_db(X[: N // 2]), label=ax_label)
        else:
            ax = axes[1, 1]
            ax.plot(frecs, magnitud_db(X[: N // 2]), label=ax_label,
                    linestyle="--")
    axes[1, 1].set_xlim(30, 80)
    axes[1, 1].set_ylim(-20, 60)
    axes[1, 1].set_title("4. Resolución espectral: 50 y 55 Hz")
    axes[1, 1].set_xlabel("Frecuencia (Hz)")
    axes[1, 1].set_ylabel("Magnitud (dB)")
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)

    plt.tight_layout()
    ruta = "../imagenes/efectos_principales.png"
    plt.savefig(ruta, dpi=150)
    print(f"Imagen guardada en {ruta}")
