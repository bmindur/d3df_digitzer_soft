import os
import sys
import argparse
import subprocess
import threading
import time
from datetime import datetime
import logging

try:
    import yaml  # Requires PyYAML
except Exception:
    yaml = None

from .caen_hv import main as caen_hv_main
from io import StringIO
import contextlib
from logging import handlers


def setup_logger(log_dir, name='dt5743_runner'):
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, f"{name}.log")
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    # Avoid duplicate handlers if called twice
    if not logger.handlers:
        fmt = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
        # File handler (rotating)
        fh = handlers.RotatingFileHandler(log_path, maxBytes=2*1024*1024, backupCount=3, encoding='utf-8')
        fh.setFormatter(fmt)
        fh.setLevel(logging.INFO)
        logger.addHandler(fh)
        # Stdout handler
        sh = logging.StreamHandler(sys.stdout)
        sh.setFormatter(fmt)
        sh.setLevel(logging.INFO)
        logger.addHandler(sh)
    return logger


def generate_ini_from_yaml(yaml_path, output_ini_path, overrides, channel_overrides=None):
    """Generate WaveDemo INI from YAML config and CLI overrides.

    Args:
        yaml_path: Path to YAML config with keys mirroring INI sections/keys.
        output_ini_path: Destination INI path.
        overrides: Dict of top-level overrides like DATAFILE_PATH, BATCH_MODE, TRIGGER_THRESHOLD, etc.
    """
    # Load YAML config
    cfg = {}
    if yaml and yaml_path and os.path.exists(yaml_path):
        with open(yaml_path, 'r', encoding='utf-8') as yf:
            cfg = yaml.safe_load(yf) or {}

    # Apply CLI overrides to appropriate sections
    for key, value in (overrides or {}).items():
        # Determine which section the key belongs to
        if key in ['DATAFILE_PATH', 'SAVE_RAW_DATA', 'SAVE_TDC_LIST', 'SAVE_WAVEFORM', 
                   'SAVE_ENERGY_HISTOGRAM', 'SAVE_TIME_HISTOGRAM', 'SAVE_LISTS', 'SAVE_RUN_INFO',
                   'OUTPUT_FILE_FORMAT', 'OUTPUT_FILE_HEADER', 'OUTPUT_FILE_TIMESTAMP_UNIT',
                   'STATS_RUN_ENABLE', 'PLOT_RUN_ENABLE', 'DGTZ_RESET', 'SYNC_ENABLE',
                   'TRIGGER_FIXED', 'BOARD_REF', 'CHANNEL_REF', 'ENERGY_H_NBIN', 'TIME_H_NBIN',
                   'TIME_H_MODE', 'TIME_H_MIN', 'TIME_H_MAX', 'BATCH_MODE', 'BATCH_MAX_EVENTS', 'BATCH_MAX_TIME']:
            cfg.setdefault('OPTIONS', {})[key] = value
        else:
            cfg.setdefault('COMMON', {})[key] = value

    # Apply per-board/channel overrides like TRIGGER_THRESHOLD
    # channel_overrides format: { (board, channel): {key: value, ...}, ... }
    for bc, kv in (channel_overrides or {}).items():
        try:
            b, c = bc
            section_name = f"BOARD {int(b)} - CHANNEL {int(c)}"
            cfg.setdefault(section_name, {})
            for k, v in kv.items():
                cfg[section_name][k] = v
        except Exception:
            pass

    # Build INI content
    lines = []
    lines.append("# ****************************************************************\n")
    lines.append("# WaveDemo_x743 Configuration File (Auto-generated)\n")
    lines.append(f"# Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    lines.append("# ****************************************************************\n\n")

    def value_to_ini(v):
        # Map Python/YAML types to INI conventions used by WaveDemo
        if isinstance(v, bool):
            return 'YES' if v else 'NO'
        if isinstance(v, (int, float)):
            return v
        # Strings like '4K' or enums should be unquoted in INI
        if isinstance(v, str):
            return v
        # Fallback: string representation
        return str(v)

    # Helper to write section
    def write_section(section_name, section_data):
        lines.append(f"[{section_name}]\n")
        for key, value in section_data.items():
            ini_val = value_to_ini(value)
            lines.append(f"{key} = {ini_val}\n")
        lines.append("\n")

    # Write sections in order
    section_order = ['CONNECTIONS', 'OPTIONS', 'COMMON']
    for section in section_order:
        if section in cfg:
            write_section(section, cfg[section])

    # Write board-specific sections
    for section in sorted(cfg.keys()):
        if section.startswith('BOARD') or section not in section_order:
            if section not in section_order:
                write_section(section, cfg[section])

    os.makedirs(os.path.dirname(output_ini_path) or '.', exist_ok=True)
    with open(output_ini_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)
    return output_ini_path


def prepend_setup_to_run_info(run_info_path, setup):
    """Prepend setup block to run_info.txt if not already present."""
    if not os.path.exists(run_info_path):
        return False
    with open(run_info_path, 'r', encoding='utf-8') as f:
        existing = f.read()
    header = (
        "-----------------------------------------------------------------\n"
        "SETUP\n"
        "-----------------------------------------------------------------\n"
        f"PMT = {setup.get('pmt', '')}\n"
        f"PMT_HV = {setup.get('pmt_hv', '')}\n"
        f"SOURCE = {setup.get('source', '')}\n"
        f"SCINTILLATOR = {setup.get('scintillator', '')}\n\n"
    )
    if existing.startswith(header):
        return False
    with open(run_info_path, 'w', encoding='utf-8') as f:
        f.write(header + existing)
    return True


def find_latest_run_files(output_dir):
    """Return latest Wave_0_0.txt and run_info.txt in output_dir, if present."""
    if not os.path.isdir(output_dir):
        return None, None
    txts = [f for f in os.listdir(output_dir) if f.endswith('_Wave_0_0.txt')]
    infos = [f for f in os.listdir(output_dir) if f.endswith('_run_info.txt')]
    if not txts and not infos:
        return None, None
    def ts_from_name(name):
        # Expect prefix like 2025-11-27_21-06-35_...
        base = os.path.basename(name)
        ts = base.split('_Wave_')[0].split('_run_info')[0]
        try:
            return datetime.strptime(ts, '%Y-%m-%d_%H-%M-%S')
        except Exception:
            return datetime.fromtimestamp(0)
    latest_txt = max(txts, key=lambda n: ts_from_name(n)) if txts else None
    latest_info = max(infos, key=lambda n: ts_from_name(n)) if infos else None
    return (
        os.path.join(output_dir, latest_txt) if latest_txt else None,
        os.path.join(output_dir, latest_info) if latest_info else None,
    )


def run_wavedemo(exe_path, ini_path, batch_mode=None, output_path=None, enable_quit=True, logger=None):
    """Run WaveDemo_x743.exe with provided ini and optional overrides, streaming stdout."""
    cmd = [exe_path]
    if ini_path:
        cmd.append(ini_path)
    if batch_mode is not None:
        cmd.extend(['--batch-mode', str(batch_mode)])
    if output_path:
        cmd.extend(['--output-path', output_path])

    # Start process and stream output line-by-line
    if logger:
        logger.info(f"Starting WaveDemo: exe={exe_path}, ini={ini_path}, batch_mode={batch_mode}, output_path={output_path}")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
        bufsize=1,
        universal_newlines=True,
    )
    if logger:
        logger.info("WaveDemo started; streaming output and waiting for completion...")
    stdout_lines = []
    stderr_lines = []
    stop_flag = {'stopped': False}

    def keyboard_quit_listener():
        if not enable_quit:
            return
        if logger:
            logger.info("Press 'q' + Enter to request graceful stop...")
        try:
            for line in sys.stdin:
                if line.strip().lower() == 'q':
                    try:
                        proc.stdin.write('q\n')
                        proc.stdin.flush()
                        stop_flag['stopped'] = True
                        if logger:
                            logger.info('Sent quit command to WaveDemo.')
                    except Exception as e:
                        if logger:
                            logger.error(f"Failed to send quit: {e}")
                    break
        except Exception:
            pass

    # Start keyboard listener in background
    if enable_quit:
        t = threading.Thread(target=keyboard_quit_listener, daemon=True)
        t.start()
    try:
        # Read stdout live
        for line in proc.stdout:
            if logger:
                logger.info(line.rstrip())
            stdout_lines.append(line)
        # After stdout closes, read remaining stderr
        err = proc.stderr.read() or ''
        if err:
            # Print stderr at the end to avoid interleaving
            if logger:
                for el in err.splitlines():
                    logger.error(el)
            stderr_lines.append(err)
    finally:
        proc.wait()
    if logger:
        logger.info(f"WaveDemo finished with code {proc.returncode}")
    return proc.returncode, ''.join(stdout_lines), ''.join(stderr_lines)


def get_current_hv(hv_device=None, hv_baudrate=None, hv_timeout=None, hv_channel=None):
    """Query CAEN HV monitor (vmon) via caen-hv CLI and return HV value as negative string.

    Returns empty string on failure.
    """
    try:
        buf = StringIO()
        argv_backup = sys.argv
        argv = ['caen-hv']
        if hv_device:
            argv += ['--device', str(hv_device)]
        if hv_baudrate:
            argv += ['--baudrate', str(hv_baudrate)]
        if hv_timeout:
            argv += ['--timeout', str(hv_timeout)]
        if hv_channel is not None:
            argv += ['--channel', str(hv_channel)]
        argv += ['mon', 'vmon']
        sys.argv = argv
        if hv_device or hv_baudrate or hv_timeout or hv_channel:
            logging.getLogger('dt5743_runner').info(f"Query HV with params device={hv_device}, baud={hv_baudrate}, timeout={hv_timeout}, channel={hv_channel}")
        with contextlib.redirect_stdout(buf):
            logging.getLogger('dt5743_runner').info("Querying current HV (VMON); waiting for response...")
            caen_hv_main()
        sys.argv = argv_backup
        output = buf.getvalue()
        import re
        for line in output.splitlines():
            l = line.strip().lower()
            if 'vmon' in l:
                m = re.search(r"[-+]?\d+(?:\.\d+)?", line)
                if m:
                    try:
                        val = float(m.group(0))
                        if val > 0:
                            val = -val
                        if val.is_integer():
                            parsed = str(int(val))
                        else:
                            parsed = f"{val:.3f}".rstrip('0').rstrip('.')
                        logging.getLogger('dt5743_runner').info(f"Parsed HV value: {parsed}")
                        return parsed
                    except Exception:
                        raw = m.group(0)
                        return '-' + raw.lstrip('+') if not raw.startswith('-') else raw
        m2 = re.search(r"[-+]?\d+(?:\.\d+)?", output)
        if m2:
            try:
                val = float(m2.group(0))
                if val > 0:
                    val = -val
                if val.is_integer():
                    return str(int(val))
                return f"{val:.3f}".rstrip('0').rstrip('.')
            except Exception:
                raw = m2.group(0)
                return '-' + raw.lstrip('+') if not raw.startswith('-') else raw
        return ''
    except Exception:
        return ''


def main():
    parser = argparse.ArgumentParser(
        description='Configure and run WaveDemo_x743.exe, optionally set HV, and augment run_info.'
    )
    parser.add_argument('--yaml', required=True, help='YAML config path for WaveDemo options')
    parser.add_argument('--output-ini', default='WaveDemoConfig.generated.ini',
                        help='Output INI filename or path; defaults to saving inside --data-output folder')
    parser.add_argument('--exe', default=os.path.join('WaveDemo_x743.exe'),
                        help='Path to WaveDemo_x743.exe')
    parser.add_argument('--data-output', default=os.path.join('.', 'data_output'),
                        help='Output folder for WaveDemo data files')
    parser.add_argument('--batch-mode', type=int, default=2,
                        help='Batch mode: 0 interactive, 1 with viz, 2 no viz (default: 2)')
    parser.add_argument('--max-events', type=int, default=0, help='Override BATCH_MAX_EVENTS (0 = unlimited)')
    parser.add_argument('--max-time', type=int, default=0, help='Override BATCH_MAX_TIME seconds (0 = unlimited)')
    parser.add_argument('--trigger-threshold', type=float, help='Override TRIGGER_THRESHOLD (V)')
    parser.add_argument('--sampling-frequency', type=int, choices=[0,1,2,3], help='Override SAMPLING_FREQUENCY enum')
    parser.add_argument('--channel-thresholds',
                        help='Comma-separated per-channel thresholds, e.g. "0:0=-0.10,0:1=0.20" (BOARD:CHANNEL=VALUE)')
    parser.add_argument('--set-hv', type=float, help='If provided, set PMT HV via caen-hv script')
    parser.add_argument('--check-hv', action='store_true', help='After setting, query and print current HV')
    parser.add_argument('--pmt', default='HAMAMATSU_R4998', help='PMT model name for run_info header')
    parser.add_argument('--source', default='BKG', help='Source label for run_info header')
    parser.add_argument('--scintillator', default='RMPS470', help='Scintillator label for run_info header')
    # CAEN HV connection options passthrough
    parser.add_argument('--hv-device', help='Serial device for CAEN HV (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--hv-baudrate', type=int, default=9600, help='Baudrate for CAEN HV serial (default: 9600)')
    parser.add_argument('--hv-timeout', type=float, help='Timeout seconds for CAEN HV communication')
    parser.add_argument('--hv-channel', type=int, help='Target CAEN HV channel to operate on')
    args = parser.parse_args()

    # Ensure PyYAML availability
    if yaml is None:
        print('Error: PyYAML is required. Install via: pip install pyyaml')
        sys.exit(2)

    # Build overrides
    overrides = {
        'DATAFILE_PATH': args.data_output,
        'BATCH_MODE': args.batch_mode,
        'BATCH_MAX_EVENTS': args.max_events,
        'BATCH_MAX_TIME': args.max_time,
    }
    if args.trigger_threshold is not None:
        overrides['TRIGGER_THRESHOLD'] = args.trigger_threshold
    if args.sampling_frequency is not None:
        overrides['SAMPLING_FREQUENCY'] = args.sampling_frequency

    # Parse per-channel thresholds
    channel_overrides = {}
    if args.channel_thresholds:
        # Format: BOARD:CHANNEL=VALUE pairs separated by commas
        pairs = [p.strip() for p in args.channel_thresholds.split(',') if p.strip()]
        for p in pairs:
            if '=' not in p or ':' not in p:
                continue
            left, val = p.split('=', 1)
            try:
                b_str, c_str = left.split(':', 1)
                b = int(b_str.strip()); c = int(c_str.strip())
                v = float(val.strip())
            except Exception:
                continue
            channel_overrides.setdefault((b, c), {})['TRIGGER_THRESHOLD'] = v

    # Resolve INI path inside output data directory
    os.makedirs(args.data_output, exist_ok=True)
    ini_out_path = args.output_ini
    # If only a filename or relative path was provided, place it under data_output
    if not os.path.isabs(ini_out_path):
        ini_out_path = os.path.join(args.data_output, ini_out_path)

    # Generate INI
    ini_path = generate_ini_from_yaml(args.yaml, ini_out_path, overrides, channel_overrides)
    logger = setup_logger(args.data_output)
    logger.info(f"Generated INI: {os.path.abspath(ini_path)}")

    # Optionally set HV via caen-hv
    if args.set_hv is not None:
        try:
            # Call caen-hv main with args-like emulation
            sys_argv_backup = sys.argv
            hv_argv = ['caen-hv']
            if args.hv_device:
                hv_argv += ['--device', str(args.hv_device)]
            if args.hv_baudrate:
                hv_argv += ['--baudrate', str(args.hv_baudrate)]
            if args.hv_timeout is not None:
                hv_argv += ['--timeout', str(args.hv_timeout)]
            if args.hv_channel is not None:
                hv_argv += ['--channel', str(args.hv_channel)]
            # Correct HV set syntax: set vset --val <HV>
            hv_argv += ['set', 'vset', '--val', str(args.set_hv)]
            sys.argv = hv_argv
            logger.info(f"Setting HV via command: caen-hv set vset --val {args.set_hv} (device={args.hv_device}, baud={args.hv_baudrate}, timeout={args.hv_timeout}, channel={args.hv_channel})")
            caen_hv_main()
            # Optional check/monitor after set
            if args.check_hv:
                buf = StringIO()
                sys.argv = hv_argv[:1] + hv_argv[1:hv_argv.index('set')] + ['mon', 'vmon']
                with contextlib.redirect_stdout(buf):
                    caen_hv_main()
                for ln in buf.getvalue().splitlines():
                    logger.info(ln)
            logger.info("HV set/check complete.")
            sys.argv = sys_argv_backup
        except Exception as e:
            logger.error(f"HV set/check failed: {e}")

    # Run WaveDemo in batch mode
    logger.info("Launching WaveDemo and waiting for it to finish...")
    code, out, err = run_wavedemo(
        args.exe,
        ini_path,
        batch_mode=args.batch_mode,
        output_path=args.data_output,
        logger=logger,
    )
    if code != 0:
        logger.error(f"WaveDemo_x743.exe exited with error code: {code}")
        if err:
            for el in err.splitlines():
                logger.error(el)
    else:
        logger.info('WaveDemo completed successfully.')

    # Find generated run_info and prepend setup header
    txt_path, info_path = find_latest_run_files(args.data_output)
    # Determine PMT_HV from monitor if available; otherwise fall back to --set-hv
    hv_str = get_current_hv(
        hv_device=args.hv_device,
        hv_baudrate=args.hv_baudrate,
        hv_timeout=args.hv_timeout,
        hv_channel=args.hv_channel,
    )
    if not hv_str:
        hv_str = f"{int(args.set_hv) if args.set_hv is not None else ''}"
    setup = {
        'pmt': args.pmt,
        'pmt_hv': hv_str,
        'source': args.source,
        'scintillator': args.scintillator,
    }
    if info_path:
        changed = prepend_setup_to_run_info(info_path, setup)
        if changed:
            logger.info(f"Prepended setup header to: {info_path}")
        else:
            logger.info(f"Setup header already present or run_info missing: {info_path}")
    else:
        logger.warning('No run_info file found to modify.')


if __name__ == '__main__':
    main()
