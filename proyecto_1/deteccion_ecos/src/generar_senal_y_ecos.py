import numpy as np  
import matplotlib.pyplot as plt  
from scipy.signal import chirp  


fs = 44100  # Frecuencia de muestreo en Hz 44100 muestras por segundo
t_dur = 0.05  # Duración del pulso en segundos (50 ms)
t = np.arange(0, t_dur, 1/fs)  # Vector de tiempo desde 0 hasta t_dur a pasos de 1/fs

retardo_simulado = 0.120005  # Tiempo en segundos que tarda en regresar el eco a 12 ms, con un desfase
atenuacion = 0.4  # se conserva el la energia del eco a 0.4 de la señal original

tx = chirp(t, f0=1000, f1=5000, t1=t_dur, method="linear") #se crea un pulso que aumenta de manera lineal de 1k a 5k en t_dur


t_rx = np.arange(0, t_dur + 0.3, 1/fs)  # Vector de tiempo total de recepción (0.35 segundos)
rx = np.zeros_like(t_rx)  # Crea un arreglo lleno de ceros con el mismo tamaño de t_rx

rx[:len(tx)] += tx * 0.8  # Se simula la onda directa insertada al inicio con un 80% de amplitud

muestras_retardo = int(retardo_simulado * fs)  # Convertimos el tiempo en segundos a un número entero de muestras
rx[muestras_retardo:muestras_retardo+len(tx)] += tx * atenuacion  # Insertamos el eco atenuado en esa posición

ruido = np.random.normal(0, 0.15, len(rx))  # Generamos ruido a la señal de amplitud de 0.15
rx += ruido  # Se lo sumamos a la onda que ya teníamos 

#se crea un .npz donde se guarden los vectores que se usan en el resto de .py
np.savez("proyecto_1/deteccion_ecos/src/datos_ecos.npz", tx=tx, rx=rx, fs=fs, retardo_simulado=retardo_simulado) 

plt.figure(figsize=(10, 4))  
plt.plot(t_rx, rx, label="Señal recibida (Directa + Eco + Ruido)")  
plt.xlabel("Tiempo (s)")  
plt.ylabel("Amplitud")  
plt.title("Simulación de Señal Transmitida y Eco")  
plt.legend()  
plt.grid(True)  
plt.tight_layout()  
plt.savefig("proyecto_1/deteccion_ecos/imagenes/generacion_senal.png")  
print("Señal generada y guardada exitosamente.")  