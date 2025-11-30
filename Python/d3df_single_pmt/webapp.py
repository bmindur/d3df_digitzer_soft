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
import os
from typing import Dict, List, Optional, Any
import logging
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, BackgroundTasks, HTTPException, Query, Depends, status
from fastapi.responses import JSONResponse, HTMLResponse, Response
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from pydantic import BaseModel, Field
import secrets

from .caen_hv import send_caen_command

app = FastAPI(title="Digitizer Web Interface", version="0.1.0")
security = HTTPBasic()

# Simple authentication - username and password from environment or defaults
AUTH_USERNAME = os.getenv("DIGITIZER_USERNAME", "d3df")
AUTH_PASSWORD = os.getenv("DIGITIZER_PASSWORD", "dt5743")

def verify_credentials(credentials: HTTPBasicCredentials = Depends(security)):
    correct_username = secrets.compare_digest(credentials.username, AUTH_USERNAME)
    correct_password = secrets.compare_digest(credentials.password, AUTH_PASSWORD)
    if not (correct_username and correct_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect username or password",
            headers={"WWW-Authenticate": "Basic"},
        )
    return credentials.username
logger = logging.getLogger("digitizer-webapp")
if not logger.handlers:
    handler = logging.StreamHandler(sys.stdout)
    handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
    logger.addHandler(handler)
logger.setLevel(logging.INFO)

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
    source: Optional[str] = Field(None, description="Radiation source identifier")
    scintillator: Optional[str] = Field(None, description="Scintillator type/identifier")

class MeasureStatus(BaseModel):
    id: str
    running: bool
    current_hv: Optional[float]
    current_threshold: Optional[float]
    start_time: float
    elapsed: float  # total elapsed (all repeats)
    events: int
    rate: float
    iteration: int
    total_iterations: Optional[int]
    repeat_index: int
    repeat_total: Optional[int]
    last_line: Optional[str]
    progress_line: Optional[str] = None  # per-run progress string
    run_elapsed: Optional[int] = None  # elapsed for current run
    run_remaining: Optional[int] = None  # remaining for current run
    total_elapsed: Optional[int] = None  # total elapsed (all repeats)
    total_remaining: Optional[int] = None  # total remaining (all repeats)
    runs: List[Dict[str, Any]] = Field(default_factory=list)  # history of completed runs

# ---------------------- RUNTIME STATE ----------------------
class MeasurementTask:
    def __init__(self, req: MeasureStartRequest):
        self.req = req
        self.id = uuid.uuid4().hex
        self.start_time = time.time()
        self.run_start_time: Optional[float] = None  # Time when current run actually started acquisition
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
        self.log_lines: list[str] = []
        self.runs: List[Dict[str, Any]] = []  # list of dicts capturing history of runs
        self.run_info_path: Optional[str] = None  # path to current run_info.txt file
        self.thread = threading.Thread(target=self.run_loop, daemon=True)
        self.lock = threading.Lock()
        self.proc: Optional[subprocess.Popen] = None
        self.thread.start()

    def append_log(self, msg: str):
        self.log_lines.append(msg)
        self.last_line = msg

    def _read_hv(self) -> Optional[float]:
        try:
            resp = send_caen_command('MON', 'VMON', channel=str(self.req.hv_channel or '1'), device=self.req.hv_device or 'COM10', baudrate=self.req.hv_baudrate or 9600, timeout=self.req.hv_timeout or 1.0)
            try:
                return float(str(resp).replace(',', '.').strip())
            except Exception:
                return None
        except Exception:
            return None

    def wait_for_hv(self, target: float, tolerance: float = 0.5, max_wait: float = 30.0, poll_interval: float = 1.0) -> bool:
        start = time.time()
        logger.info(f"Waiting for HV to reach target {target} V (±{tolerance} V), max_wait={max_wait}s")
        while time.time() - start < max_wait:
            hv = self._read_hv()
            msg_mon = f"Monitored HV: {hv} V" if hv is not None else "Monitored HV: read error"
            logger.info(msg_mon)
            with self.lock:
                self.append_log(msg_mon)
            if hv is not None:
                # Compare absolute difference; some systems use negative voltages
                if abs(hv - target) <= tolerance:
                    logger.info(f"HV reached target: measured {hv} V within tolerance (target {target} V)")
                    return True
            time.sleep(poll_interval)
        logger.warning(f"HV did not reach target within tolerance after {max_wait}s (last measured HV may differ)")
        return False

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
        if self.req.source:
            cmd += ['--source', self.req.source]
        if self.req.scintillator:
            cmd += ['--scintillator', self.req.scintillator]
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
        
        # Read current HV if not setting a new one
        if hv is None:
            current_hv = self._read_hv()
            if current_hv is not None:
                self.current_hv = current_hv
                msg_read = f"Current HV: {current_hv} V"
                logger.info(msg_read)
                with self.lock:
                    self.append_log(msg_read)
        
        # Wait for HV before starting measurement process
        if hv is not None:
            # Set HV before waiting
            try:
                msg_set = f"Setting HV to {hv} V..."
                logger.info(msg_set)
                with self.lock:
                    self.append_log(msg_set)
                send_caen_command('SET', 'VSET', str(hv), channel=str(self.req.hv_channel or '1'), device=self.req.hv_device or 'COM10', baudrate=self.req.hv_baudrate or 9600, timeout=self.req.hv_timeout or 1.0)
            except Exception as e:
                msg_err = f"Failed to set HV to {hv} V: {e}"
                logger.error(msg_err)
                with self.lock:
                    self.append_log(msg_err)
                return
            msg_wait = f"Waiting for HV to reach target {hv} V before starting measurement..."
            logger.info(msg_wait)
            with self.lock:
                self.append_log(msg_wait)
            ok = self.wait_for_hv(hv, tolerance=0.5, max_wait=max((self.req.hv_timeout or 1.0) * 10, 30.0))
            if not ok:
                msg_skip = f"HV not within tolerance (target {hv} V). Skipping measurement."
                with self.lock:
                    self.append_log(msg_skip)
                logger.info(f"Skipping measurement: HV not within tolerance for target {hv} V")
                return
            msg_ready = f"HV reached target {hv} V. Proceeding to measurement."
            logger.info(msg_ready)
            with self.lock:
                self.append_log(msg_ready)
        # Compose info for this iteration
        hv_display = f"{hv} V" if hv is not None else (f"{self.current_hv} V" if self.current_hv is not None else "unknown")
        thr_display = f"{threshold}" if threshold is not None else "default"
        rep_str = f"repeat {self.repeat_index+1}/{self.repeat_total}" if self.repeat_total else (f"repeat {self.repeat_index+1}/∞" if self.req.repeat == -1 else "")
        msg_start = f"Starting measurement iteration {self.iteration + 1} with HV={hv_display}, threshold={thr_display} {rep_str}"
        logger.info(msg_start)
        with self.lock:
            self.append_log(msg_start)
            self.run_start_time = None  # Reset run start time
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
                self.append_log(line.rstrip())
                
                # Detect acquisition start
                if 'Starting Acquisition' in line:
                    self.run_start_time = time.time()
                
                # Capture run_info.txt path from log
                if 'Prepended setup header to:' in line:
                    # Extract path after "Prepended setup header to: "
                    path_match = re.search(r'Prepended setup header to:\s*(.+)', line)
                    if path_match:
                        self.run_info_path = path_match.group(1).strip()
                
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
        hv_display = f"{hv} V" if hv is not None else (f"{self.current_hv} V" if self.current_hv is not None else "unknown")
        thr_display = f"{threshold}" if threshold is not None else "default"
        msg_end = f"Measurement iteration finished (HV={hv_display}, threshold={thr_display}). Events={self.events}, Rate={self.rate:.2f} 1/s"
        logger.info(msg_end)
        with self.lock:
            self.append_log(msg_end)
            # Record run history entry
            end_time = time.time()
            total_duration = end_time - start  # includes setup + launch overhead
            # Measurement duration excludes setup/launch; uses acquisition start timestamp if available
            if self.run_start_time is not None and self.run_start_time >= start and self.run_start_time <= end_time:
                measurement_duration = end_time - self.run_start_time
            else:
                measurement_duration = total_duration
            run_record = {
                'timestamp': start,
                'repeat': self.repeat_index + 1,
                'iteration': self.iteration,
                'hv': self.current_hv if hv is None else hv,
                'threshold': threshold,
                'run_info': self.run_info_path or '',
                'duration': measurement_duration,
                'total_duration': total_duration,
                'events': self.events,
                'rate': self.rate,
            }
            self.runs.append(run_record)
            self.run_info_path = None  # Reset for next run

    def run_loop(self):
        iterations = self.compute_plan()
        repeat_index = 0
        while self.running:
            for idx, (hv, thr) in enumerate(iterations):
                if not self.running:
                    break
                with self.lock:
                    self.iteration = repeat_index * len(iterations) + idx + 1
                    self.repeat_index = repeat_index
                self.run_single(hv, thr)
            repeat_index += 1
            if self.repeat_total is not None and repeat_index >= self.repeat_total:
                break
        with self.lock:
            self.running = False
            msg_complete = "All measurements completed."
            logger.info(msg_complete)
            self.append_log(msg_complete)

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
            now = time.time()
            hv_str = f"HV={self.current_hv} V" if self.current_hv is not None else "HV=unknown"
            thr_str = f"threshold={self.current_threshold}" if self.current_threshold is not None else "threshold=default"
            rep_str = f"repeat {self.repeat_index+1}/{self.repeat_total}" if self.repeat_total else (f"repeat {self.repeat_index+1}/∞" if self.req.repeat == -1 else "")
            total_elapsed = int(now - self.start_time)
            
            # Calculate iterations
            iterations_per_repeat = self.total_iterations or 1
            current_iteration_in_repeat = (self.iteration - 1) % iterations_per_repeat if self.iteration > 0 else 0
            
            # Per-run elapsed/remaining (from actual acquisition start)
            run_elapsed = None
            run_remaining = None
            total_remaining = None
            
            if self.req.max_time and self.req.max_time > 0:
                # Calculate per-run elapsed from acquisition start if available
                if self.run_start_time is not None:
                    run_elapsed = int(now - self.run_start_time)
                    run_remaining = max(self.req.max_time - run_elapsed, 0)
                
                # Calculate average time per iteration so far (for total remaining calculation)
                completed_iterations = self.iteration - 1 if self.iteration > 0 else 0
                avg_time_per_iter = (total_elapsed / completed_iterations) if completed_iterations > 0 else self.req.max_time
                
                # Total remaining: calculate based on all remaining iterations across all repeats
                if self.repeat_total:
                    total_iterations_planned = iterations_per_repeat * self.repeat_total
                    remaining_iterations = total_iterations_planned - self.iteration
                    # Remaining in current iteration + all future iterations
                    if run_remaining is not None:
                        total_remaining = int(run_remaining + (remaining_iterations * avg_time_per_iter))
                    else:
                        # Fallback if run hasn't started yet
                        total_remaining = int(remaining_iterations * avg_time_per_iter) if completed_iterations > 0 else None
                    
            rate_val = self.rate if self.rate else 0.0
            if self.req.repeat == -1:
                # Infinite loop: only show elapsed for run
                prog = (
                    f"Progress: {hv_str}, {thr_str}, {rep_str}, elapsed {run_elapsed if run_elapsed is not None else total_elapsed}s, events {self.events}, rate {rate_val:.2f} 1/s"
                )
            else:
                prog = (
                    f"Progress: {hv_str}, {thr_str}, {rep_str}, elapsed {run_elapsed if run_elapsed is not None else total_elapsed}s"
                    + (f", remaining {run_remaining}s" if run_remaining is not None else "")
                    + f", events {self.events}, rate {rate_val:.2f} 1/s"
                )
            return MeasureStatus(
                id=self.id,
                running=self.running,
                current_hv=self.current_hv,
                current_threshold=self.current_threshold,
                start_time=self.start_time,
                elapsed=total_elapsed,
                events=self.events,
                rate=self.rate,
                iteration=self.iteration,
                total_iterations=self.total_iterations,
                repeat_index=self.repeat_index,
                repeat_total=self.repeat_total,
                last_line=self.last_line,
                progress_line=prog,
                run_elapsed=run_elapsed,
                run_remaining=run_remaining,
                total_elapsed=total_elapsed,
                total_remaining=total_remaining,
                runs=self.runs,
            )

measurements: Dict[str, MeasurementTask] = {}

# ---------------------- HV ENDPOINTS ----------------------
@app.post('/hv/set')
def hv_set(req: HVSetRequest, username: str = Depends(verify_credentials)):
    val = req.value
    try:
        resp = send_caen_command('SET', 'VSET', str(val), channel=req.channel, device=req.device, baudrate=req.baudrate, timeout=req.timeout)
        return {'status': 'ok', 'response': resp, 'hv_set': val}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get('/hv/read')
def hv_read(channel: str = Query('1'), device: str = Query('COM10'), baudrate: int = Query(9600), timeout: float = Query(1.0), username: str = Depends(verify_credentials)):
    try:
        resp = send_caen_command('MON', 'VMON', channel=channel, device=device, baudrate=baudrate, timeout=timeout)
        return {'status': 'ok', 'hv': resp}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post('/hv/send')
def hv_send(req: HVSendRequest, username: str = Depends(verify_credentials)):
    try:
        resp = send_caen_command(req.cmd.upper(), req.par.upper(), None if req.val is None else str(req.val), channel=req.channel, device=req.device, baudrate=req.baudrate, timeout=req.timeout)
        return {'status': 'ok', 'response': resp}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

# ---------------------- MEASUREMENT ENDPOINTS ----------------------
@app.post('/measure/start')
def measure_start(req: MeasureStartRequest, username: str = Depends(verify_credentials)):
    task = MeasurementTask(req)
    measurements[task.id] = task
    return {'status': 'started', 'id': task.id}

@app.post('/measure/stop/{mid}')
def measure_stop(mid: str, username: str = Depends(verify_credentials)):
    task = measurements.get(mid)
    if not task:
        raise HTTPException(status_code=404, detail='Measurement not found')
    task.stop()
    return {'status': 'stopping', 'id': mid}

@app.get('/measure/status')
def measure_status(username: str = Depends(verify_credentials)):
    return {'measurements': [m.snapshot().dict() for m in measurements.values()]}

@app.get('/measure/history/{mid}')
def measure_history(mid: str, format: str = Query('csv'), username: str = Depends(verify_credentials)):
    task = measurements.get(mid)
    if not task:
        raise HTTPException(status_code=404, detail='Measurement not found')
    # Copy runs under lock
    with task.lock:
        runs = list(task.runs)
    if format.lower() == 'json':
        return {'id': mid, 'runs': runs}
    # Default CSV
    import io, csv, datetime
    buf = io.StringIO()
    writer = csv.writer(buf)
    writer.writerow(['index','timestamp_iso','repeat','iteration','hv','threshold','duration_s','events','rate_1_per_s','run_info'])
    for idx, r in enumerate(runs, start=1):
        ts_iso = ''
        if isinstance(r.get('timestamp'), (int,float)):
            ts_iso = datetime.datetime.fromtimestamp(r['timestamp']).isoformat(sep=' ', timespec='seconds')
        writer.writerow([
            idx,
            ts_iso,
            r.get('repeat',''),
            r.get('iteration',''),
            r.get('hv',''),
            r.get('threshold',''),
            f"{r.get('duration',0):.3f}",
            r.get('events',''),
            r.get('rate',''),
            r.get('run_info',''),
        ])
    csv_data = buf.getvalue()
    return Response(content=csv_data, media_type='text/csv', headers={'Content-Disposition': f'attachment; filename="run_history_{mid}.csv"'})

@app.get('/yaml/list')
def yaml_list(username: str = Depends(verify_credentials)):
    import os
    import glob
    # List all .yaml files in current working directory
    yaml_files = glob.glob('*.yaml')
    return {'files': sorted(yaml_files)}

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
                # Send final status one more time to ensure frontend receives it
                await asyncio_sleep(0.5)
                await ws.send_json(snap.dict())
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
INDEX_HTML = """<!doctype html><html><head><meta charset='utf-8'/><meta name='viewport' content='width=device-width,initial-scale=1'/><title>Digitizer Web Interface</title><script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js'></script><style>
/* Theme variables */
:root{
    --bg:#ffffff; --fg:#111111; --muted:#555; --card:#f7f7f7; --border:#cccccc; --accent:#2663ff; --accent-contrast:#ffffff; --table-head:#222; --table-head-fg:#eee;
}
@media (prefers-color-scheme: dark){
    :root{ --bg:#0f1115; --fg:#e6edf3; --muted:#8b949e; --card:#161b22; --border:#30363d; --accent:#58a6ff; --accent-contrast:#0b0e12; --table-head:#1f2328; --table-head-fg:#d0d7de; }
}
.theme-dark{ --bg:#0f1115; --fg:#e6edf3; --muted:#8b949e; --card:#161b22; --border:#30363d; --accent:#58a6ff; --accent-contrast:#0b0e12; --table-head:#1f2328; --table-head-fg:#d0d7de; }
.theme-light{ --bg:#ffffff; --fg:#111111; --muted:#555; --card:#f7f7f7; --border:#cccccc; --accent:#2663ff; --accent-contrast:#ffffff; --table-head:#222; --table-head-fg:#eee; }

body{font-family:system-ui,Arial,sans-serif;margin:16px;background:var(--bg);color:var(--fg)}
h1{display:flex;align-items:center;justify-content:space-between}
.theme-toggle{display:flex;align-items:center;gap:8px;font-size:.9rem}
.row{display:flex;flex-wrap:wrap;gap:16px}.col{flex:1 1 400px;min-width:360px}
fieldset{border:1px solid var(--border);background:var(--card);padding:12px;border-radius:8px}
label{display:block;margin-top:6px;font-size:.9rem;color:var(--muted)}
input,textarea,select{width:100%;padding:6px;margin-top:4px;background:var(--bg);color:var(--fg);border:1px solid var(--border)}
button{margin-top:8px;padding:6px 10px;background:var(--accent);color:var(--accent-contrast);border:0;border-radius:6px;cursor:pointer}
button:disabled{background:var(--border);color:var(--muted);cursor:not-allowed;opacity:0.6}
code{background:var(--card);padding:2px 4px;border-radius:4px;border:1px solid var(--border)}
canvas{max-height:320px}
#log{white-space:pre-wrap;max-height:160px;overflow:auto;background:#111;color:#ddd;padding:8px}
.status-badge{display:inline-block;padding:2px 6px;border-radius:4px;font-size:.75rem;font-weight:600}
#hv_status.connected{background:#2a6;color:#fff}#hv_status.disconnected{background:#c22;color:#fff}
.progress-wrap{margin-top:8px}.progress-labels{display:flex;justify-content:space-between;font-size:.85rem;margin-bottom:4px;color:var(--muted)}
.progress-bar{width:100%;height:12px;background:#eee;border-radius:6px;overflow:hidden}.progress-bar>div{height:100%;background:var(--accent);width:0%}
table.run-history{width:100%;border-collapse:collapse;font-size:.75rem;margin-top:12px}
table.run-history th,table.run-history td{border:1px solid var(--border);padding:4px 6px;text-align:left}
table.run-history th{background:var(--table-head);color:var(--table-head-fg)}
table.run-history tbody tr:nth-child(even){background:rgba(0,0,0,0.03)}
</style></head>
<body>
<h1>
    <span>Digitizer Web Interface</span>
    <div class='theme-toggle'>
        <label for='theme_select'>Theme</label>
        <select id='theme_select'>
            <option value='system'>System</option>
            <option value='light'>Light</option>
            <option value='dark'>Dark</option>
        </select>
    </div>
</h1>
<div class='row'><div class='col'><fieldset><legend>Setup Control</legend><label>YAML config <select id='m_yaml'><option value=''>Loading...</option></select></label><label>Data output <input id='m_out' value='./data_output'/></label><label>WaveDemo exe <input id='m_exe' value='WaveDemo_x743.exe'/></label><label>Source <input id='m_source' list='source_list' placeholder='e.g. BKG'/><datalist id='source_list'><option>BKG</option><option>Cs-137_D10-224</option><option>Cd-109_M8-546</option><option>Fe-55_AC-6389</option></datalist><label>Scintillator <input id='m_scint' list='scint_list' placeholder='e.g. RMPS470'/><datalist id='scint_list'><option>RMPS470</option><option>BC-408</option></datalist><label>HV sequence (comma-separated) <input id='m_hvseq' placeholder='1800,1700'/></label><label>Thresholds (comma-separated) <input id='m_thrseq' placeholder='-0.10,-0.20'/></label><label>Repeat (-1=infinite, empty=1) <input id='m_repeat' value='1'/></label><label>Max events <input id='m_maxev' type='number' value='0'/></label><label>Max time (s) <input id='m_maxt' type='number' value='30'/></label></fieldset></div><div class='col'><fieldset><legend>HV Control</legend><label>Device <input id='hv_device' value='COM10'/></label><label>Channel <input id='hv_channel' value='1'/></label><label>Baudrate <input id='hv_baud' type='number' value='9600'/></label><div style='display:flex;gap:8px;align-items:end'><div style='flex:1'><label>Set HV (V) <input id='hv_value' type='number' step='1' value='1800'/></label></div><div><button id='btn_hv_set' type='button'>Set HV</button><button id='btn_hv_read' type='button'>Read HV</button></div></div><div><label>Raw HV Command</label><div style='display:flex;gap:8px'><input id='hv_cmd' value='MON' style='max-width:80px'/><input id='hv_par' value='VMON' style='max-width:120px'/><input id='hv_val' placeholder='val (optional)' style='max-width:140px'/><button id='btn_hv_send' type='button'>Send</button></div></div><div style='display:flex;gap:8px;align-items:end;margin-top:8px'><div style='flex:1'><label>Monitor interval (s)<input id='hv_interval' type='number' step='0.1' value='2'/></label></div><button id='btn_hv_toggle' type='button'>Start Monitoring</button><span id='hv_status' class='status-badge disconnected'>OFF</span></div><div id='hv_result' style='margin-top:8px'>Result: <code>(none)</code></div></fieldset></div></div><div class='row'><div class='col'><fieldset><legend>Measurement Control</legend><div style='display:flex;gap:8px;align-items:end'><button id='btn_m_start' type='button'>Start</button><button id='btn_m_stop' type='button' disabled>Stop</button><div>Current ID: <code id='m_id'>(none)</code></div></div></fieldset></div></div><h2>Live Monitoring</h2><div class='row'><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>HV Plot</span><button id='btn_clear_hv' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_hv'></canvas></div><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>Events Plot</span><button id='btn_clear_events' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_events'></canvas></div></div><div class='row'><div class='col'><div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:8px'><span style='font-weight:600'>Rate Plot</span><button id='btn_clear_rate' type='button' style='padding:4px 8px;font-size:.85rem'>Clear</button></div><canvas id='chart_rate'></canvas></div><div class='col'><fieldset><legend>Progress</legend><div class='progress-wrap'><div class='progress-labels'><span>Elapsed: <span id='prog_elapsed'>0s</span></span><span>Remaining: <span id='prog_remaining'>0s</span></span></div><div class='progress-bar'><div id='prog_bar'></div></div></div></fieldset><fieldset style='margin-top:12px'><legend>Log</legend><div id='log'></div></fieldset><fieldset style='margin-top:12px'><legend>Run History</legend><button id='btn_run_history_dl' type='button' style='margin-bottom:6px'>Download CSV</button><table class='run-history' id='run_history'><thead><tr><th>#</th><th>Timestamp</th><th>Repeat</th><th>Iteration</th><th>HV</th><th>Threshold</th><th>Duration(s)</th><th>Run Info</th></tr></thead><tbody></tbody></table></fieldset></div></div><script src='/static/app.js'></script></body></html>"""

@app.get('/static/app.js')
def static_app_js(username: str = Depends(verify_credentials)):
        # Serve external JS file content from static folder.
        import os
        js_path = os.path.join(os.path.dirname(__file__), 'static', 'app.js')
        if not os.path.exists(js_path):
                return Response("console.error('app.js missing');", media_type='application/javascript')
        with open(js_path, 'r', encoding='utf-8') as f:
                return Response(f.read(), media_type='application/javascript')

@app.get('/', response_class=HTMLResponse)
def index(username: str = Depends(verify_credentials)):
    return HTMLResponse(INDEX_HTML)

# ---------------------- ENTRY POINT ----------------------
def main():
    import uvicorn
    uvicorn.run("d3df_single_pmt.webapp:app", host="0.0.0.0", port=8000, reload=False)

if __name__ == '__main__':
    main()
