"""FastAPI web interface for CAEN HV control and DT5743 measurement batching.

Provides endpoints:
- POST /hv/set {value,...} : set HV (VSET)
- GET /hv/read : current HV (VMON)
- POST /hv/send {cmd,par,val?,channel?,device?...} : raw command
- WebSocket /ws/hv?interval=2 : live HV monitoring

Measurement control:
- POST /measure/start {yaml, data_output, exe, batch_mode, max_events, max_time, trigger_threshold, sampling_frequency, channel_thresholds, hv_sequence, thresholds, repeat, loop}
- POST /measure/stop/{id}
- GET /measure/status : list active measurements
- WebSocket /ws/measure/{id} : live progress (events, rate, elapsed, hv, threshold)

Design notes:
- Uses subprocess to invoke dt5743_runner for each measurement configuration.
- HV set performed before each run when hv value specified.
- Progress parsing is heuristic: looks for integers in lines containing 'event' or 'Events'.
- Event rate computed as events / elapsed_seconds.
- For long-running loops, user can stop via /measure/stop/{id}.

This is a simple starting point; refine parsing or persistence as needed.
"""
from __future__ import annotations
import time
import uuid
import threading
import subprocess
import sys
import re
from typing import Dict, List, Optional, Any
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, BackgroundTasks, HTTPException, Query
from fastapi.responses import JSONResponse, HTMLResponse, Response
from pydantic import BaseModel, Field

from .caen_hv import send_caen_command

app = FastAPI(title="Digitizer Web Interface", version="0.1.0")

# ---------------------- HV CONTROL MODELS ----------------------
class HVSetRequest(BaseModel):
    value: float = Field(..., description="HV set value (positive number; will be converted to negative if needed)")
    channel: str = Field("1", description="HV channel")
    device: str = Field("COM10", description="Serial device")
    baudrate: int = 9600
    timeout: float = 1.0

class HVSendRequest(BaseModel):
    cmd: str
    par: str
    val: Optional[float] = None
    channel: str = "1"
    device: str = "COM10"
    baudrate: int = 9600
    timeout: float = 1.0

# ---------------------- MEASUREMENT MODELS ----------------------
class MeasureStartRequest(BaseModel):
    yaml: str
    data_output: str = Field("./data_output")
    exe: str = Field("WaveDemo_x743.exe")
    batch_mode: int = 2
    max_events: int = 0
    max_time: int = 0
    trigger_threshold: Optional[float] = None
    sampling_frequency: Optional[int] = Field(None, description="Enum value 0-3")
    channel_thresholds: Optional[str] = Field(None, description="Comma separated BOARD:CHANNEL=VALUE")
    hv_sequence: Optional[List[float]] = Field(None, description="List of HV values to iterate")
    thresholds: Optional[List[float]] = Field(None, description="List of trigger thresholds to iterate")
    repeat: Optional[int] = Field(None, description="Number of repeats for the full HV/threshold sweep; None = single, -1 = infinite loop")
    loop: bool = Field(False, description="If True, treat repeat None as 1; use repeat=-1 for infinite loop")
    hv_device: Optional[str] = None
    hv_baudrate: Optional[int] = None
    hv_timeout: Optional[float] = None
    hv_channel: Optional[int] = None

class MeasureStatus(BaseModel):
    id: str
    running: bool
    current_hv: Optional[float]
    current_threshold: Optional[float]
    start_time: float
    elapsed: float
    events: int
    rate: float
    iteration: int
    total_iterations: Optional[int]
    repeat_index: int
    repeat_total: Optional[int]
    last_line: Optional[str]
    progress_line: Optional[str] = None

# ---------------------- RUNTIME STATE ----------------------
class MeasurementTask:
    def __init__(self, req: MeasureStartRequest):
        self.req = req
        self.id = uuid.uuid4().hex
        self.start_time = time.time()
        self.events = 0
        self.rate = 0.0
        self.current_hv: Optional[float] = None
        self.current_threshold: Optional[float] = None
        self.running = True
        self.iteration = 0
        self.total_iterations: Optional[int] = None
        self.repeat_index = 0
        self.repeat_total: Optional[int] = None
        self.last_line: Optional[str] = None
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.lock = threading.Lock()
        self.proc: Optional[subprocess.Popen] = None
        self.thread.start()

    def build_runner_cmd(self, hv: Optional[float], threshold: Optional[float]) -> List[str]:
        py = sys.executable
        cmd = [py, '-m', 'd3df_single_pmt.dt5743_runner', '--yaml', self.req.yaml, '--data-output', self.req.data_output, '--exe', self.req.exe, '--batch-mode', str(self.req.batch_mode), '--max-events', str(self.req.max_events), '--max-time', str(self.req.max_time)]
        if threshold is not None:
            cmd += ['--trigger-threshold', str(threshold)]
        if self.req.channel_thresholds:
            cmd += ['--channel-thresholds', self.req.channel_thresholds]
        if hv is not None:
            cmd += ['--set-hv', str(hv)]
        if self.req.hv_device:
            cmd += ['--hv-device', self.req.hv_device]
        if self.req.hv_baudrate:
            cmd += ['--hv-baudrate', str(self.req.hv_baudrate)]
        if self.req.hv_timeout is not None:
            cmd += ['--hv-timeout', str(self.req.hv_timeout)]
        if self.req.hv_channel is not None:
            cmd += ['--hv-channel', str(self.req.hv_channel)]
        return cmd

    def compute_plan(self):
        hv_seq = self.req.hv_sequence or [None]
        thr_seq = self.req.thresholds or [None]
        iterations = [(h, t) for h in hv_seq for t in thr_seq]
        self.total_iterations = len(iterations)
        if self.req.repeat is not None and self.req.repeat >= 0:
            self.repeat_total = self.req.repeat if self.req.repeat > 0 else 1
        elif self.req.repeat == -1:
            self.repeat_total = None  # Infinite
        else:
            self.repeat_total = 1 if self.req.loop else 1
        return iterations

    def run_single(self, hv: Optional[float], threshold: Optional[float]):
        self.current_hv = hv
        self.current_threshold = threshold
        cmd = self.build_runner_cmd(hv, threshold)
        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE,
            universal_newlines=True,
            bufsize=1,
        )
        start = time.time()
        for line in self.proc.stdout:  # type: ignore
            with self.lock:
                self.last_line = line.rstrip()
                
                # Parse "Batch mode progress: 10/30 seconds, 107 events"
                batch_match = re.search(r'Batch mode progress:\s*(\d+)/(\d+)\s*seconds,\s*(\d+)\s*events?', line, re.IGNORECASE)
                if batch_match:
                    elapsed_sec = int(batch_match.group(1))
                    max_sec = int(batch_match.group(2))
                    events = int(batch_match.group(3))
                    self.events = events
                    if elapsed_sec > 0:
                        self.rate = events / elapsed_sec
                    continue
                
                # Parse throughput line: "  0  0  |    9.44 Hz  100.00%   0.00%        320          9"
                throughput_match = re.search(r'\|\s*([\d.]+)\s*Hz\s+[\d.]+%\s+[\d.]+%\s+(\d+)', line)
                if throughput_match:
                    rate_hz = float(throughput_match.group(1))
                    total_events = int(throughput_match.group(2))
                    self.events = total_events
                    self.rate = rate_hz
                    continue
                
                # Removed fallback generic event parsing
                
                elapsed = time.time() - self.start_time
                if elapsed > 0 and self.events > 0:
                    self.rate = self.events / elapsed
            # Optional early stop check
            if not self.running:
                break
        try:
            self.proc.wait(timeout=5)
        except Exception:
            pass
        self.proc = None

    def run_loop(self):
        iterations = self.compute_plan()
        repeat_index = 0
        while self.running:
            for (hv, thr) in iterations:
                if not self.running:
                    break
                self.iteration += 1
                self.run_single(hv, thr)
            repeat_index += 1
            self.repeat_index = repeat_index
            if self.repeat_total is not None and repeat_index >= self.repeat_total:
                break
        self.running = False

    def stop(self):
        with self.lock:
            self.running = False
            if self.proc and self.proc.poll() is None:
                try:
                    # Attempt graceful shutdown: send "q\nq\n" and wait a moment
                    if self.proc.stdin:
                        try:
                            self.proc.stdin.write("q\nq\n")
                            self.proc.stdin.flush()
                        except Exception:
                            pass
                    time.sleep(1.0)
                    # If still running, terminate
                    if self.proc.poll() is None:
                        self.proc.terminate()
                except Exception:
                    pass

    def snapshot(self) -> MeasureStatus:
        with self.lock:
            # Build progress string like: "Batch mode progress: 20/30 seconds, 110 events"
            prog = None
            if self.req.max_time and self.req.max_time > 0:
                elapsed_sec = int(time.time() - self.start_time)
                remaining_sec = max(self.req.max_time - elapsed_sec, 0)
                rate_val = self.rate if self.rate else 0.0
                prog = (
                    f"Batch progress: elapsed {elapsed_sec}s, remaining {remaining_sec}s, "
                    f"events {self.events}, rate {rate_val:.2f} 1/s"
                )
            return MeasureStatus(
                id=self.id,
                running=self.running,
                current_hv=self.current_hv,
                current_threshold=self.current_threshold,
                start_time=self.start_time,
                elapsed=time.time() - self.start_time,
                events=self.events,
                rate=self.rate,
                iteration=self.iteration,
                total_iterations=self.total_iterations,
                repeat_index=self.repeat_index,
                repeat_total=self.repeat_total,
                last_line=self.last_line,
                progress_line=prog,
            )

measurements: Dict[str, MeasurementTask] = {}

# ---------------------- HV ENDPOINTS ----------------------
@app.post('/hv/set')
def hv_set(req: HVSetRequest):
    val = req.value
    try:
        resp = send_caen_command('SET', 'VSET', str(val), channel=req.channel, device=req.device, baudrate=req.baudrate, timeout=req.timeout)
        return {'status': 'ok', 'response': resp, 'hv_set': val}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get('/hv/read')
def hv_read(channel: str = Query('1'), device: str = Query('COM10'), baudrate: int = Query(9600), timeout: float = Query(1.0)):
    try:
        resp = send_caen_command('MON', 'VMON', channel=channel, device=device, baudrate=baudrate, timeout=timeout)
        return {'status': 'ok', 'hv': resp}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post('/hv/send')
def hv_send(req: HVSendRequest):
    try:
        resp = send_caen_command(req.cmd.upper(), req.par.upper(), None if req.val is None else str(req.val), channel=req.channel, device=req.device, baudrate=req.baudrate, timeout=req.timeout)
        return {'status': 'ok', 'response': resp}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# ---------------------- MEASUREMENT ENDPOINTS ----------------------
@app.post('/measure/start')
def measure_start(req: MeasureStartRequest):
    task = MeasurementTask(req)
    measurements[task.id] = task
    return {'status': 'started', 'id': task.id}

@app.post('/measure/stop/{mid}')
def measure_stop(mid: str):
    task = measurements.get(mid)
    if not task:
        raise HTTPException(status_code=404, detail='Measurement not found')
    task.stop()
    return {'status': 'stopping', 'id': mid}

@app.get('/measure/status')
def measure_status():
    return {'measurements': [m.snapshot().dict() for m in measurements.values()]}

# ---------------------- WEBSOCKETS ----------------------
@app.websocket('/ws/hv')
async def ws_hv(ws: WebSocket, interval: float = 2.0, channel: str = '1', device: str = 'COM10', baudrate: int = 9600, timeout: float = 1.0):
    await ws.accept()
    try:
        while True:
            try:
                hv = send_caen_command('MON', 'VMON', channel=channel, device=device, baudrate=baudrate, timeout=timeout)
            except Exception as e:
                hv = ''
                await ws.send_json({'error': str(e)})
            await ws.send_json({'ts': time.time(), 'hv': hv})
            await asyncio_sleep(interval)
    except WebSocketDisconnect:
        return

@app.websocket('/ws/measure/{mid}')
async def ws_measure(ws: WebSocket, mid: str):
    await ws.accept()
    task = measurements.get(mid)
    if not task:
        await ws.send_json({'error': 'Measurement not found'})
        await ws.close()
        return
    try:
        while True:
            snap = task.snapshot()
            await ws.send_json(snap.dict())
            if not snap.running:
                break
            await asyncio_sleep(1.0)
    except WebSocketDisconnect:
        return
    finally:
        await ws.close()

# ---------------------- UTILS ----------------------
def asyncio_sleep(seconds: float):
    # Lightweight fallback without importing asyncio at module top (avoid event loop confusion if run in thread)
    import asyncio
    return asyncio.sleep(seconds)

# ---------------------- SIMPLE UI ----------------------
INDEX_HTML = """<!doctype html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width,initial-scale=1'/><title>Digitizer Web Interface</title><script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js'></script><style>body{font-family:system-ui,Arial,sans-serif;margin:16px}h2{margin-top:24px}.row{display:flex;flex-wrap:wrap;gap:16px}.col{flex:1 1 400px;min-width:360px}fieldset{border:1px solid #ccc;padding:12px;border-radius:8px}label{display:block;margin-top:6px;font-size:.9rem}input,textarea{width:100%;padding:6px;margin-top:4px}button{margin-top:8px;padding:6px 10px}code{background:#f4f4f4;padding:2px 4px;border-radius:4px}canvas{max-height:320px}#log{white-space:pre-wrap;max-height:160px;overflow:auto;background:#111;color:#ddd;padding:8px}.status-badge{display:inline-block;padding:2px 6px;border-radius:4px;font-size:.75rem;font-weight:600}#hv_status.connected{background:#2a6;color:#fff}#hv_status.disconnected{background:#c22;color:#fff}.progress-wrap{margin-top:8px}.progress-labels{display:flex;justify-content:space-between;font-size:.85rem;margin-bottom:4px}.progress-bar{width:100%;height:12px;background:#eee;border-radius:6px;overflow:hidden}.progress-bar>div{height:100%;background:#26c;width:0%}</style></head><body><h1>Digitizer Web Interface</h1><div class='row'><div class='col'><fieldset><legend>HV Control</legend><label>Device <input id='hv_device' value='COM10'/></label><label>Channel <input id='hv_channel' value='1'/></label><label>Baudrate <input id='hv_baud' type='number' value='9600'/></label><div style='display:flex;gap:8px;align-items:end'><div style='flex:1'><label>Set HV (V) <input id='hv_value' type='number' step='1' value='1800'/></label></div><div><button id='btn_hv_set' type='button'>Set HV</button><button id='btn_hv_read' type='button'>Read HV</button></div></div><div><label>Raw HV Command</label><div style='display:flex;gap:8px'><input id='hv_cmd' value='MON' style='max-width:80px'/><input id='hv_par' value='VMON' style='max-width:120px'/><input id='hv_val' placeholder='val (optional)' style='max-width:140px'/><button id='btn_hv_send' type='button'>Send</button></div></div><div style='display:flex;gap:8px;align-items:end;margin-top:8px'><div style='flex:1'><label>Monitor interval (s)<input id='hv_interval' type='number' step='0.1' value='2'/></label></div><button id='btn_hv_toggle' type='button'>Start Monitoring</button><span id='hv_status' class='status-badge disconnected'>OFF</span></div><div id='hv_result' style='margin-top:8px'>Result: <code>(none)</code></div></fieldset></div><div class='col'><fieldset><legend>Measurement Control</legend><label>YAML path <input id='m_yaml' value='config.yaml'/></label><label>Data output <input id='m_out' value='./data_output'/></label><label>WaveDemo exe <input id='m_exe' value='WaveDemo_x743.exe'/></label><label>Batch mode <input id='m_batch' type='number' value='2'/></label><label>Max events <input id='m_maxev' type='number' value='0'/></label><label>Max time (s) <input id='m_maxt' type='number' value='30'/></label><label>HV sequence (comma-separated) <input id='m_hvseq' placeholder='1800,1700'/></label><label>Thresholds (comma-separated) <input id='m_thrseq' placeholder='-0.10,-0.20'/></label><label>Repeat (-1=infinite, empty=1) <input id='m_repeat' placeholder='1'/></label><div style='display:flex;gap:8px;align-items:end'><button id='btn_m_start' type='button'>Start</button><button id='btn_m_stop' type='button' disabled>Stop</button><div>Current ID: <code id='m_id'>(none)</code></div></div></fieldset></div></div><h2>Live Monitoring</h2><div class='row'><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>HV Plot</span><button id='btn_clear_hv' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_hv'></canvas></div><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>Events Plot</span><button id='btn_clear_events' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_events'></canvas></div></div><div class='row'><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>Rate Plot</span><button id='btn_clear_rate' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_rate'></canvas></div><div class='col'><fieldset><legend>Progress</legend><div class='progress-wrap'><div class='progress-labels'><span>Elapsed: <span id='prog_elapsed'>0s</span></span><span>Remaining: <span id='prog_remaining'>0s</span></span></div><div class='progress-bar'><div id='prog_bar'></div></div></div></fieldset><fieldset style='margin-top:12px'><legend>Log</legend><div id='log'></div></fieldset></div></div><script src='/static/app.js'></script></body></html>"""

@app.get('/static/app.js')
def static_app_js():
        # Serve external JS file content from static folder.
        import os
        js_path = os.path.join(os.path.dirname(__file__), 'static', 'app.js')
        if not os.path.exists(js_path):
                return Response("console.error('app.js missing');", media_type='application/javascript')
        with open(js_path, 'r', encoding='utf-8') as f:
                return Response(f.read(), media_type='application/javascript')

@app.get('/', response_class=HTMLResponse)
def index():
    return HTMLResponse(INDEX_HTML)

# ---------------------- ENTRY POINT ----------------------
def main():
    import uvicorn
    uvicorn.run("d3df_single_pmt.webapp:app", host="0.0.0.0", port=8000, reload=False)

if __name__ == '__main__':
    main()
