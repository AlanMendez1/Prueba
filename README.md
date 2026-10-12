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
| 1 | PCB mínima (mismos pines) | Pendiente |

## Decisiones de este prototipo (2026-10-12)

- **M-001** Hardware: ESP32-S3 + TFT SPI + matriz 5×5 **directa en GPIO**. **Sin MCP23017 y sin cámara.**
- **M-002** Teclado: **pulsadores táctiles 6×6** propios (no la goma original) → PCB trivial, sin ENIG obligatorio.
- **M-003** Alimentación (para la PCB): **LiPo 1S con PCM propio + TP4056 (USB-C) + LDO 3V3 SOT-23-5** (AP2112K-3.3 / ME6211). Sin DW01A/FS8205A ni TPS6302x: todo soldable a mano.
- **M-004** F1 = **menú de preguntas predefinidas** sobre la operación actual (sin texto libre).
- **M-005** Modelo: **Claude Haiku 5.5** (`claude-haiku-5-5`), `effort: low`, respuesta corta en texto plano.

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
| BL / LED | IO21 |

**Teclado — matriz 5×5.** Cada pulsador une su **fila** con su **columna**. No lleva resistencias
(pull-ups internos) ni diodos (no hacen falta combinaciones de teclas).

| Fila \ Col | C0 = IO16 | C1 = IO17 | C2 = IO18 | C3 = IO8 | C4 = IO9 |
|---|---|---|---|---|---|
| **R0 = IO4**  | 7 | 8 | 9 | ÷ | AC |
| **R1 = IO5**  | 4 | 5 | 6 | × | DEL |
| **R2 = IO6**  | 1 | 2 | 3 | − | F1 |
| **R3 = IO7**  | 0 | . | = | + | F2 |
| **R4 = IO15** | ↑ | ↓ | ← | → | (libre) |

Pines elegidos para no tocar: strapping (IO0, IO3, IO45, IO46), PSRAM octal (IO33–37),
USB (IO19/20), JTAG (IO39–42) ni el LED RGB de algunos DevKitC (IO38/IO48).
IO8/IO9 quedan libres porque ya no hay I2C. `ADC_VBAT` (IO1) se reserva para la PCB.

## Compilar y subir

Con **PlatformIO** (VS Code + extensión PlatformIO, o `pip install platformio`):

```bash
cd firmware
cp src/secrets.h.example src/secrets.h     # y completa WiFi + API key
pio run -e ili9341 -t upload               # o: -e st7789
pio device monitor                          # log por el puerto serie
```

- Conecta el DevKitC por el puerto **USB** (no el "UART"): el log va por el USB nativo, como en la PCB final.
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
2. `↑ ↓` elige, `=` envía. Aparece "Consultando a Claude..." (unos segundos).
3. En la respuesta: `↑ ↓` la desplaza, `AC` o `F1` vuelven al menú, `F2` vuelve a la calculadora.

Las fuentes de la pantalla son ASCII, así que el firmware convierte acentos y símbolos
(`á→a`, `ñ→n`, `×→x`, `÷→/`, `²→^2`...). La pregunta pide además texto plano y breve.

## Claude API

- `POST https://api.anthropic.com/v1/messages` con `x-api-key`, `anthropic-version: 2023-06-01` y `content-type: application/json`.
- Modelo `claude-haiku-5-5`, `output_config.effort = "low"` y `max_tokens = 1024`. Haiku 5.5 razona por defecto y ese razonamiento cuenta dentro de `max_tokens`, por eso el margen.
- La respuesta puede traer bloques `thinking` vacíos: solo se muestran los bloques `text`.
  Se manejan `stop_reason: "refusal"` (aviso) y `"max_tokens"` (se agrega `[...]`).
- Costo aproximado: unos US$0,0004 por consulta (US$0,10 por millón de tokens de entrada y US$0,50 por millón de salida).
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
```

## Notas para la PCB (fase 1)

- Mismos pines que la fase 0. Se cambia el DevKitC por **ESP32-S3-WROOM-1 N16R8** con EN
  (10k + 100nF + RESET), IO0 (BOOT) y USB-C con D+/D− a IO20/IO19 y CC1/CC2 con 5.1k a GND.
- **LDO 3V3:** AP2112K-3.3 o ME6211 (600/500 mA). Pon bulk de ≥ 22 µF a la salida y 10 µF a la entrada:
  los picos de transmisión WiFi rondan los 350–500 mA. Con LiPo funciona hasta ~3.5 V; la última parte de la descarga no se aprovecha (aceptable en un prototipo).
- **TP4056 sin load-sharing:** si la placa funciona mientras carga, el consumo engaña al fin de carga.
  Para el prototipo es aceptable; si molesta, se agrega P-MOSFET + Schottky.
- Divisor 100k/100k de VBAT a IO1 para medir la batería (el firmware todavía no lo usa).
