import numpy as np
import time

datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")
tx, rx, fs = datos["tx"], datos["rx"], datos["fs"]
retardo_real = datos["retardo_simulado"]

print(f"Retardo simulado real: {retardo_real:.4f} s\n")

# --- Método 1: Directo ---
t0 = time.time()
corr_dir = np.correlate(rx, tx, mode="valid")
t1 = time.time()
tiempo_dir = t1 - t0
# Ignorar el tiempo que dura la señal directa original para no detectarla como eco
muestras_zona_muerta = len(tx) 
corr_dir[:muestras_zona_muerta] = 0
retardo_dir = np.argmax(corr_dir) / fs

# --- Método 2: FFT ---
t0 = time.time()
N = len(rx) + len(tx) - 1
N_fft = 2**int(np.ceil(np.log2(N)))
corr_fft = np.fft.ifft(np.fft.fft(rx, N_fft) * np.conj(np.fft.fft(tx, N_fft)))
corr_fft = np.real(corr_fft)[:len(rx) - len(tx) + 1]
t1 = time.time()
tiempo_fft = t1 - t0
# Ignorar el tiempo que dura la señal directa original para no detectarla como eco
muestras_zona_muerta = len(tx) 
corr_fft[:muestras_zona_muerta] = 0
retardo_fft = np.argmax(corr_fft) / fs

# --- Despliegue de Resultados ---
print(f"{"MÉTODO":<10} | {"RETARDO (s)":<15} | {"ERROR (s)":<15} | {"TIEMPO (s)":<15}")
print("-" * 65)
print(f"{"Directo":<10} | {retardo_dir:<15.6f} | {abs(retardo_real - retardo_dir):<15.6f} | {tiempo_dir:<15.6f}")
print(f"{"Por FFT":<10} | {retardo_fft:<15.6f} | {abs(retardo_real - retardo_fft):<15.6f} | {tiempo_fft:<15.6f}")
print("-" * 65)
print(f"Rendimiento: La FFT fue {tiempo_dir / tiempo_fft:.2f} veces más rápida.")