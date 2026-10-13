# Guía de conexiones — PCB MÍNIMA (KiCad)

Prototipo mínimo: ESP32-S3-WROOM-1 + TFT 2,8" ILI9341 + 24 pulsadores (matriz 5×5) +
LiPo con carga USB-C. Sin MCP23017, sin cámara.

**Método (igual que en el diseño grande):**
- `GND`, `+3V3`, `+5V` → **símbolos de alimentación** (tecla **P**).
- Todo lo demás → **etiquetas de red** (tecla **L**). Mismo nombre = misma red.
- `VBAT`, `VSYS` y `VSYS_SW` pueden ser etiquetas o símbolos de alimentación propios.

> ⚠️ Los números de pin del ESP32 son los del **ESP32-S3-WROOM-1** (datasheet de Espressif).
> Conéctalos por **nombre** (IO4, IO5…) y compara con tu símbolo de KiCad antes de cablear.

## Referencias

| Ref | Parte | Ref | Parte |
|---|---|---|---|
| U1 | ESP32-S3-WROOM-1 N16R8 | J1 | USB-C 16 pines (USB 2.0) |
| U2 | TP4056 (cargador LiPo) | J2 | JST-PH 2 pines (batería) |
| U3 | AP2112K-3.3 (LDO 3V3) | J3 | Zócalo hembra 1×14 (TFT) |
| Q1 | AO3401A (P-MOSFET, carga con la placa encendida) | J4 | Header 1×04 (UART, opcional) |
| Q2 | AO3401A (P-MOSFET, retroiluminación) | SW1 / SW2 | RESET / BOOT |
| D1 | SS14 (Schottky) | SW3 | Interruptor deslizante ON/OFF |
| D2 / D3 | LED rojo (cargando) / LED verde (cargada) | SW10–SW33 | 24 teclas 6×6 |

---

## BLOQUE A — ESP32-S3-WROOM-1 (U1)

Alimentación:
- `GND` → U1 **GND (1)**, **GND (40)**, **EPAD (41)**
- `+3V3` → U1 **3V3 (2)**
- C6 **22 µF** y C7 **100 nF**: `+3V3` — `GND`, **pegados al pin 2**

Reset / Boot:
- Red **EN**: U1 **EN (3)** + R1 + C1 + SW1
  - R1 **10k**: `+3V3` — `EN`
  - C1 **1 µF**: `EN` — `GND`   (Espressif recomienda 1 µF en el S3)
  - SW1 (RESET): `EN` — `GND`
- Red **BOOT**: U1 **IO0 (27)** + SW2 (SW2: `BOOT` — `GND`)

Teclado (matriz 5×5):
- `ROW0` → **IO4 (4)** · `ROW1` → **IO5 (5)** · `ROW2` → **IO6 (6)** · `ROW3` → **IO7 (7)** · `ROW4` → **IO15 (8)**
- `COL0` → **IO16 (9)** · `COL1` → **IO17 (10)** · `COL2` → **IO18 (11)** · `COL3` → **IO8 (12)** · `COL4` → **IO9 (17)**

Pantalla TFT (SPI):
- `TFT_CS` → **IO10 (18)** · `MOSI` → **IO11 (19)** · `SCK` → **IO12 (20)**
- `TFT_DC` → **IO13 (21)** · `TFT_RST` → **IO14 (22)** · `BL` → **IO21 (23)**

USB nativo:
- `USB_DM` → **IO19 (13)** · `USB_DP` → **IO20 (14)**

Batería:
- `ADC_VBAT` → **IO1 (39)**

Depuración (opcional):
- `U0TXD` → **TXD0 (37)** · `U0RXD` → **RXD0 (36)**

**Sin conectar:** IO2, IO3, IO35–IO42, IO45, IO46, IO47, IO48.
(IO3, IO45 e IO46 son de arranque: déjalos libres. IO35–37 los usa la PSRAM octal.)

---

## BLOQUE B — Teclado (SW10–SW33, pulsadores 6×6 THT)

Cada pulsador: **pin 1 → fila**, **pin 2 → columna**. Sin diodos ni resistencias.
La disposición física es la misma que la lógica; la fila R4 (flechas) va **arriba**.

| Ref | Tecla | Fila | Col | | Ref | Tecla | Fila | Col |
|---|---|---|---|---|---|---|---|---|
| SW10 | 7 | ROW0 | COL0 | | SW22 | 3 | ROW2 | COL2 |
| SW11 | 8 | ROW0 | COL1 | | SW23 | − | ROW2 | COL3 |
| SW12 | 9 | ROW0 | COL2 | | SW24 | F1 | ROW2 | COL4 |
| SW13 | ÷ | ROW0 | COL3 | | SW25 | 0 | ROW3 | COL0 |
| SW14 | AC | ROW0 | COL4 | | SW26 | . | ROW3 | COL1 |
| SW15 | 4 | ROW1 | COL0 | | SW27 | = | ROW3 | COL2 |
| SW16 | 5 | ROW1 | COL1 | | SW28 | + | ROW3 | COL3 |
| SW17 | 6 | ROW1 | COL2 | | SW29 | F2 | ROW3 | COL4 |
| SW18 | × | ROW1 | COL3 | | SW30 | ← | ROW4 | COL0 |
| SW19 | DEL | ROW1 | COL4 | | SW31 | ↑ | ROW4 | COL1 |
| SW20 | 1 | ROW2 | COL0 | | SW32 | ↓ | ROW4 | COL2 |
| SW21 | 2 | ROW2 | COL1 | | SW33 | → | ROW4 | COL3 |

(ROW4 × COL4 queda libre.)

---

## BLOQUE C — Pantalla TFT 2,8" ILI9341 (J3, zócalo hembra 1×14)

Orden del módulo SPI rojo típico de 2,8" (con táctil y SD). **Compáralo con la serigrafía de tu módulo.**

- J3.1 `VCC` → `+3V3`
- J3.2 `GND` → `GND`
- J3.3 `CS` → `TFT_CS`
- J3.4 `RESET` → `TFT_RST`
- J3.5 `DC` → `TFT_DC`
- J3.6 `SDI (MOSI)` → `MOSI`
- J3.7 `SCK` → `SCK`
- J3.8 `LED` → `BL_LED`
- J3.9 `SDO (MISO)` y J3.10–J3.14 (táctil) → **sin conectar**
- C8 **100 nF**: `+3V3` — `GND`, junto a J3.1

Retroiluminación por Q2 (P-MOSFET; protege el GPIO y queda apagada durante el arranque):
- Q2 **S** → `+3V3` · Q2 **D** → `BL_LED` · Q2 **G** → `BL`
- R10 **10k**: `+3V3` — `BL`   (pull-up: apagada hasta que el firmware la encienda)
- Firmware: compilar con `pio run -e pcb` (enciende con LOW).

> Si el módulo trae un jumper "J1" para alimentarlo a 3,3 V (puentea su regulador), ciérralo.

---

## BLOQUE D — USB-C (J1)

- `+5V` → J1 **A4, B4, A9, B9** (VBUS)
- `GND` → J1 **A1, B1, A12, B12** y **SHIELD**
- `USB_DP` → J1 **A6** y **B6** · `USB_DM` → J1 **A7** y **B7**
- R2 **5.1k**: J1 **A5 (CC1)** — `GND` · R3 **5.1k**: J1 **B5 (CC2)** — `GND`
- A8 / B8 (SBU) → sin conectar

---

## BLOQUE E — Carga de batería (U2 TP4056, J2)

- `+5V` → U2 **VCC (4)** y **CE (8)**
- `VBAT` → U2 **BAT (5)**
- `GND` → U2 **GND (3)**, **EPAD (9)** y **TEMP (1)** (sin NTC)
- Red **PROG**: U2 **PROG (2)** + R4 (R4 → `GND`)
  - R4 **2k** ≈ 580 mA (seguro para LiPo ≥ 600 mAh). 1.2k ≈ 1 A solo si la batería es ≥ 1000 mAh y el EPAD está bien soldado.
- Red **CHRG**: U2 **CHRG (7)** + cátodo D2 (rojo). Ánodo D2 → R5 **1k** → `+5V`
- Red **STDBY**: U2 **STDBY (6)** + cátodo D3 (verde). Ánodo D3 → R6 **1k** → `+5V`
- C2 **10 µF**: `+5V` — `GND` · C3 **10 µF**: `VBAT` — `GND`
- J2.1 → `VBAT` · J2.2 → `GND`   (LiPo 1S **con PCM integrado**; revisa la polaridad del cable: no está estandarizada)

---

## BLOQUE F — Alimentación del sistema (Q1, D1, SW3, U3)

Para que la placa funcione mientras carga sin confundir al TP4056: con USB, el sistema se
alimenta desde `+5V` por D1; sin USB, desde la batería por Q1.

- D1 (SS14): ánodo `+5V` → cátodo `VSYS`
- Q1 (AO3401A): **D** → `VBAT` · **S** → `VSYS` · **G** → `+5V`
- R7 **100k**: `+5V` — `GND`   (lleva el gate de Q1 a 0 V cuando se desconecta el USB)
- SW3 (deslizante): común → `VSYS_SW` · un extremo → `VSYS` · el otro sin conectar

LDO 3,3 V — U3 AP2112K-3.3 (SOT-23-5):
- `VSYS_SW` → U3 **VIN (1)** y **EN (3)**
- `GND` → U3 **GND (2)** · U3 **NC (4)** sin conectar
- `+3V3` → U3 **VOUT (5)**
- C4 **10 µF**: `VSYS_SW` — `GND` · C5 **22 µF**: `+3V3` — `GND`

Medición de batería:
- R8 **100k**: `VBAT` — `ADC_VBAT` · R9 **100k**: `ADC_VBAT` — `GND` · C9 **100 nF**: `ADC_VBAT` — `GND`

---

## BLOQUE G — UART de depuración (J4, opcional)

- J4.1 `+3V3` · J4.2 `U0TXD` · J4.3 `U0RXD` · J4.4 `GND`

---

## Al terminar
1. **ERC.** Avisos esperables: etiquetas con un solo pin (no debería haber ninguna si todo está cableado).
2. Asignar huellas según `BOM_minima.csv`.
3. Pasar a la placa siguiendo `PCB_Especificacion_minima.md`.
