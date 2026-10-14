# CalculadoraIA — prototipo MÍNIMO

Prototipo paralelo y simplificado de la CalculadoraIA (Casio fx-CG50 con ESP32-S3).
Solo las teclas básicas + 2 teclas de modo:

- **Modo calculadora:** `0–9`, `+ − × ÷`, `.`, `=`, `AC`, `DEL`, flechas.
- **F1 → modo IA:** menú de preguntas a **Claude** sobre la operación actual; la respuesta se muestra en la TFT.
- **F2 → vuelve** a la calculadora.

El diseño completo (50 teclas, cámara, MCP23017) sigue en pausa y **no** se toca aquí.

## Estado

| Fase | Qué | Estado |
|---|---|---|
| 0 | Firmware en protoboard: ESP32-S3-DevKitC + TFT SPI + 24 pulsadores | **Firmware listo** (compila; lógica con pruebas) — falta probarlo en hardware |
| 1 | PCB mínima 90 × 135 mm (mismos pines) | **Esquema, BOM y plano listos** en [`hardware/`](hardware/) — falta dibujarla en KiCad |

## Decisiones de este prototipo (2026-10-12)

- **M-001** Hardware: ESP32-S3 + TFT SPI + matriz 5×5 **directa en GPIO**. **Sin MCP23017 y sin cámara.**
- **M-002** Teclado: **pulsadores táctiles 6×6** propios (no la goma original) → PCB trivial, sin ENIG obligatorio.
- **M-003** Alimentación (para la PCB): **LiPo 1S con PCM propio + TP4056 (USB-C) + LDO 3V3 SOT-23-5** (AP2112K-3.3 / ME6211). Sin DW01A/FS8205A ni TPS6302x: todo soldable a mano.
- **M-004** F1 = **menú de preguntas predefinidas** sobre la operación actual (sin texto libre).
- **M-005** Modelo: **Claude Opus 5.5** (`claude-opus-5-5`), `effort: medium`, respuesta corta en texto plano.
  Si el modelo declina una consulta, la API la reintenta en otro modelo (`fallbacks: "default"`).
- **M-006** Pantalla: **TFT 2,8" ILI9341 320×240** (2026-10-13). En la ventana de la Casio (75 × 43 mm)
  encaja de alto; los ~8,7 mm que sobran a cada lado se cubren con un **marco negro** impreso.

## Fase 0 — lo que necesitas

- ESP32-S3-DevKitC-1 (cualquier variante; ideal N16R8 como el módulo final)
- Módulo TFT SPI **ILI9341** o **ST7789** 240×320 (el firmware tiene una configuración para cada uno)
- 24 pulsadores táctiles 6×6 mm, protoboard y cables
- Red WiFi 2.4 GHz y una API key de Anthropic

## Cableado (DevKitC ↔ TFT y teclado)

**TFT** (mismos pines que el diseño grande):

| TFT | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | IO10 |
| RST | IO14 |
| DC | IO13 |
| MOSI / SDA | IO11 |
| SCK / SCL | IO12 |
| BL / LED | **3V3** (en protoboard) |

En la protoboard, el pin LED va a 3V3: la luz de fondo queda siempre encendida y el GPIO no carga
con su corriente. IO21 queda libre. En la PCB la controla el P-MOSFET Q2 desde IO21.
Guía visual paso a paso: https://claude.ai/artifact/F7U5y6DYWdb4tAgpFHSM6E

**Teclado — matriz 5×5.** Cada pulsador une su **fila** con su **columna**. No lleva resistencias
(pull-ups internos) ni diodos (no hacen falta combinaciones de teclas).

| Fila \ Col | C0 = IO16 | C1 = IO17 | C2 = IO18 | C3 = IO8 | C4 = IO9 |
|---|---|---|---|---|---|
| **R0 = IO4**  | 7 | 8 | 9 | ÷ | AC |
| **R1 = IO5**  | 4 | 5 | 6 | × | DEL |
| **R2 = IO6**  | 1 | 2 | 3 | − | F1 |
| **R3 = IO7**  | 0 | . | = | + | F2 |
| **R4 = IO15** | ← | ↑ | ↓ | → | (libre) |

Pines elegidos para no tocar: strapping (IO0, IO3, IO45, IO46), PSRAM octal (IO33–37),
USB (IO19/20), JTAG (IO39–42) ni el LED RGB de algunos DevKitC (IO38/IO48).
IO8/IO9 quedan libres porque ya no hay I2C. `ADC_VBAT` (IO1) se reserva para la PCB.

## Compilar y subir

Con **PlatformIO** (VS Code + extensión PlatformIO, o `pip install platformio`):

```bash
cd firmware
cp src/secrets.h.example src/secrets.h     # y completa WiFi + API key
pio run -e ili9341 -t upload               # o: -e st7789 · en la PCB mínima: -e pcb
pio device monitor                          # log por el puerto serie
```

- Conecta el DevKitC por la entrada **COM** (o UART) para subir el programa: su puerto no desaparece cuando el ESP32
  se reinicia. (El log de `Serial` sale por la entrada USB nativa, como en la PCB final.)
- Si la carga falla con `MD5 of file does not match data in flash`, borra la memoria una vez y vuelve a subir:
  `esptool.py --port COMx erase_flash` (pasó con una N16R8 clónica con flash Boya).
- El firmware está configurado para el módulo **N16R8** (flash DIO de 16 MB + PSRAM octal, `dio_opi`).
  Con la configuración por defecto de la placa (`qio_qspi`) esa N16R8 clónica se reiniciaba en bucle
  (`Guru Meditation Error`, watchdog).
- `src/secrets.h` está en `.gitignore`. **Nunca subas la API key al repo.**
- ST7789: si los colores salen invertidos o con rojo y azul cambiados, mira los comentarios de `[env:st7789]` en `platformio.ini`.
- Pruebas de la lógica en la PC, sin hardware: `pio test -e native`.

## Cómo se usa

**Modo CALC**

| Tecla | Acción |
|---|---|
| `0–9 . + − × ÷` | Escribe en la posición del cursor |
| `=` | Calcula. Después, un operador **sigue sobre el resultado**; un número empieza de cero |
| `DEL` | Borra el carácter a la izquierda del cursor |
| `AC` | Borra todo |
| `← →` | Mueve el cursor |
| `↑ ↓` | Historial (últimas 8 operaciones) |
| `F1` | Modo IA con la operación actual (si no hay operación, avisa) |

**Modo IA**

1. `F1` muestra *"Preguntar a Claude sobre: 125x4/3 = 166.6666667"* y un menú:
   - Explicar paso a paso
   - Verificar el resultado
   - Otra forma de resolverlo
   - Dato curioso del resultado
2. `↑ ↓` elige, `=` envía. Aparece "Consultando a Claude..." (unos segundos; el teclado no responde mientras tanto).
3. En la respuesta: `↑ ↓` la desplaza, `AC` o `F1` vuelven al menú, `F2` vuelve a la calculadora.

Las fuentes de la pantalla son ASCII, así que el firmware convierte acentos y símbolos
(`á→a`, `ñ→n`, `×→x`, `÷→/`, `²→^2`...). La pregunta pide además texto plano y breve.

## Claude API

- `POST https://api.anthropic.com/v1/messages` con `x-api-key`, `anthropic-version: 2023-06-01`,
  `anthropic-beta: server-side-fallback-2026-07-01` y `content-type: application/json`.
- Modelo `claude-opus-5-5`, `output_config.effort = "medium"` y `max_tokens = 4096`. Opus 5.5 siempre razona
  (no se puede apagar) y ese razonamiento cuenta dentro de `max_tokens`, por eso el margen. Con `effort = "low"`
  responde más rápido; con `"high"` razona más.
- `fallbacks: "default"`: si el modelo declina la consulta, la API la reintenta sola en el modelo que Anthropic
  recomienda para ese caso. Para quitarlo, borra esa línea y el header `anthropic-beta` en `claude_client.cpp`.
- La respuesta puede traer bloques `thinking` vacíos (y `fallback` si hubo reintento): solo se muestran los bloques `text`.
  Se manejan `stop_reason: "refusal"` (aviso) y `"max_tokens"` (se agrega `[...]`).
- Costo aproximado: unos US$0,01–0,03 por consulta (US$4 por millón de tokens de entrada y US$20 por millón de salida;
  el razonamiento se cobra como salida).
- Ajustes en `firmware/src/config.h`; textos del menú y de las preguntas en `firmware/src/main.cpp`.

### TLS

Por defecto el firmware usa `setInsecure()`: la conexión va **cifrada**, pero el ESP32 no
verifica que del otro lado esté realmente `api.anthropic.com`. Sirve para la prueba; para
algo más serio, define `ANTHROPIC_ROOT_CA` en `secrets.h` con el certificado **raíz** de la
CA que firma el de la API. Para identificarla:

```bash
openssl s_client -connect api.anthropic.com:443 -showcerts </dev/null
```

El campo `issuer` del último certificado de la cadena nombra la CA raíz. Descarga ese raíz
(PEM) desde el sitio de la CA y pégalo en `secrets.h`. Los raíces duran años; los
intermedios cambian más seguido.

## Estructura

```
firmware/
  platformio.ini          entornos ili9341 / st7789 / native; pines de la TFT
  src/
    config.h              pines de la matriz, modelo y parámetros de Claude
    secrets.h.example     plantilla de WiFi + API key (copiar a secrets.h)
    main.cpp              modos CALC / menú IA / respuesta
    keypad.*              escaneo 5x5, antirrebote 20 ms, autorepetición en flechas y DEL
    calculator.*          evaluador + − × ÷ con precedencia, cursor, historial
    claude_client.*       HTTPS a la Messages API (WiFiClientSecure + ArduinoJson)
    ui.*                  pantallas con TFT_eSPI (320x240 horizontal)
    text_utils.*          UTF-8 → ASCII para la pantalla
  test/test_calculator/   pruebas de calculadora y texto (pio test -e native)
hardware/
  guia_conexiones_minima.md   esquemático bloque por bloque para KiCad
  BOM_minima.csv              materiales con huellas sugeridas
  PCB_Especificacion_minima.md  contorno, ubicación, ruteo, primer encendido
  contorno_minima.dxf         contorno 90 × 135 (Edge.Cuts)
  floorplan_minima.svg        plano de ubicación
```

## Pantalla y ventana de la Casio

- **Ventana (abertura) de la Casio: 75 × 43 mm** (ancho × alto, medida 2026-10-13). Es formato
  ~16:9, pensado para la pantalla original de 3,17" (384×216). Hueco interior: ~79 × 69 mm.
- Ninguna TFT SPI común es 16:9; comparación (imagen / vidrio aprox., confirmar con el módulo):

| Pantalla | Imagen | Vidrio | Resultado en la ventana |
|---|---|---|---|
| 2,8" ILI9341 320×240 | 57,6 × 43,2 | ≈ 69 × 50 | Alto justo; ~8,7 mm vacíos a cada lado → requiere marco negro |
| 3,2" ILI9341 320×240 | 64,8 × 48,6 | ≈ 78 × 56 | Cubre toda la ventana; se tapan ~2,8 mm (≈14 px) arriba y abajo |
| 3,5" ILI9488 480×320 | 73,4 × 49 | ≈ 85 × 55 | El vidrio no entra en el hueco de 79 mm |

- **Elegida: 2,8" ILI9341** (M-006). La imagen (57,6 × 43,2) ocupa todo el alto de la ventana y queda centrada
  a lo ancho; un **marco negro** (impreso en 3D) tapa los ~8,7 mm de cada costado, donde el vidrio no llega.
  El firmware no cambia (320×240 horizontal, todo el área visible). La placa típica del módulo (≈ 86 × 50 mm)
  es más ancha que el hueco (79 mm): para el montaje final, panel suelto o placa recortada.

## PCB mínima (fase 1)

Detalle completo en [`hardware/PCB_Especificacion_minima.md`](hardware/PCB_Especificacion_minima.md) y
[`hardware/guia_conexiones_minima.md`](hardware/guia_conexiones_minima.md). En resumen:

- Placa de prueba **90 × 135 mm**, 2 capas, HASL. Todo en la cara frontal; la TFT 2,8" va enchufada encima (J3).
- **ESP32-S3-WROOM-1 N16R8** soldado, con la antena en el borde superior (keepout sin cobre). Mismos pines que la fase 0.
- **USB-C** nativo (programación + carga). **TP4056** (R_PROG 2k ≈ 580 mA) con **carga con la placa encendida**
  (Q1 + D1). Interruptor ON/OFF y **LDO AP2112K-3.3**.
- Retroiluminación por P-MOSFET (activa en LOW): compilar con `pio run -e pcb`.
- Divisor 100k/100k de VBAT a IO1 (el firmware todavía no lo usa).
