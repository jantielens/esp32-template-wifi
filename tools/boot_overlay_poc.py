#!/usr/bin/env python3
"""boot_overlay_poc.py

PoC runner to correlate boot-time health-history samples with timestamped serial logs.

What it does:
- Opens a serial port and captures log lines that include "[<ms>ms]" prefix.
- Triggers a reboot via HTTP POST /api/reboot (optional basic auth).
- Continues reading serial and stops when device uptime reaches a target window.
- Fetches /api/health/history and joins log events to the nearest *previous* health sample.
- Writes artifacts:
  - logs.txt (raw serial)
  - history.json (raw /api/health/history response)
  - overlay.csv (joined view for analysis)
    - plot.html (optional: self-contained interactive chart)

Notes:
- This script intentionally joins on device uptime (millis) rather than wall clock.
- Serial ports may disconnect during reboot; the script attempts to reconnect.

Dependencies:
- Python 3.8+
- pyserial (pip install pyserial)
- plotly (optional, for --plot-html) (pip install plotly)

Example:
  python3 tools/boot_overlay_poc.py \
    --port /dev/ttyACM0 \
    --device-url http://192.168.4.1 \
        --window-seconds 15
"""

from __future__ import annotations

import argparse
import base64
import csv
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import urllib.parse
from dataclasses import dataclass
from queue import Empty, Queue
from typing import Any, Dict, List, Optional, Tuple

from bisect import bisect_right


try:
    import serial  # type: ignore
except Exception as e:
    print("ERROR: pyserial is required. Install with: pip install pyserial", file=sys.stderr)
    raise


def _find_arduino_cli() -> Optional[str]:
    # Prefer repo-local install, matching the project scripts.
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    candidate = os.path.join(repo_root, "bin", "arduino-cli")
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate

    # Fallback to PATH.
    which = shutil.which("arduino-cli")
    if which:
        return which
    return None


class _LineSource:
    def readline(self) -> bytes:
        raise NotImplementedError()

    def drain(self) -> None:
        return

    def close(self) -> None:
        return


class _PySerialLineSource(_LineSource):
    def __init__(self, ser: "serial.Serial"):
        self._ser = ser

    def readline(self) -> bytes:
        return self._ser.readline()

    def drain(self) -> None:
        try:
            self._ser.reset_input_buffer()
        except Exception:
            pass

    def close(self) -> None:
        try:
            self._ser.close()
        except Exception:
            pass


class _ArduinoCliMonitorLineSource(_LineSource):
    def __init__(self, *, arduino_cli: str, port: str, baud: int):
        self._arduino_cli = arduino_cli
        self._port = port
        self._baud = baud
        self._proc: Optional[subprocess.Popen[bytes]] = None
        self._q: "Queue[bytes]" = Queue()
        self._reader: Optional[threading.Thread] = None
        self._start()

    def _start(self) -> None:
        # NOTE: Use bytes mode for consistent handling.
        self._proc = subprocess.Popen(
            [
                self._arduino_cli,
                "monitor",
                "-p",
                self._port,
                "-c",
                f"baudrate={self._baud}",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        assert self._proc.stdout is not None

        def _pump() -> None:
            try:
                while True:
                    b = self._proc.stdout.readline()
                    if not b:
                        break
                    self._q.put(b)
            except Exception:
                return

        self._reader = threading.Thread(target=_pump, name="arduino-cli-monitor-reader", daemon=True)
        self._reader.start()

    def readline(self) -> bytes:
        # If the process died, surface that as EOF.
        if self._proc is not None and self._proc.poll() is not None:
            return b""
        try:
            return self._q.get(timeout=0.25)
        except Empty:
            return b""

    def drain(self) -> None:
        try:
            while True:
                self._q.get_nowait()
        except Empty:
            return

    def close(self) -> None:
        if self._proc is not None:
            try:
                self._proc.terminate()
            except Exception:
                pass
            try:
                self._proc.wait(timeout=1.0)
            except Exception:
                pass
        self._proc = None


def _maybe_write_plot_html(
    *,
    out_path: str,
    hist: Dict[str, Any],
    samples: List[HealthSample],
    events: List[LogEvent],
) -> bool:
    """Write a self-contained interactive HTML plot.

    Returns True if written, False if skipped (e.g., missing dependency).
    """

    try:
        import plotly.graph_objects as go  # type: ignore
        from plotly.subplots import make_subplots  # type: ignore
    except Exception:
        print(
            "NOTE: plotly is not installed; skipping plot.html. Install with: pip install plotly",
            file=sys.stderr,
        )
        return False

    if not samples:
        print("NOTE: No samples captured; skipping plot.html", file=sys.stderr)
        return False

    # Prepare data
    samples_sorted = sorted(samples, key=lambda s: s.uptime_ms)
    events_sorted = sorted(events, key=lambda e: e.t_ms)

    x_s = [s.uptime_ms / 1000.0 for s in samples_sorted]
    heap_free_kb = [s.heap_internal_free / 1024.0 for s in samples_sorted]
    heap_largest_kb = [s.heap_internal_largest / 1024.0 for s in samples_sorted]

    # Module coloring for events.
    palette = [
        "#1f77b4",  # blue
        "#ff7f0e",  # orange
        "#2ca02c",  # green
        "#d62728",  # red
        "#9467bd",  # purple
        "#8c564b",  # brown
        "#e377c2",  # pink
        "#7f7f7f",  # gray
        "#bcbd22",  # olive
        "#17becf",  # cyan
    ]

    module_to_color: Dict[str, str] = {}
    next_color = 0

    def color_for_module(module: str) -> str:
        nonlocal next_color
        key = module.strip() or "(none)"
        if key in module_to_color:
            return module_to_color[key]
        module_to_color[key] = palette[next_color % len(palette)]
        next_color += 1
        return module_to_color[key]

    ev_x_s = [e.t_ms / 1000.0 for e in events_sorted]
    ev_y = [0.5 for _ in events_sorted]
    ev_colors = [color_for_module(e.module) for e in events_sorted]
    ev_hover = [f"[{e.t_ms}ms] {e.raw}" for e in events_sorted]

    period_ms = hist.get("period_ms")
    seconds = hist.get("seconds")
    title = f"Boot Overlay (period_ms={period_ms}, seconds={seconds}, samples={len(samples_sorted)}, events={len(events_sorted)})"

    fig = make_subplots(
        rows=2,
        cols=1,
        shared_xaxes=True,
        vertical_spacing=0.06,
        row_heights=[0.82, 0.18],
        subplot_titles=("Internal Heap", "Log Events"),
    )

    fig.add_trace(
        go.Scatter(
            x=x_s,
            y=heap_free_kb,
            mode="lines",
            name="heap_internal_free (KB)",
            line=dict(color="#1f77b4", width=2),
            hovertemplate="t=%{x:.3f}s<br>free=%{y:.1f}KB<extra></extra>",
        ),
        row=1,
        col=1,
    )

    fig.add_trace(
        go.Scatter(
            x=x_s,
            y=heap_largest_kb,
            mode="lines",
            name="heap_internal_largest (KB)",
            line=dict(color="#ff7f0e", width=2, dash="dot"),
            hovertemplate="t=%{x:.3f}s<br>largest=%{y:.1f}KB<extra></extra>",
        ),
        row=1,
        col=1,
    )

    fig.add_trace(
        go.Scatter(
            x=ev_x_s,
            y=ev_y,
            mode="markers",
            name="log events",
            marker=dict(color=ev_colors, size=6, opacity=0.85),
            hovertext=ev_hover,
            hovertemplate="%{hovertext}<extra></extra>",
            showlegend=False,
        ),
        row=2,
        col=1,
    )

    fig.update_xaxes(title_text="Uptime (s)", row=2, col=1)
    fig.update_yaxes(title_text="KB", row=1, col=1)
    # Note: Plotly's built-in axis spikes are per-subplot (especially noticeable in
    # some browsers). We add a single figure-level hover crosshair via a tiny
    # inline JS snippet when writing HTML instead.
    fig.update_yaxes(
        title_text="",
        row=2,
        col=1,
        range=[0, 1],
        showticklabels=False,
        ticks="",
        showgrid=False,
        zeroline=False,
    )

    try:
        # Put a figure-level title above the plotting area (in the top margin),
        # and keep the legend outside of the chart.
        fig.add_annotation(
            x=0,
            y=1.18,
            xref="paper",
            yref="paper",
            xanchor="left",
            yanchor="top",
            text=title,
            showarrow=False,
            font=dict(size=16),
        )

        fig.update_layout(
            hovermode="x unified",
            legend=dict(
                orientation="h",
                yanchor="top",
                y=1.10,
                xanchor="left",
                x=0,
                bgcolor="rgba(255,255,255,0.65)",
            ),
            margin=dict(l=60, r=20, t=160, b=50),
        )

        # Self-contained HTML: inline plotly.js.
        html = fig.to_html(full_html=True, include_plotlyjs="inline", div_id="boot_overlay_plot")

        # Add a figure-spanning dotted crosshair on hover (works reliably in Edge
        # and avoids subplot-specific spikes).
        hover_js = """
<script>
(function(){
  var gd = document.getElementById('boot_overlay_plot');
  if (!gd || typeof Plotly === 'undefined') return;

  function setLine(x){
    var shape = {
      type: 'line',
      xref: 'x',
      yref: 'paper',
      x0: x, x1: x,
      y0: 0, y1: 1,
      line: {color: 'rgba(0,0,0,0.35)', width: 1, dash: 'dot'}
    };
    Plotly.relayout(gd, {shapes: [shape]});
  }

    gd.on('plotly_hover', function(ev){
        try {
            if (!ev || !ev.points || !ev.points.length) return;
            setLine(ev.points[0].x);
        } catch (e) {}
    });

    gd.on('plotly_unhover', function(){
        try { Plotly.relayout(gd, {shapes: []}); } catch (e) {}
    });
})();
</script>
"""

        if "</body>" in html:
            html = html.replace("</body>", hover_js + "\n</body>")
        else:
            html += "\n" + hover_js
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(html)
        return True
    except Exception as e:
        print(f"NOTE: Failed to write plot.html ({type(e).__name__}: {e}); skipping", file=sys.stderr)
        return False


LOG_PREFIX_RE = re.compile(r"^\[(\d+)ms\]\s*(.*)$")
MODULE_RE = re.compile(r"^\s*\[([^\]]+)\]\s*(.*)$")
EMBEDDED_LOG_PREFIX_RE = re.compile(r"\[(\d+)ms\]")


@dataclass
class LogEvent:
    t_ms: int
    module: str
    message: str
    raw: str


@dataclass
class HealthSample:
    uptime_ms: int
    cpu_usage: Optional[int]
    heap_internal_free: int
    heap_internal_largest: int
    psram_free: int


def http_request(
    url: str,
    method: str,
    user: Optional[str] = None,
    password: Optional[str] = None,
    timeout_s: float = 5.0,
) -> bytes:
    req = urllib.request.Request(url=url, method=method)

    if user is not None and password is not None:
        token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
        req.add_header("Authorization", f"Basic {token}")

    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        return resp.read()


def http_post_fire_and_forget(
    url: str,
    user: Optional[str] = None,
    password: Optional[str] = None,
    timeout_s: float = 3.0,
) -> None:
    """Send a POST request and immediately close the connection.

    This is intended for endpoints like /api/reboot where the device may reset
    before sending a complete HTTP response.
    """

    parsed = urllib.parse.urlparse(url)
    if parsed.scheme not in ("http", ""):
        raise ValueError(f"Only http:// URLs are supported for fire-and-forget POSTs: {url}")

    host = parsed.hostname
    if not host:
        raise ValueError(f"Invalid URL (missing host): {url}")

    port = parsed.port or 80
    path = parsed.path or "/"
    if parsed.query:
        path = f"{path}?{parsed.query}"

    headers = [
        f"POST {path} HTTP/1.1",
        f"Host: {host}",
        "Connection: close",
        "Content-Length: 0",
    ]

    if user is not None and password is not None:
        token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
        headers.append(f"Authorization: Basic {token}")

    payload = ("\r\n".join(headers) + "\r\n\r\n").encode("ascii", errors="ignore")

    s: Optional[socket.socket] = None
    try:
        s = socket.create_connection((host, port), timeout=timeout_s)
        s.sendall(payload)
        # Do not wait for response; device may reboot immediately.
    finally:
        if s is not None:
            try:
                s.close()
            except Exception:
                pass


def http_post_reboot(device_url: str, user: Optional[str], password: Optional[str], retries: int = 15) -> None:
    url = device_url.rstrip("/") + "/api/reboot"
    last_err: Optional[BaseException] = None
    for _ in range(retries):
        try:
            http_post_fire_and_forget(url, user=user, password=password, timeout_s=2.0)
            return
        except Exception as e:
            last_err = e
            time.sleep(0.5)
    raise RuntimeError(f"Failed to POST {url}: {last_err}")


def http_get_health_history(device_url: str, user: Optional[str], password: Optional[str], retries: int = 20) -> Dict[str, Any]:
    url = device_url.rstrip("/") + "/api/health/history"
    last_err: Optional[BaseException] = None
    for _ in range(retries):
        try:
            raw = http_request(url, method="GET", user=user, password=password, timeout_s=5.0)
            obj = json.loads(raw.decode("utf-8", errors="replace"))
            if isinstance(obj, dict) and obj.get("available") is True:
                return obj
        except Exception as e:
            last_err = e
        time.sleep(0.5)
    raise RuntimeError(f"Failed to GET {url} (or available=false): {last_err}")


def parse_log_line(line: str) -> Optional[LogEvent]:
    # Some monitors/USB bridges may introduce stray carriage returns.
    # Keep indentation spaces intact; only strip leading '\r'.
    if line.startswith("\r"):
        line = line.lstrip("\r")

    m = LOG_PREFIX_RE.match(line)
    if not m:
        return None

    t_ms = int(m.group(1))
    rest = m.group(2)

    module = ""
    message = rest

    mm = MODULE_RE.match(rest)
    if mm:
        module = mm.group(1)
        message = mm.group(2)

    return LogEvent(t_ms=t_ms, module=module, message=message, raw=line)


def split_embedded_timestamp_lines(text: str) -> List[str]:
        """Split cases where multiple timestamped log prefixes are glued together.

        Example input:
            "[67696ms] ... WiFi: 192.168.1.1[1075ms] [System Boot] Starting..."

        Output:
            ["[67696ms] ... WiFi: 192.168.1.1", "[1075ms] [System Boot] Starting..."]

        Only splits when the first timestamp starts at position 0.
        """

        # First split by actual newlines.
        out: List[str] = []
        for line in text.splitlines():
                matches = list(EMBEDDED_LOG_PREFIX_RE.finditer(line))
                if len(matches) <= 1:
                        out.append(line)
                        continue
                if matches[0].start() != 0:
                        out.append(line)
                        continue

                for i, m in enumerate(matches):
                        start = m.start()
                        end = matches[i + 1].start() if i + 1 < len(matches) else len(line)
                        chunk = line[start:end]
                        if chunk:
                                out.append(chunk)

        return out


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def open_serial_with_retries(port: str, baud: int, timeout_s: float = 0.2, retries: int = 200) -> "serial.Serial":
    last_err: Optional[BaseException] = None
    for _ in range(retries):
        try:
            # On many ESP32 USB-serial adapters, opening the port can toggle DTR/RTS
            # and inadvertently reset the chip (EN/IO0 wiring). To behave more like
            # `arduino-cli monitor` (attach without reboot), set DTR/RTS low *before*
            # opening the port.
            ser = serial.Serial(port=None, baudrate=baud, timeout=timeout_s)
            try:
                ser.dtr = False
                ser.rts = False
            except Exception:
                pass
            ser.port = port
            ser.open()
            return ser
        except Exception as e:
            last_err = e
            time.sleep(0.1)
    raise RuntimeError(f"Failed to open serial port {port}: {last_err}")


def open_line_source(
    *,
    backend: str,
    port: str,
    baud: int,
    timeout_s: float = 0.2,
) -> _LineSource:
    """Open a line source for the serial logs.

    backend:
      - "auto": prefer arduino-cli if available, else pyserial
      - "arduino-cli": use `arduino-cli monitor`
      - "pyserial": use pyserial directly
    """

    if backend not in ("auto", "arduino-cli", "pyserial"):
        raise ValueError(f"Unknown serial backend: {backend}")

    if backend in ("auto", "arduino-cli"):
        arduino_cli = _find_arduino_cli()
        if arduino_cli:
            return _ArduinoCliMonitorLineSource(arduino_cli=arduino_cli, port=port, baud=baud)
        if backend == "arduino-cli":
            raise RuntimeError("arduino-cli not found (expected ./bin/arduino-cli or in PATH)")

    ser = open_serial_with_retries(port=port, baud=baud, timeout_s=timeout_s)
    return _PySerialLineSource(ser)


def read_boot_window_events(
    port: str,
    baud: int,
    window_ms: int,
    out_logs_txt_path: str,
    serial_backend: str = "auto",
    reconnect_grace_s: float = 10.0,
    trigger_reboot_fn=None,
    max_wall_seconds: Optional[float] = None,
) -> List[LogEvent]:
    """Capture log events during a boot window using device uptime.

    Strategy:
    - We assume the device logs prefix lines with [<ms>ms] from millis().
    - After reboot, millis() resets; we use the first seen t_ms as boot start.
    - We stop when t_ms - boot_start_t_ms >= window_ms.

    Reconnect:
    - If serial disconnects mid-capture, attempt to re-open for up to reconnect_grace_s.
    """

    events: List[LogEvent] = []

    # Start with a best-effort open.
    src = open_line_source(backend=serial_backend, port=port, baud=baud)

    triggered_reboot = False

    # If requested, trigger reboot only after serial is open so we capture the earliest boot logs.
    if trigger_reboot_fn is not None:
        # Give `arduino-cli monitor` a brief moment to attach so we don't miss
        # the log line from the reboot handler itself.
        if serial_backend in ("auto", "arduino-cli"):
            time.sleep(0.3)
        src.drain()
        trigger_reboot_fn()
        triggered_reboot = True

    boot_started = False
    boot_start_t_ms: Optional[int] = None
    last_t_ms: Optional[int] = None
    # Prevent a pre-reboot heartbeat (high uptime) from becoming boot start.
    # For our use-case, a "real" boot window should start very early.
    boot_start_max_ms = 3000

    started_wall = time.time()
    # If we never get parsable lines, we still need to stop eventually.
    # Default: window + small slack (logs may become sparse after early boot).
    max_wall = max_wall_seconds
    if max_wall is None:
        max_wall = (window_ms / 1000.0) + 5.0
    deadline_wall = started_wall + max(5.0, float(max_wall))

    raw_lines: List[str] = []

    def flush_raw() -> None:
        if not raw_lines:
            return
        with open(out_logs_txt_path, "a", encoding="utf-8") as f:
            f.writelines(raw_lines)
        raw_lines.clear()

    # Truncate logs file at start.
    with open(out_logs_txt_path, "w", encoding="utf-8") as f:
        f.write("")

    while True:
        if time.time() >= deadline_wall:
            flush_raw()
            break
        try:
            b = src.readline()
            if not b:
                # periodic flush so we don't lose logs on crash
                flush_raw()
                continue

            # arduino-cli and some UART drivers can deliver chunks that include
            # multiple lines or use bare '\r'. Also, occasionally two timestamped
            # log lines get glued together (no newline between). Normalize and
            # process per logical line.
            text = b.decode("utf-8", errors="replace")
            for line in split_embedded_timestamp_lines(text):
                raw_lines.append(line + "\n")

                ev = parse_log_line(line)
                if ev is None:
                    if len(raw_lines) >= 200:
                        flush_raw()
                    continue

                # Detect boot start.
                # - if we haven't started, the first parsed event defines boot_start.
                # - if millis seems to reset (t_ms drops a lot), treat as a new boot.
                if not boot_started:
                    # After we trigger reboot, we may still receive a late line from
                    # before the reset (e.g. heartbeat). Ignore those until we see
                    # early-boot timestamps.
                    if triggered_reboot and ev.t_ms > boot_start_max_ms:
                        last_t_ms = ev.t_ms
                        continue

                    boot_started = True
                    boot_start_t_ms = ev.t_ms
                    last_t_ms = ev.t_ms
                    events.append(ev)
                else:
                    if last_t_ms is not None and ev.t_ms + 5000 < last_t_ms:
                        # New reboot detected. Keep raw logs, but reset parsed events so
                        # overlay uses the latest boot window.
                        boot_start_t_ms = ev.t_ms
                        events.clear()
                    last_t_ms = ev.t_ms
                    events.append(ev)

                if boot_start_t_ms is not None:
                    if ev.t_ms - boot_start_t_ms >= window_ms:
                        flush_raw()
                        break

            if len(raw_lines) >= 200:
                flush_raw()

        except (OSError, serial.SerialException):
            # Port likely reset; try to reconnect.
            flush_raw()
            try_until = time.time() + reconnect_grace_s
            reconnected = False
            while time.time() < try_until:
                try:
                    try:
                        src.close()
                    except Exception:
                        pass
                    src = open_line_source(backend=serial_backend, port=port, baud=baud)
                    reconnected = True
                    break
                except Exception:
                    time.sleep(0.2)
            if not reconnected:
                raise

    src.close()

    return events


def parse_health_history(hist: Dict[str, Any]) -> List[HealthSample]:
    uptime_ms = hist.get("uptime_ms")
    cpu_usage = hist.get("cpu_usage")
    heap_internal_free = hist.get("heap_internal_free")
    heap_internal_largest = hist.get("heap_internal_largest")
    psram_free = hist.get("psram_free")

    if not isinstance(uptime_ms, list):
        raise ValueError("health history missing uptime_ms[]")

    n = len(uptime_ms)

    def arr(name: str) -> List[Any]:
        v = hist.get(name)
        if not isinstance(v, list) or len(v) != n:
            raise ValueError(f"health history missing {name}[] or length mismatch")
        return v

    cpu = hist.get("cpu_usage")
    if not isinstance(cpu, list) or len(cpu) != n:
        cpu = [None] * n

    heap_free = arr("heap_internal_free")
    largest = arr("heap_internal_largest")
    psram = arr("psram_free")

    samples: List[HealthSample] = []
    for i in range(n):
        t = int(uptime_ms[i])
        c = cpu[i]
        c_int: Optional[int]
        if c is None:
            c_int = None
        else:
            try:
                c_int = int(c)
            except Exception:
                c_int = None
        samples.append(
            HealthSample(
                uptime_ms=t,
                cpu_usage=c_int,
                heap_internal_free=int(heap_free[i]),
                heap_internal_largest=int(largest[i]),
                psram_free=int(psram[i]),
            )
        )

    samples.sort(key=lambda s: s.uptime_ms)
    return samples


def join_events_to_samples(events: List[LogEvent], samples: List[HealthSample]) -> List[Tuple[LogEvent, Optional[HealthSample]]]:
    if not samples:
        return [(e, None) for e in events]

    # Two-pointer walk since both are time-ordered enough for PoC.
    samples_idx = 0
    joined: List[Tuple[LogEvent, Optional[HealthSample]]] = []

    for ev in sorted(events, key=lambda e: e.t_ms):
        while samples_idx + 1 < len(samples) and samples[samples_idx + 1].uptime_ms <= ev.t_ms:
            samples_idx += 1

        # previous sample if it exists and is not in the future.
        if samples[samples_idx].uptime_ms <= ev.t_ms:
            joined.append((ev, samples[samples_idx]))
        else:
            joined.append((ev, None))

    return joined


def _fmt_kb(b: Optional[int]) -> str:
    if b is None:
        return "n/a"
    try:
        return f"{int(b) / 1024.0:.1f}KB"
    except Exception:
        return "n/a"


def _event_context_lines(events_sorted: List[LogEvent], t_ms: int, before: int = 2, after: int = 3) -> List[str]:
    if not events_sorted:
        return []
    times = [e.t_ms for e in events_sorted]
    idx = bisect_right(times, t_ms) - 1
    if idx < 0:
        idx = 0
    start = max(0, idx - before)
    end = min(len(events_sorted), idx + after + 1)
    return [events_sorted[i].raw for i in range(start, end)]


def build_boot_summary(
    *,
    hist: Dict[str, Any],
    samples: List[HealthSample],
    events: List[LogEvent],
    window_seconds: float,
    top_n: int = 8,
) -> str:
    period_ms = 0
    try:
        period_ms = int(hist.get("period_ms") or 0)
    except Exception:
        period_ms = 0

    lines: List[str] = []
    lines.append("# Boot Overlay PoC Summary")
    lines.append("")
    lines.append("## Run")
    lines.append("")
    lines.append(f"- Requested capture window: **{window_seconds:.1f}s**")
    lines.append(f"- Health history: `period_ms={period_ms}`, `seconds={hist.get('seconds')}`, `count={len(samples)}`")
    lines.append(f"- Log events parsed: `{len(events)}`")

    if not samples:
        lines.append("")
        lines.append("No samples captured.")
        return "\n".join(lines) + "\n"

    samples_sorted = sorted(samples, key=lambda s: s.uptime_ms)
    events_sorted = sorted(events, key=lambda e: e.t_ms)

    oldest_ms = samples_sorted[0].uptime_ms
    newest_ms = samples_sorted[-1].uptime_ms
    lines.append("")
    lines.append("## Coverage")
    lines.append("")
    lines.append(f"- History window (device uptime): `{oldest_ms}ms -> {newest_ms}ms`")

    # Detect early boot loss.
    if period_ms > 0 and oldest_ms > (period_ms * 10):
        lines.append(f"- **Warning:** oldest history sample starts at `{oldest_ms}ms`; early boot samples were likely overwritten before fetch.")

    heap_vals = [s.heap_internal_free for s in samples_sorted]
    largest_vals = [s.heap_internal_largest for s in samples_sorted]
    lines.append("")
    lines.append("## Memory")
    lines.append("")
    lines.append(f"- Internal heap free range: `{min(heap_vals)}` ({_fmt_kb(min(heap_vals))}) → `{max(heap_vals)}` ({_fmt_kb(max(heap_vals))})")
    lines.append(f"- Largest free block range: `{min(largest_vals)}` ({_fmt_kb(min(largest_vals))}) → `{max(largest_vals)}` ({_fmt_kb(max(largest_vals))})")

    # Biggest drops between consecutive samples.
    drops: List[Tuple[int, int]] = []  # (delta_bytes, index)
    for i in range(1, len(samples_sorted)):
        delta = samples_sorted[i].heap_internal_free - samples_sorted[i - 1].heap_internal_free
        if delta < 0:
            drops.append((delta, i))
    drops.sort(key=lambda x: x[0])  # most negative first

    lines.append("")
    lines.append("## Top Drops")
    lines.append("")
    lines.append("Largest negative deltas between consecutive `heap_internal_free` samples:")
    lines.append("")
    for (delta, i) in drops[: max(1, int(top_n))]:
        t = samples_sorted[i].uptime_ms
        before = samples_sorted[i - 1].heap_internal_free
        after = samples_sorted[i].heap_internal_free
        lines.append(f"- **t={t}ms**: drop `{ -delta }` ({_fmt_kb(-delta)}) — heap `{before}` ({_fmt_kb(before)}) → `{after}` ({_fmt_kb(after)})")
        ctx = _event_context_lines(events_sorted, t)
        if ctx:
            lines.append("")
            lines.append("  Nearest log context:")
            lines.append("")
            lines.append("  ```text")
            for c in ctx:
                lines.append(f"  {c}")
            lines.append("  ```")

    # Minimums.
    min_heap_sample = min(samples_sorted, key=lambda s: s.heap_internal_free)
    min_largest_sample = min(samples_sorted, key=lambda s: s.heap_internal_largest)
    lines.append("")
    lines.append("## Minimums")
    lines.append("")
    lines.append(f"- Min `heap_internal_free`: `{min_heap_sample.heap_internal_free}` ({_fmt_kb(min_heap_sample.heap_internal_free)}) at **{min_heap_sample.uptime_ms}ms**")
    ctx1 = _event_context_lines(events_sorted, min_heap_sample.uptime_ms)
    if ctx1:
        lines.append("")
        lines.append("  ```text")
        for c in ctx1:
            lines.append(f"  {c}")
        lines.append("  ```")

    lines.append("")
    lines.append(f"- Min `heap_internal_largest`: `{min_largest_sample.heap_internal_largest}` ({_fmt_kb(min_largest_sample.heap_internal_largest)}) at **{min_largest_sample.uptime_ms}ms**")
    ctx2 = _event_context_lines(events_sorted, min_largest_sample.uptime_ms)
    if ctx2:
        lines.append("")
        lines.append("  ```text")
        for c in ctx2:
            lines.append(f"  {c}")
        lines.append("  ```")

    return "\n".join(lines) + "\n"


def write_overlay_csv(path: str, joined: List[Tuple[LogEvent, Optional[HealthSample]]]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "log_t_ms",
                "sample_uptime_ms",
                "heap_internal_free",
                "heap_internal_largest",
                "psram_free",
                "cpu_usage",
                "module",
                "message",
            ]
        )
        for ev, s in joined:
            if s is None:
                w.writerow([ev.t_ms, "", "", "", "", "", ev.module, ev.message])
            else:
                w.writerow(
                    [
                        ev.t_ms,
                        s.uptime_ms,
                        s.heap_internal_free,
                        s.heap_internal_largest,
                        s.psram_free,
                        "" if s.cpu_usage is None else s.cpu_usage,
                        ev.module,
                        ev.message,
                    ]
                )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument(
        "--serial-backend",
        choices=["auto", "arduino-cli", "pyserial"],
        default="auto",
        help="Serial capture backend. 'auto' prefers arduino-cli monitor when available.",
    )
    ap.add_argument("--device-url", required=True, help="Device base URL, e.g. http://192.168.4.1")
    ap.add_argument("--user", default=None)
    ap.add_argument("--password", default=None)
    # Default to a tighter window than the device-side history buffer to reduce
    # the chance that early boot samples are overwritten before we fetch history.
    ap.add_argument("--window-seconds", type=float, default=15.0)
    ap.add_argument("--out-dir", default=None, help="Output directory (defaults to ./build/boot-overlay-poc/<timestamp>)")
    ap.add_argument("--no-summary", action="store_true", help="Do not write summary.md")
    ap.add_argument(
        "--plot-html",
        dest="plot_html",
        action="store_true",
        help="Write an interactive self-contained plot.html (requires plotly) (default)",
    )
    ap.add_argument(
        "--no-plot-html",
        dest="plot_html",
        action="store_false",
        help="Disable writing plot.html",
    )
    ap.set_defaults(plot_html=True)

    args = ap.parse_args()

    window_ms = int(max(1.0, args.window_seconds) * 1000.0)

    ts = time.strftime("%Y%m%d-%H%M%S")
    out_dir = args.out_dir or os.path.join("build", "boot-overlay-poc", ts)
    ensure_dir(out_dir)

    logs_path = os.path.join(out_dir, "logs.txt")
    hist_path = os.path.join(out_dir, "history.json")
    overlay_path = os.path.join(out_dir, "overlay.csv")
    summary_path = os.path.join(out_dir, "summary.md")
    plot_path = os.path.join(out_dir, "plot.html")

    print(f"Output: {out_dir}")
    print(f"Capturing serial for ~{args.window_seconds:.1f}s device uptime...")
    events = read_boot_window_events(
        port=args.port,
        baud=args.baud,
        window_ms=window_ms,
        serial_backend=str(args.serial_backend),
        out_logs_txt_path=logs_path,
        trigger_reboot_fn=lambda: http_post_reboot(args.device_url, args.user, args.password),
        # Keep this tight so we don't overrun the device-side history ring buffer.
        max_wall_seconds=float(args.window_seconds) + 2.0,
    )

    print("Fetching /api/health/history...")
    hist = http_get_health_history(args.device_url, args.user, args.password)

    with open(hist_path, "w", encoding="utf-8") as f:
        json.dump(hist, f, indent=2, sort_keys=True)

    samples = parse_health_history(hist)

    # Detect whether we likely missed early boot samples due to fetching too late
    # relative to the device-side history window.
    if samples:
        oldest_ms = samples[0].uptime_ms
        p = hist.get("period_ms")
        try:
            period_ms = int(p) if p is not None else 0
        except Exception:
            period_ms = 0
        # If history starts well after boot (> ~10 samples), we've probably overwritten early boot.
        if period_ms > 0 and oldest_ms > (period_ms * 10):
            print(
                f"WARNING: oldest history sample starts at {oldest_ms}ms. "
                "This likely means early boot samples were overwritten before fetch. "
                "Re-run sooner (tighter capture) or temporarily increase HEALTH_HISTORY_SECONDS."
            )

    joined = join_events_to_samples(events, samples)
    write_overlay_csv(overlay_path, joined)

    if not args.no_summary:
        summary = build_boot_summary(hist=hist, samples=samples, events=events, window_seconds=float(args.window_seconds))
        with open(summary_path, "w", encoding="utf-8") as f:
            f.write(summary)
        print("\n" + summary.rstrip("\n"))

    if args.plot_html:
        wrote = _maybe_write_plot_html(out_path=plot_path, hist=hist, samples=samples, events=events)
        if wrote:
            print(f"Wrote: {plot_path}")

    # If the markdown summary is disabled, still print a minimal console summary.
    if args.no_summary:
        if samples:
            min_free = min(samples, key=lambda s: s.heap_internal_free)
            print(f"Samples: {len(samples)} (period_ms={hist.get('period_ms')})")
            print(f"Min internal free heap: {min_free.heap_internal_free} at {min_free.uptime_ms}ms")
        print(f"Log events parsed: {len(events)}")
    print(f"Wrote: {logs_path}")
    print(f"Wrote: {hist_path}")
    print(f"Wrote: {overlay_path}")
    if not args.no_summary:
        print(f"Wrote: {summary_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
