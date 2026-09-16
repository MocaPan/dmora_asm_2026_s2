import numpy as np
import matplotlib.pyplot as plt
import time

# Cargar señales
datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")
tx, rx, fs = datos["tx"], datos["rx"], datos["fs"]

# Implementación de correlación directa
inicio = time.time()
# Se usa mode="valid" para desplazar tx sobre toda la longitud de rx
corr = np.correlate(rx, tx, mode="valid")
fin = time.time()

tiempo_ejecucion = fin - inicio
muestras_zona_muerta = len(tx) 
corr[:muestras_zona_muerta] = 0
muestras_retardo_estimado = np.argmax(corr)
retardo_estimado = muestras_retardo_estimado / fs

print("--- Correlación Directa ---")
print(f"Retardo estimado: {retardo_estimado:.4f} s")
print(f"Tiempo de ejecución: {tiempo_ejecucion:.6f} s")

# Graficar
t_corr = np.arange(len(corr)) / fs
plt.figure(figsize=(10, 4))
plt.plot(t_corr, corr)
plt.axvline(x=retardo_estimado, color="r", linestyle="--", label=f"Eco detectado en {retardo_estimado:.4f}s")
plt.xlabel("Retardo (s)")
plt.ylabel("Amplitud de Correlación")
plt.title("Detección de Eco - Correlación Directa")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("proyecto_1/deteccion_ecos/imagenes/correlacion_directa.png")