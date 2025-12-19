#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Tuple


RE_DEFINE = re.compile(r"^\s*#define\s+(?P<key>[A-Z0-9_]+)\s+(?P<value>.+?)\s*(?://.*)?$")
RE_DEFINE_FLAG = re.compile(r"^\s*#define\s+(?P<key>[A-Z0-9_]+)\s*(?://.*)?$")


@dataclass(frozen=True)
class BoardInfo:
    name: str
    display_backend: str
    display_panel: str
    display_bus: str
    resolution: str
    rotation: str
    touch_backend: str
    touch_hw: str
    notes: str


def _strip_parens(value: str) -> str:
    value = value.strip()
    if value.startswith("(") and value.endswith(")"):
        return value[1:-1].strip()
    return value


def _read_defines(path: Path) -> Dict[str, str]:
    defines: Dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = RE_DEFINE.match(line)
        if m:
            key = m.group("key")
            value = m.group("value").strip()
            defines[key] = value
            continue

        m_flag = RE_DEFINE_FLAG.match(line)
        if m_flag:
            # Flag-only define (no explicit value) counts as enabled.
            defines[m_flag.group("key")] = "true"
    return defines


def _map_display_backend(token: str) -> str:
    token = token.strip()
    if token == "DISPLAY_DRIVER_TFT_ESPI":
        return "TFT_eSPI"
    if token == "DISPLAY_DRIVER_ST7789V2":
        return "ST7789V2 (native)"
    if token == "DISPLAY_DRIVER_ARDUINO_GFX":
        return "Arduino_GFX"
    if token == "DISPLAY_DRIVER_LOVYANGFX":
        return "LovyanGFX"
    return token


def _map_touch_backend(token: str) -> Tuple[str, str]:
    token = token.strip()
    if token in ("0", "false", "False"):
        return "none", "none"
    if token == "TOUCH_DRIVER_XPT2046":
        return "XPT2046", "XPT2046"
    if token == "TOUCH_DRIVER_AXS15231B":
        return "AXS15231B", "AXS15231B"
    if token == "TOUCH_DRIVER_FT6236":
        return "FT6236", "FT6236"
    return token, token


def _detect_bus(defines: Dict[str, str]) -> str:
    if "LCD_QSPI_CS" in defines or "LCD_QSPI_PCLK" in defines:
        return "QSPI"
    if any(k.startswith("TFT_") for k in defines.keys()) or "LCD_SCK_PIN" in defines:
        return "SPI"
    return "?"


def _detect_panel(defines: Dict[str, str], display_backend_token: str) -> str:
    if display_backend_token == "DISPLAY_DRIVER_ARDUINO_GFX":
        return "AXS15231B"

    # TFT_eSPI boards often declare a controller selection macro.
    if "DISPLAY_DRIVER_ILI9341_2" in defines:
        return "ILI9341"

    if display_backend_token == "DISPLAY_DRIVER_ST7789V2":
        return "ST7789V2"

    return "?"


def _build_notes(defines: Dict[str, str]) -> str:
    notes = []
    if defines.get("DISPLAY_INVERSION_ON") == "true":
        notes.append("inversion on")
    if defines.get("DISPLAY_INVERSION_OFF") == "true":
        notes.append("inversion off")
    if defines.get("DISPLAY_NEEDS_GAMMA_FIX") == "true":
        notes.append("gamma fix")
    return ", ".join(notes)


def parse_board_overrides(board_name: str, overrides_path: Path) -> BoardInfo:
    defines = _read_defines(overrides_path)

    display_backend_token = defines.get("DISPLAY_DRIVER", "?")
    display_backend = _map_display_backend(display_backend_token)

    touch_enabled = defines.get("HAS_TOUCH")
    if touch_enabled in ("false", "False", "0"):
        touch_backend, touch_hw = "none", "none"
    else:
        touch_token = defines.get("TOUCH_DRIVER", "?")
        touch_backend, touch_hw = _map_touch_backend(touch_token)

    width = _strip_parens(defines.get("DISPLAY_WIDTH", "?"))
    height = _strip_parens(defines.get("DISPLAY_HEIGHT", "?"))
    rotation = _strip_parens(defines.get("DISPLAY_ROTATION", "?"))

    display_bus = _detect_bus(defines)
    display_panel = _detect_panel(defines, display_backend_token)

    notes = _build_notes(defines)

    return BoardInfo(
        name=board_name,
        display_backend=display_backend,
        display_panel=display_panel,
        display_bus=display_bus,
        resolution=f"{width}×{height}",
        rotation=str(rotation),
        touch_backend=touch_backend,
        touch_hw=touch_hw,
        notes=notes,
    )


def generate_table(board_infos: list[BoardInfo]) -> str:
    header = "| Board | Display backend | Panel | Bus | Res | Rot | Touch backend | Notes |\n"
    sep = "|---|---|---|---:|---:|---:|---|---|\n"
    rows = []
    for b in sorted(board_infos, key=lambda x: x.name):
        notes = b.notes if b.notes else ""
        rows.append(
            f"| {b.name} | {b.display_backend} | {b.display_panel} | {b.display_bus} | {b.resolution} | {b.rotation} | {b.touch_backend} | {notes} |"
        )
    return header + sep + "\n".join(rows) + "\n"


def update_markdown_file(path: Path, table_md: str) -> None:
    start = "<!-- BOARD_DRIVER_TABLE_START -->"
    end = "<!-- BOARD_DRIVER_TABLE_END -->"
    text = path.read_text(encoding="utf-8", errors="replace")

    if start not in text or end not in text:
        raise SystemExit(
            f"{path} is missing BOARD_DRIVER_TABLE markers. "
            "Add them or run without an update flag."
        )

    before, rest = text.split(start, 1)
    _, after = rest.split(end, 1)

    replacement = (
        start
        + "\n\n"
        + table_md.strip()
        + "\n\n"
        + end
    )

    path.write_text(before + replacement + after, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Board → Drivers → HW markdown table")
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Repository root (defaults to auto-detect based on this script location)",
    )
    parser.add_argument(
        "--update-file",
        default=None,
        help="Update the table between markers in the given markdown file (path relative to repo root)",
    )
    parser.add_argument(
        "--update-drivers-readme",
        action="store_true",
        help="Update src/app/drivers/README.md between markers",
    )
    args = parser.parse_args()

    repo_root = Path(args.repo_root) if args.repo_root else Path(__file__).resolve().parents[1]
    boards_dir = repo_root / "src" / "boards"

    if not boards_dir.is_dir():
        raise SystemExit(f"Boards directory not found: {boards_dir}")

    board_infos: list[BoardInfo] = []

    for board_dir in sorted(p for p in boards_dir.iterdir() if p.is_dir()):
        overrides_path = board_dir / "board_overrides.h"
        if not overrides_path.exists():
            continue
        board_infos.append(parse_board_overrides(board_dir.name, overrides_path))

    table_md = generate_table(board_infos)

    if args.update_drivers_readme and args.update_file:
        raise SystemExit("Use only one of --update-drivers-readme or --update-file")

    if args.update_drivers_readme:
        update_markdown_file(repo_root / "src" / "app" / "drivers" / "README.md", table_md)

    if args.update_file:
        update_markdown_file(repo_root / args.update_file, table_md)

    print(table_md, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
