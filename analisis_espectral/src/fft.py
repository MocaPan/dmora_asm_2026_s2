"""
FFT radix-2 por decimación en frecuencia.

Implementación recursiva del algoritmo de Cooley-Tukey. Solo funciona
cuando N es potencia de 2. Si x tiene largo distinto, se rellena con
ceros hasta la próxima potencia de 2.

Se parte la señal en índices pares e impares y
combinamos dos DFTs de tamaño N/2 con un factor de fase.
Repetimos recursivamente hasta llegar a DFTs de tamaño 1.

Convención: misma que dft.py. La salida X[k] corresponde a la
frecuencia f_k = k * f_s / N.
"""

import numpy as np


def fft(x):
    """Calcula la FFT radix-2 de la señal x (recursiva, sin numpy.fft).

    Parámetros
    ----------
    x : array_like
        Señal de entrada.

    Retorna
    -------
    X : numpy.ndarray
        FFT de x, vector de complejos.
    """
    x = np.asarray(x, dtype=complex)
    N = len(x)

    # Caso base: DFT de un solo elemento es el elemento mismo.
    if N == 1:
        return x

    # Si N no es potencia de 2, rellenamos con ceros hasta la próxima.
    if N & (N - 1) != 0:
        N_potencia = 1
        while N_potencia < N:
            N_potencia *= 2
        x = np.concatenate([x, np.zeros(N_potencia - N, dtype=complex)])
        N = N_potencia

    # Partimos en índices pares e impares.
    x_par = x[0::2]
    x_impar = x[1::2]

    # FFT recursiva de cada mitad.
    X_par = fft(x_par)
    X_impar = fft(x_impar)

    # Combinamos las dos mitades usando los twiddle factors.
    # W[k] = exp(-j 2 pi k / N) para k = 0, ..., N/2 - 1.
    k = np.arange(N // 2)
    W = np.exp(-1j * 2 * np.pi * k / N)

    X = np.empty(N, dtype=complex)
    X[: N // 2] = X_par + W * X_impar
    X[N // 2 :] = X_par - W * X_impar

    return X


def ifft(X):
    """Calcula la FFT inversa usando la identidad ifft(X) = conj(fft(conj(X))) / N.

    Es equivalente a una FFT recursiva con factor conjugado
    """
    X = np.asarray(X, dtype=complex)
    N = len(X)
    if N & (N - 1) != 0:
        raise ValueError("La FFT inversa requiere largo potencia de 2.")
    return np.conj(fft(np.conj(X))) / N


if __name__ == "__main__":
    # Misma prueba que dft.py, pero con N potencia de 2 para que la FFT
    # no tenga que rellenar con ceros.
    fs = 1024.0
    N = 1024
    t = np.arange(N) / fs
    f_señal = 50.0
    x = np.sin(2 * np.pi * f_señal * t)

    X = fft(x)
    mag = np.abs(X[: N // 2])
    k_pico = int(np.argmax(mag))
    f_pico = k_pico * fs / N

    print(f"FFT: pico en k = {k_pico}, frecuencia {f_pico} Hz")
    print(f"Magnitud en el pico: {mag[k_pico]:.2f}")

    # Round-trip.
    x_recuperada = ifft(X)
    error = np.max(np.abs(x_recuperada - x))
    print(f"Error round-trip FFT/IFFT: {error:.2e}")
