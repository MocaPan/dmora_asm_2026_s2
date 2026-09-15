# dmora_asm_2026_s2.

Proyecto: Diseño de un sistema de radar acústico para estimación de distancia.  
Curso CE 1110 Análisis de Señales Mixtas — Instituto Tecnológico de Costa Rica.

## Estructura

- `analisis_espectral/` — Experimentos con DFT y FFT.
- `deteccion_ecos/` — Experimentos de detección de ecos por correlación.
- `paper/` — Artículo en LaTeX (entrega final).

## Ramas

- `master` — versión estable, con tags por hito.
- `development` — rama de integración. El trabajo diario se hace aquí.
- Las features nuevas se hacen en ramas de trabajo creadas desde `development` y se mergean de vuelta a `development` con PR.

## Entregables por semana (referencia)

| Semana | Alcance |
|--------|---------|
| 1 | Revisión literaria. |
| 2 | `analisis_espectral/`: DFT, FFT, tiempos, magnitud y fase. |
| 3 | `deteccion_ecos/`: correlación directa y por FFT. |
| 4 | Radar en microcontrolador. |
| 5 | Sistema funcional completo + paper. |
