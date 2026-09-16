import numpy as np
import matplotlib.pyplot as plt
import time


datos = np.load("proyecto_1/deteccion_ecos/src/datos_ecos.npz")  #Carga el archivo comprimido guardado .npz
tx = datos["tx"]  #De los datos conseguimos el pulso
rx = datos["rx"]  #De los datos conseguimos el ruido + eco
fs = datos["fs"]  #De los datos conseguimos la frecuencuia de muestreo 44100 Hz

inicio = time.time()  #Tomamos como referencia el inicio previo al calculo

#Desliza el pulso "tx" a lo largo de "rx" en el dominio del tiempo para encontrar coincidencias.
#mode="valid" se encarga que solo se calcule donde "tx" entra completamente dentro de "rx".
corr = np.correlate(rx, tx, mode="valid")

fin = time.time()  #Guarda la hora exacta al finalizar
tiempo_ejecucion = fin - inicio  #Resta los tiempos para saber cuánto tardó en segundos

muestras_zona_muerta = len(tx)  #Cantidad de muestras que dura el pulso inicial
corr[:muestras_zona_muerta] = 0 #Borra la correlación inicial para no confundir la onda directa con un eco

muestras_retardo_estimado = np.argmax(corr)  #Encuentra el índice donde la correlación dio el valor más alto
retardo_estimado = muestras_retardo_estimado / fs  #Convierte el índice de muestra a segundos dividiendo por fs

print("--- Correlación Directa ---")
print(f"Retardo estimado: {retardo_estimado:.6f} s")  
print(f"Tiempo de ejecución: {tiempo_ejecucion:.6f} s")  

t_corr = np.arange(len(corr)) / fs  #Arreglo de tiempo en segundos para el eje X
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