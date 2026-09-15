"""
DFT por definición.

Implementación de la Transformada Discreta de Fourier, basada
directamente en la fórmula:

    X[k] = sum_{n=0}^{N-1} x[n] * exp(-j 2 pi k n / N)

Convención:
    - x es un numpy array de N muestras reales.
    - La salida X es un array de N números complejos.
    - X[k] corresponde a la frecuencia f_k = k * f_s / N.
"""

import numpy as np


def dft(x):
    """Calcula la DFT de la señal x usando la fórmula directa.

    Parámetros
    ----------
    x : array_like
        Señal de entrada, de largo N.

    Retorna
    -------
    X : numpy.ndarray
        DFT de x, vector de N números complejos.
    """
    x = np.asarray(x, dtype=complex)
    N = len(x)
    # Precalculo de los índices n y k. n es columna, k es fila.
    n = np.arange(N)
    k = np.arange(N).reshape(-1, 1)
    # Matriz de factores: W[k, n] = exp(-j 2 pi k n / N).
    # Es una matriz de N x N.
    W = np.exp(-1j * 2 * np.pi * k * n / N)
    # DFT: producto matricial. Cada fila k es X[k].
    X = W @ x
    return X


def idft(X):
    """Calcula la DFT inversa. Sirve como verificación de la implementación.

    Parámetros
    ----------
    X : array_like
        Vector de N números complejos en el dominio de la frecuencia.

    Retorna
    -------
    x : numpy.ndarray
        Señal recuperada en el dominio del tiempo.
    """
    X = np.asarray(X, dtype=complex)
    N = len(X)
    n = np.arange(N)
    k = np.arange(N).reshape(-1, 1)
    W = np.exp(1j * 2 * np.pi * k * n / N)
    x = (W @ X) / N
    return x


if __name__ == "__main__":
    fs = 1000.0
    N = 1000
    t = np.arange(N) / fs
    f_señal = 50.0
    x = np.sin(2 * np.pi * f_señal * t)

    X = dft(x)

    mag = np.abs(X[: N // 2])

    k_pico = int(np.argmax(mag))
    f_pico = k_pico * fs / N

    print(f"Señal: seno de {f_señal} Hz a fs = {fs} Hz, N = {N}")
    print(f"Pico de magnitud en k = {k_pico}, frecuencia estimada {f_pico} Hz")
    print(f"Magnitud en el pico: {mag[k_pico]:.2f}")
    print(f"Magnitud en k = 0 (DC): {mag[0]:.6f}")

    # Verificación con la IDFT: si hacemos DFT y después IDFT, recuperamos x.
    x_recuperada = idft(X)
    error = np.max(np.abs(x_recuperada - x))
    print(f"Error máximo de la IDFT (round-trip): {error:.2e}")
