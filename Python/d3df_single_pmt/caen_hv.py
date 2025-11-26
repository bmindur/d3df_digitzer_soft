"""CAEN HV simple serial control utilities."""
from __future__ import annotations
import time
import serial
from serial.serialutil import SerialException

__all__ = ['send_caen_command']

def _build_command(
    cmd: str,
    par: str,
    channel: str = '2',
    val: str | None = None,
) -> str:
    base = f"$BD:00,CMD:{cmd},CH:{channel},PAR:{par}"
    if val is not None:
        base += f",VAL:{val}"
    return base

def send_caen_command(
    cmd: str,
    par: str,
    val: str | None = None,
    channel: str = '2',
    device: str = '/dev/caen_hv',
    baudrate: int = 9600,
    timeout: float = 1.0,
) -> str:
    command = _build_command(cmd, par, channel, val)
    try:
        ser = serial.serial_for_url(
            device,
            baudrate,
            parity=serial.PARITY_NONE,
            xonxoff=True,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
        )
    except SerialException as e:
        raise RuntimeError(f"Serial open failed: {e}") from e
    try:
        ser.write((command + '\r\n').encode())
        time.sleep(0.2)
        out_bytes = b''
        while ser.in_waiting > 0:
            out_bytes += ser.read(1)
        ser.close()
        out = out_bytes.decode(errors='ignore').strip()
        if not out:
            return ''
        if ':' in out:
            return out.split(':')[-1].split('\n')[0].strip()
        return out
    except SerialException as e:
        raise RuntimeError(f"Serial IO failed: {e}") from e

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description='Send CAEN HV serial command.'
    )
    parser.add_argument('cmd', help='Command (SET / MON)')
    parser.add_argument('par', help='Parameter (ON OFF VMON VSET)')
    parser.add_argument(
        '--val', help='Optional value (e.g. 200.0)', default=None
    )
    parser.add_argument(
        '--channel', default='1', help='HV channel (default 1)'
    )
    parser.add_argument(
        '--device', default='COM10', help='Serial device path or URL  (default COM10)'
    )
    parser.add_argument(
        '--baudrate', type=int, default=9600, help='Baud rate (default 9600)'
    )
    parser.add_argument(
        '--timeout', type=float, default=1.0, help='Read timeout seconds'
    )
    args = parser.parse_args()
    try:
        resp = send_caen_command(
            args.cmd,
            args.par,
            args.val,
            channel=args.channel,
            device=args.device,
            baudrate=args.baudrate,
            timeout=args.timeout,
        )
        print(resp if resp else '(no response)')
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    main()
