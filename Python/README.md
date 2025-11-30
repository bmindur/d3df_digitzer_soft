# d3df-single-pmt

CAEN DT5743 waveform CSV (Wave_0_0.txt) to HDF5 converter, analysis, and plotting package for single PMT workflows. Includes automatic parsing of `run_info` metadata and advanced pulse analysis/visualization tools.

## Features
- Converts waveform CSV files produced by DT5743 digitizer into compressed HDF5.
- Extracts timestamps, energies, ADC samples.
- Parses `run_info` text to attach experimental metadata as HDF5 attributes:
  - `pmt_hv`, `source`, `scintilator`
  - `trigger_threshold_common` and per-channel thresholds `trigger_threshold_boardX_chY`
- Provides Python API and CLI tools for conversion, analysis, and plotting.
- Pulse alignment, normalization, and timing analysis (rise/fall/width in ns).
- **Advanced plotting:**
  - Overlaid ADC diagrams (oscilloscope style)
  - Statistical pulse diagrams (mean, std, envelope)
  - Pulse timing analysis with annotated rise/fall/width (in ns)
  - **Zoomed-in pulse timing plots** with all timing markers
  - **Timing vs HV plots** - Plot rise/fall/width time as function of PMT HV grouped by source and scintillator
- All plots use nanoseconds (ns) for time axis.

## Directory Structure
```
pyproject.toml
README.md
Python/
  d3df_single_pmt/
    __init__.py
    converter.py
    analysis.py
    plot_analysis.py
    plot_timing_vs_hv.py
    dsa_converter.py
    caen_hv.py
```

## Installation
### Standard install
```powershell
pip install .
```
### Editable install (development)
```powershell
pip install -e .
```

## CLI Usage
After installation, the following console scripts are available:
- `convert-dt5743` — Convert CSV to HDF5
- `analyze-dt-hdf` — Analyze HDF5 files (timing, normalization, etc.)
- `plot-analysis` — Generate all analysis and timing plots
- `plot-timing-vs-hv` — Plot timing parameters vs PMT HV from analysis results
- `caen-hv` — Control CAEN HV via serial
- `digitizer-web` — Launch FastAPI web interface (HV control + measurement batching)

## Web Interface (`digitizer-web`)
Start the server:
```powershell
digitizer-web
```
Then open: http://localhost:8000/docs for interactive Swagger UI.

### HV Endpoints
- `POST /hv/set` — Set HV (VSET). Body: `{ "value": -1800 }`
- `GET /hv/read` — Read current HV (VMON)
- `POST /hv/send` — Raw command (e.g. `{ "cmd": "MON", "par": "VMON" }`)
- `WS /ws/hv` — Live HV monitoring. Example (browser console):
```javascript
const ws = new WebSocket('ws://localhost:8000/ws/hv?interval=2');
ws.onmessage = e => console.log(JSON.parse(e.data));
```

### Measurement Control
- `POST /measure/start` — Start batch measurement. Example body:
```json
{
  "yaml": "config.yaml",
  "data_output": "./data_output",
  "exe": "WaveDemo_x743.exe",
  "batch_mode": 2,
  "max_events": 0,
  "max_time": 30,
  "hv_sequence": [-1800, -1700],
  "thresholds": [-0.10, -0.20],
  "repeat": 2
}
```
- `POST /measure/stop/{id}` — Stop measurement loop
- `GET /measure/status` — Snapshot of all tasks
- `WS /ws/measure/{id}` — Live progress JSON (elapsed, events, rate, hv, threshold)

Progress lines (heuristic) derived from underlying runner output; events parsed from lines containing the word "events".
Rate = events / elapsed seconds.

### Notes
- HV values auto-coerced to negative if positive provided.
- For infinite measurement loop, send `"repeat": -1`.
- Threshold / HV sweeping: All combinations iterated per repeat.
- Extend parsing logic in `webapp.py` if WaveDemo output format differs.

### Example: Convert and plot
```powershell
convert-dt5743 data_output
analyze-dt-hdf data_output
plot-analysis data_output
```

### Example: Plot timing vs HV
After running `analyze-dt-hdf`, use the generated `analysis_results_*.h5` file:
```powershell
plot-timing-vs-hv analysis_results_20251123_120000.h5
# Or auto-detect the most recent file:
plot-timing-vs-hv
```
This creates plots showing rise/fall/width time (ns) vs PMT HV, grouped by source and scintillator.

### Plotting options
```powershell
plot-analysis <folder> [--alpha 0.05] [--max-pulses 1000] [--no-normalize] [--norm-method individual|global|baseline] [--no-align] [--overlay]
```
- Generates PNGs for each HDF5 file:
  - `<prefix>_ADC_diagram_normalized.png` — All pulses, mean, std envelope
  - `<prefix>_pulse_timing_analysis.png` — Pulse timing with annotated rise/fall/width (ns)
  - `<prefix>_pulse_timing_zoom.png` — **Zoomed-in timing plot** with all timing markers and scatter points
  - `<prefix>_ADC_overlay.png` — Overlaid pulses (with `--overlay` flag)

## Python API
```python
from d3df_single_pmt import convert_csv_to_hdf5, analyze_pulse_timing, plot_waveform_analysis

# Convert a file
h5_path = convert_csv_to_hdf5("D:/path/Wave_0_0.txt")

# Analyze and plot
plot_waveform_analysis(h5_path, output_folder=".")
```

## HDF5 Output
Returned HDF5 file has attributes and datasets:
```python
import h5py
with h5py.File(h5_path, 'r') as f:
    print(list(f.attrs.items()))
    print(list(f.keys()))  # ADC, timestamps, etc.
```

## Building Distributions
```powershell
pip install build
python -m build
```

## Uninstall
```powershell
pip uninstall d3df-single-pmt
```

## Updating
```powershell
pip install --upgrade .
```

## Troubleshooting
- If `pip build .` was attempted: correct command is `python -m build` (after `pip install build`).
- Ensure `README.md` exists or remove `readme = "README.md"` from `pyproject.toml`.
- Long lines lint warnings do not affect functionality; can be cleaned later.

## License
Proprietary (as set in `pyproject.toml`). Adjust as needed.

## Next Steps
- Add unit tests for parsing and plotting.
- Optional: extend plotting for multi-channel or batch workflows.
