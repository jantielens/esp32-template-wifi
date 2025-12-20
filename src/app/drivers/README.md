# Drivers (Display + Touch)

This folder contains reusable **display** and **touch** driver implementations used by multiple boards.

## Where driver selection lives

Board selection is done at compile time in:
- `src/boards/<board>/board_overrides.h` (see **Driver Selection (HAL)**)
- Defaults live in `src/app/board_config.h`

## Generated board → drivers table

To make it easy to see which boards use which backends/controllers, we generate a table from the board override headers.

Regenerate this table:

```bash
python3 tools/generate-board-driver-table.py --update-drivers-readme
```

<!-- BOARD_DRIVER_TABLE_START -->

| Board | Display backend | Panel | Bus | Res | Rot | Touch backend | Notes |
|---|---|---|---:|---:|---:|---|---|
| cyd-v2 | TFT_eSPI | ILI9341 | SPI | 320×240 | 1 | XPT2046 | inversion on, gamma fix |
| cyd-v3 | TFT_eSPI | ILI9341 | SPI | 320×240 | 1 | XPT2046 | inversion off, gamma fix |
| esp32c3 | ST7789V2 (native) | ST7789V2 | SPI | 240×280 | 1 | none |  |
| jc3248w535 | Arduino_GFX | AXS15231B | QSPI | 320×480 | 1 | AXS15231B |  |
| jc3636w518 | ESP_Panel | ST77916 | QSPI | 360×360 | 0 | CST816S |  |

<!-- BOARD_DRIVER_TABLE_END -->

## Conventions

- **Display backend (HAL)** is selected with `DISPLAY_DRIVER` (e.g., `DISPLAY_DRIVER_TFT_ESPI`, `DISPLAY_DRIVER_ARDUINO_GFX`).
- **Touch backend (HAL)** is selected with `TOUCH_DRIVER` (e.g., `TOUCH_DRIVER_XPT2046`, `TOUCH_DRIVER_AXS15231B`).
- TFT_eSPI-specific controller macros (like `DISPLAY_DRIVER_ILI9341_2`) are *controller/config flags*, not the HAL backend selector.
