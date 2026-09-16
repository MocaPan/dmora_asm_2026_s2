import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import chirp

# Parámetros 
fs = 44100  # Frecuencia de muestreo en Hz
t_dur = 0.05 # Duración del pulso, 50 ms
t = np.arange(0, t_dur, 1/fs)
retardo_simulado = 0.12  # Retardo del eco en segundos (120 ms)
atenuacion = 0.4

# Generar la señal conocida con un pulso de 1kHz a 5kHz
tx = chirp(t, f0=1000, f1=5000, t1=t_dur, method="linear")

# Configurar la señal recibida (directa + eco + ruido)
t_rx = np.arange(0, t_dur + 0.3, 1/fs)
rx = np.zeros_like(t_rx)

# Simular recepción de onda directa 
rx[:len(tx)] += tx * 0.8

# Incorporar el eco retardado
muestras_retardo = int(retardo_simulado * fs)
rx[muestras_retardo:muestras_retardo+len(tx)] += tx * atenuacion

# Añadir ruido ambiental
ruido = np.random.normal(0, 0.15, len(rx))
rx += ruido

# Guardar los vectores generados para que los otros scripts los utilicen
np.savez("proyecto_1/deteccion_ecos/src/datos_ecos.npz", tx=tx, rx=rx, fs=fs, retardo_simulado=retardo_simulado)

# Graficar y guardar imagen
plt.figure(figsize=(10, 4))
plt.plot(t_rx, rx, label="Señal recibida (Directa + Eco + Ruido)")
plt.xlabel("Tiempo (s)")
plt.ylabel("Amplitud")
plt.title("Simulación de Señal Transmitida y Eco")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("proyecto_1/deteccion_ecos/imagenes/generacion_senal.png")
print("Señal generada y guardada")
