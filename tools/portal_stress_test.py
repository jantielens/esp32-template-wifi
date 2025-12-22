#!/usr/bin/env python3
"""Portal/API stress test for ESP32 template.

Goal: produce repeatable memory/fragmentation measurements without relying on serial logs.

This script is intentionally dependency-free (stdlib only).

Typical usage:
  python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario api
  python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario portal

Scenarios:
- api:    hits API endpoints only (JSON churn focus)
- portal: fetches HTML/CSS/JS pages in addition to API calls (portal load)

Notes:
- Requires --no-reboot and always uses ?no_reboot=1 when saving config.
- Uses /api/health as the single source of metrics.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Dict, Optional, Tuple

from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


DEFAULT_TIMEOUT_S = 5.0
DEFAULT_RETRIES = 3
DEFAULT_RETRY_SLEEP_S = 0.25


@dataclass
class HealthSample:
    ts_iso: str
    cycle: int
    phase: str
    heap_free: Optional[int]
    heap_min: Optional[int]
    heap_largest: Optional[int]
    heap_internal_free: Optional[int]
    heap_internal_min: Optional[int]
    heap_fragmentation: Optional[int]


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _normalize_base_url(host: str) -> str:
    host = host.strip()
    if host.startswith("http://") or host.startswith("https://"):
        return host.rstrip("/")
    return f"http://{host.rstrip('/')}"


def _http(
    base_url: str,
    method: str,
    path: str,
    body: Optional[bytes] = None,
    content_type: Optional[str] = None,
    timeout_s: float = DEFAULT_TIMEOUT_S,
    accept: Optional[str] = None,
    retries: int = DEFAULT_RETRIES,
    retry_sleep_s: float = DEFAULT_RETRY_SLEEP_S,
) -> Tuple[int, bytes]:
    url = f"{base_url}{path}"
    headers = {
        "User-Agent": "esp32-template-portal-stress-test/1.0",
    }
    if accept:
        headers["Accept"] = accept
    if content_type:
        headers["Content-Type"] = content_type

    req = Request(url=url, data=body, headers=headers, method=method)

    last_err: Optional[BaseException] = None
    for attempt in range(1, max(1, retries) + 1):
        try:
            with urlopen(req, timeout=timeout_s) as resp:
                return resp.getcode(), resp.read()
        except HTTPError as e:
            # HTTPError is a valid response; keep body if present.
            try:
                data = e.read()
            except Exception:
                data = b""
            return e.code, data
        except URLError as e:
            last_err = e
            if attempt < retries:
                time.sleep(retry_sleep_s * attempt)
                continue
            break
        except TimeoutError as e:
            last_err = e
            if attempt < retries:
                time.sleep(retry_sleep_s * attempt)
                continue
            break

    raise RuntimeError(f"Request failed: {url} ({last_err})")


def get_health(base_url: str, timeout_s: float, retries: int, retry_sleep_s: float) -> Dict[str, Any]:
    status, data = _http(
        base_url,
        method="GET",
        path="/api/health",
        timeout_s=timeout_s,
        accept="application/json",
        retries=retries,
        retry_sleep_s=retry_sleep_s,
    )
    if status != 200:
        raise RuntimeError(f"GET /api/health failed: HTTP {status} body={data[:200]!r}")
    try:
        return json.loads(data.decode("utf-8", errors="replace"))
    except Exception as e:
        raise RuntimeError(f"Failed to parse /api/health JSON: {e}")


def get_config(base_url: str, timeout_s: float, retries: int, retry_sleep_s: float) -> Dict[str, Any]:
    status, data = _http(
        base_url,
        method="GET",
        path="/api/config",
        timeout_s=timeout_s,
        accept="application/json",
        retries=retries,
        retry_sleep_s=retry_sleep_s,
    )
    if status != 200:
        raise RuntimeError(f"GET /api/config failed: HTTP {status} body={data[:200]!r}")
    try:
        return json.loads(data.decode("utf-8", errors="replace"))
    except Exception as e:
        raise RuntimeError(f"Failed to parse /api/config JSON: {e}")


def post_config_no_reboot(base_url: str, payload: Dict[str, Any], timeout_s: float, retries: int, retry_sleep_s: float) -> None:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    status, data = _http(
        base_url,
        method="POST",
        path="/api/config?no_reboot=1",
        body=body,
        content_type="application/json",
        timeout_s=timeout_s,
        accept="application/json",
        retries=retries,
        retry_sleep_s=retry_sleep_s,
    )
    if status != 200:
        raise RuntimeError(f"POST /api/config failed: HTTP {status} body={data[:200]!r}")


def set_screen(base_url: str, screen_id: str, timeout_s: float, retries: int, retry_sleep_s: float) -> None:
    body = json.dumps({"screen": screen_id}, separators=(",", ":")).encode("utf-8")
    status, data = _http(
        base_url,
        method="PUT",
        path="/api/display/screen",
        body=body,
        content_type="application/json",
        timeout_s=timeout_s,
        accept="application/json",
        retries=retries,
        retry_sleep_s=retry_sleep_s,
    )
    if status != 200:
        raise RuntimeError(
            f"PUT /api/display/screen ({screen_id}) failed: HTTP {status} body={data[:200]!r}"
        )


def fetch_portal_assets(base_url: str, timeout_s: float, retries: int, retry_sleep_s: float) -> None:
    # We don't need to parse/decompress; we just want the server to do the work.
    for path in ("/network.html", "/home.html", "/firmware.html", "/portal.css", "/portal.js"):
        status, _ = _http(
            base_url,
            method="GET",
            path=path,
            timeout_s=timeout_s,
            retries=retries,
            retry_sleep_s=retry_sleep_s,
        )
        # In AP mode, home/firmware may redirect; accept 200/302.
        if status not in (200, 302):
            raise RuntimeError(f"GET {path} failed: HTTP {status}")


def extract_sample(cycle: int, phase: str, health: Dict[str, Any]) -> HealthSample:
    def _int_or_none(key: str) -> Optional[int]:
        v = health.get(key)
        if isinstance(v, bool) or v is None:
            return None
        try:
            return int(v)
        except Exception:
            return None

    return HealthSample(
        ts_iso=_now_iso(),
        cycle=cycle,
        phase=phase,
        heap_free=_int_or_none("heap_free"),
        heap_min=_int_or_none("heap_min"),
        heap_largest=_int_or_none("heap_largest"),
        heap_internal_free=_int_or_none("heap_internal_free"),
        heap_internal_min=_int_or_none("heap_internal_min"),
        heap_fragmentation=_int_or_none("heap_fragmentation"),
    )


def summarize(samples: list[HealthSample]) -> None:
    if not samples:
        return

    def _series(field: str) -> list[int]:
        vals = [getattr(s, field) for s in samples]
        return [v for v in vals if isinstance(v, int)]

    def _minmax(vals: list[int]) -> str:
        if not vals:
            return "n/a"
        return f"min={min(vals)} max={max(vals)}"

    print("\nSummary (across all collected /api/health samples):")
    for field in (
        "heap_free",
        "heap_min",
        "heap_largest",
        "heap_internal_free",
        "heap_internal_min",
        "heap_fragmentation",
    ):
        vals = _series(field)
        print(f"  {field:18s} {_minmax(vals)}")


def write_csv(path: str, samples: list[HealthSample]) -> None:
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "ts_iso",
                "cycle",
                "phase",
                "heap_free",
                "heap_min",
                "heap_largest",
                "heap_internal_free",
                "heap_internal_min",
                "heap_fragmentation",
            ]
        )
        for s in samples:
            w.writerow(
                [
                    s.ts_iso,
                    s.cycle,
                    s.phase,
                    s.heap_free,
                    s.heap_min,
                    s.heap_largest,
                    s.heap_internal_free,
                    s.heap_internal_min,
                    s.heap_fragmentation,
                ]
            )


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description="ESP32 portal/API stress test (health API driven)")
    p.add_argument("--host", required=True, help="Device IP address (or full URL). Example: 192.168.1.111")

    # Safety: explicit opt-in.
    p.add_argument(
        "--no-reboot",
        action="store_true",
        help="Required safety flag: script will use /api/config?no_reboot=1 and avoid reboot loops.",
    )

    p.add_argument("--cycles", type=int, default=10, help="Number of stress cycles (default: 10)")
    p.add_argument(
        "--scenario",
        choices=("api", "portal", "both"),
        default="api",
        help="Stress scenario: api (JSON churn), portal (fetch pages+assets), both (portal + api).",
    )
    p.add_argument("--sleep", type=float, default=0.5, help="Sleep between steps in seconds (default: 0.5)")
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_S, help="HTTP timeout seconds (default: 5)")
    p.add_argument("--retries", type=int, default=DEFAULT_RETRIES, help="Retries per request (default: 3)")
    p.add_argument("--retry-sleep", type=float, default=DEFAULT_RETRY_SLEEP_S, help="Base sleep between retries in seconds (default: 0.25)")
    p.add_argument(
        "--out",
        default=None,
        help="Optional CSV output path (writes all samples).",
    )

    args = p.parse_args(argv)

    if not args.no_reboot:
        print("ERROR: --no-reboot is required for safety.", file=sys.stderr)
        return 2

    if args.cycles < 1:
        print("ERROR: --cycles must be >= 1", file=sys.stderr)
        return 2

    base_url = _normalize_base_url(args.host)

    print(f"Target: {base_url}")
    print(f"Scenario: {args.scenario}")
    print(f"Cycles: {args.cycles}")

    samples: list[HealthSample] = []

    try:
        # Baseline samples
        health0 = get_health(base_url, timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
        samples.append(extract_sample(cycle=0, phase="baseline", health=health0))

        # Capture a baseline config to re-post (so we stress JSON + NVS without changing the device).
        cfg = get_config(base_url, timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
    except Exception as e:
        print(f"ERROR: Failed to capture baseline: {e}", file=sys.stderr)
        return 1

    # Only include fields the backend understands and that are safe to re-send.
    config_payload: Dict[str, Any] = {
        "wifi_ssid": cfg.get("wifi_ssid", ""),
        # Passwords: keep empty so backend doesn't overwrite.
        "wifi_password": "",
        "device_name": cfg.get("device_name", ""),
        "fixed_ip": cfg.get("fixed_ip", ""),
        "subnet_mask": cfg.get("subnet_mask", ""),
        "gateway": cfg.get("gateway", ""),
        "dns1": cfg.get("dns1", ""),
        "dns2": cfg.get("dns2", ""),
        "dummy_setting": cfg.get("dummy_setting", ""),
        "mqtt_host": cfg.get("mqtt_host", ""),
        "mqtt_port": cfg.get("mqtt_port", 0),
        "mqtt_username": cfg.get("mqtt_username", ""),
        "mqtt_password": "",
        "mqtt_interval_seconds": cfg.get("mqtt_interval_seconds", 0),
        "backlight_brightness": cfg.get("backlight_brightness", 100),
    }

    try:
        for i in range(1, args.cycles + 1):
            if args.scenario in ("portal", "both"):
                fetch_portal_assets(base_url, timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
                time.sleep(args.sleep)

            # Stress JSON parse + NVS save + response handling
            post_config_no_reboot(base_url, config_payload, timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
            time.sleep(args.sleep)

            # Stress screen change JSON parse + response
            try:
                set_screen(base_url, "test", timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
                time.sleep(args.sleep)
                set_screen(base_url, "info", timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
                time.sleep(args.sleep)
            except RuntimeError:
                # Boards without display will 404; that's fine for "api" scenario.
                pass

            # Take a sample per cycle
            health = get_health(base_url, timeout_s=args.timeout, retries=args.retries, retry_sleep_s=args.retry_sleep)
            samples.append(extract_sample(cycle=i, phase="after_cycle", health=health))

            # Light progress output
            hl = samples[-1].heap_largest
            frag = samples[-1].heap_fragmentation
            print(f"cycle {i:3d}/{args.cycles}: heap_largest={hl} heap_fragmentation={frag}")
    finally:
        summarize(samples)

    if args.out:
        write_csv(args.out, samples)
        print(f"\nWrote CSV: {args.out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
