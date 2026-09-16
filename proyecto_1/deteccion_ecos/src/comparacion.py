import numpy as np
import time


datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")  #Carga el archivo comprimido guardado .npz
tx = datos["tx"]  #De los datos conseguimos el pulso
rx = datos["rx"]  #De los datos conseguimos el ruido + eco
fs = datos["fs"]  #De los datos conseguimos la frecuencuia de muestreo 44100 Hz
retardo_real = float(datos["retardo_simulado"])  #Lee el valor del retardo simulado teórico (0.120005 s)

print(f"Retardo simulado real: {retardo_real:.6f} s\n")  #Imprime el retardo real con 6 decimales

#Correlación directa
t0 = time.time()  # Toma tiempo de inicio
corr_dir = np.correlate(rx, tx, mode="valid")  # Ejecuta correlación en tiempo
t1 = time.time()  #Toma tiempo final
tiempo_dir = t1 - t0  #Tiempo que duró el cálculo directo

#Ignorar la zona muerta de la onda directa
muestras_zona_muerta = len(tx)
corr_dir[:muestras_zona_muerta] = 0

#Calcular el retardo y su error respecto al valor real
retardo_dir = np.argmax(corr_dir) / fs  #Convierte posición del pico a segundos
error_dir = abs(retardo_real - retardo_dir)  #Calcula la diferencia absoluta (error)

#Correlación FFT
t0 = time.time()  #Toma tiempo de inicio
N = len(rx) + len(tx) - 1  #Tamaño mínimo requerido
N_fft = 2**int(np.ceil(np.log2(N)))  #Ajusta a la potencia de 2 más cercana para acelerar la FFT

#Realiza la FFT de ambas señales, las multiplica por el conjugado y aplica la IFFT
corr_fft = np.fft.ifft(np.fft.fft(rx, N_fft) * np.conj(np.fft.fft(tx, N_fft)))
corr_fft = np.real(corr_fft)[:len(rx) - len(tx) + 1]  #Extrae la parte real recortada
t1 = time.time()  #Toma tiempo final
tiempo_fft = t1 - t0  #Tiempo que duró el cálculo por FFT

#Ignorar la zona muerta de la onda directa
corr_fft[:muestras_zona_muerta] = 0

#Calcular el retardo y su error respecto al valor real
retardo_fft = np.argmax(corr_fft) / fs  #Convierte posición del pico a segundos
error_fft = abs(retardo_real - retardo_fft)  #Calcula la diferencia absoluta (error)

#Impresión de datos
print(f"{'MÉTODO':<10} | {'RETARDO (s)':<15} | {'ERROR (s)':<15} | {'TIEMPO (s)':<15}")
print("-" * 65)
print(f"{'Directo':<10} | {retardo_dir:<15.6f} | {error_dir:<15.6f} | {tiempo_dir:<15.6f}")
print(f"{'Por FFT':<10} | {retardo_fft:<15.6f} | {error_fft:<15.6f} | {tiempo_fft:<15.6f}")
print("-" * 65)

# Calcula cuántas veces fue más rápida una opción sobre la otra
if tiempo_fft > 0:
    rendimiento = tiempo_dir / tiempo_fft
    print(f"Rendimiento: La FFT fue {rendimiento:.2f} veces más rápida.")