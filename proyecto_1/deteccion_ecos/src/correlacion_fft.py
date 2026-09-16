import numpy as np
import matplotlib.pyplot as plt
import time


datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")  #Carga el archivo comprimido guardado .npz
tx = datos["tx"]  #De los datos conseguimos el pulso
rx = datos["rx"]  #De los datos conseguimos el ruido + eco
fs = datos["fs"]  #De los datos conseguimos la frecuencuia de muestreo 44100 Hz

inicio = time.time()  #Tomamos como referencia el inicio previo al calculo

N = len(rx) + len(tx) - 1 #Para evitar distorsiones al usar convolución/correlación circular, N debe ser la suma las dos señales menos 1

N_fft = 2**int(np.ceil(np.log2(N))) #Se redondea N hacia la potencia de 2 más cercana, para que FFT puede trabajar de la forma más rápida

TX_fft = np.fft.fft(tx, N_fft)  #Convierte el pulso al dominio de la frecuencia
RX_fft = np.fft.fft(rx, N_fft)  #Convierte la señal al dominio de la frecuencia

#Correlación en tiempo = Multiplicación en frecuencia por el complejo conjugado: R_xy = IFFT( RX * conj(TX) )
corr_freq = RX_fft * np.conj(TX_fft)  #Multiplica el espectro de RX por el conjugado espectral de TX
corr_fft_full = np.fft.ifft(corr_freq)  #Aplica la transformada inversa (IFFT) para volver al dominio del tiempo

corr_fft = np.real(corr_fft_full)[:len(rx) - len(tx) + 1] #Agarramos solo lo real lo imaginario lo despreciamos, y recortamos al tamaño establecido

fin = time.time()  #Guarda la hora exacta al finalizar
tiempo_ejecucion = fin - inicio #Resta los tiempos para saber cuánto tardó en segundos

muestras_zona_muerta = len(tx)  #Longitud del pulso directo inicial
corr_fft[:muestras_zona_muerta] = 0  #Elimina el pico de la señal directa

muestras_retardo_estimado = np.argmax(corr_fft)  #Encuentra el índice donde la correlación dio el valor más alto
retardo_estimado = muestras_retardo_estimado / fs  #Convierte el índice de muestra a segundos dividiendo por fs

print("--- Correlación vía FFT ---")
print(f"Retardo estimado: {retardo_estimado:.6f} s")  
print(f"Tiempo de ejecución: {tiempo_ejecucion:.6f} s") 


t_corr = np.arange(len(corr_fft)) / fs  # Tiempo en segundos para el eje X
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