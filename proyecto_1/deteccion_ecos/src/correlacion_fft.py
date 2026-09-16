import numpy as np
import matplotlib.pyplot as plt
import time

# Cargar señales
datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")
tx, rx, fs = datos["tx"], datos["rx"], datos["fs"]

inicio = time.time()

# Zero-padding: Para evitar alias por convolución circular, N >= len(rx) + len(tx) - 1
N = len(rx) + len(tx) - 1
# Optimización: Ajustar a la siguiente potencia de 2 mejora la velocidad de la FFT
N_fft = 2**int(np.ceil(np.log2(N)))

# Transformada al dominio de la frecuencia
TX_fft = np.fft.fft(tx, N_fft)
RX_fft = np.fft.fft(rx, N_fft)

# Correlación vía multiplicación en frecuencia: R_xy = IFFT(RX * conj(TX))
corr_freq = RX_fft * np.conj(TX_fft)
corr_fft_full = np.fft.ifft(corr_freq)

# Tomar la parte real y recortar al tamaño válido equivalente a la correlación en tiempo
corr_fft = np.real(corr_fft_full)[:len(rx) - len(tx) + 1]

fin = time.time()

tiempo_ejecucion = fin - inicio
# Ignorar el tiempo que dura la señal directa original para no detectarla como eco
muestras_zona_muerta = len(tx) 
corr_fft[:muestras_zona_muerta] = 0
muestras_retardo_estimado = np.argmax(corr_fft)
retardo_estimado = muestras_retardo_estimado / fs

print("--- Correlación vía FFT ---")
print(f"Retardo estimado: {retardo_estimado:.4f} s")
print(f"Tiempo de ejecución: {tiempo_ejecucion:.6f} s")

# Graficar
t_corr = np.arange(len(corr_fft)) / fs
plt.figure(figsize=(10, 4))
plt.plot(t_corr, corr_fft, color="orange")
plt.axvline(x=retardo_estimado, color="r", linestyle="--", label=f"Eco detectado en {retardo_estimado:.4f}s")
plt.xlabel("Retardo (s)")
plt.ylabel("Amplitud de Correlación")
plt.title("Detección de Eco - Correlación vía FFT")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("proyecto_1/deteccion_ecos/imagenes/correlacion_fft.png")