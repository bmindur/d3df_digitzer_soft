# WaveDemo x743 - Batch Mode Quick Reference

## Command-Line Syntax

```bash
WaveDemo_x743.exe [ConfigFile] [Options]
```

## Quick Start Commands

### Run batch mode (no visualization) for 10,000 events
```bash
WaveDemo_x743.exe --batch --max-events 10000
```

### Run batch mode for 5 minutes
```bash
WaveDemo_x743.exe --batch --max-time 300
```

### Run with custom output directory
```bash
WaveDemo_x743.exe --batch --max-events 5000 --output-path ./my_data/
```

### Use custom config + batch mode override
```bash
WaveDemo_x743.exe myconfig.ini --batch-mode 2 --max-events 10000
```

## All Command-Line Options

| Option | Arguments | Description |
|--------|-----------|-------------|
| `--help` or `-h` | None | Show help message |
| `--version` | None | Print program version |
| `--batch` | None | Enable batch mode 2 (no visualization) |
| `--batch-mode` | `0`, `1`, or `2` | Set batch mode explicitly |
| `--max-events` | Number | Maximum events to record |
| `--max-time` | Seconds | Maximum time to run |
| `--output-path` | Path | Output directory path (will auto-add trailing separator) |

**Note:** The output path is automatically normalized to end with a path separator (`\` on Windows, `/` on Linux), so you can specify it with or without the trailing separator.

## Batch Modes

| Mode | Name | Description | Plots | Keyboard | Statistics |
|------|------|-------------|-------|----------|-----------|
| 0 | Interactive | Normal operation | Yes | Yes | Yes |
| 1 | Batch with vis | Auto-run with plots | Yes | Yes | Yes |
| 2 | Batch no vis | Auto-run headless | No | No | Minimal |

## Override Behavior

Command-line arguments **always override** config file settings:

1. Load configuration file
2. Apply command-line overrides
3. Start acquisition

Example: Config has `BATCH_MODE=0`, command has `--batch` → Result: Mode 2 is used

## Typical Use Cases

### Automated Testing
```bash
WaveDemo_x743.exe test_config.ini --batch --max-events 1000 --output-path ./test_results/
```

### Long Run (Overnight)
```bash
WaveDemo_x743.exe --batch-mode 2 --max-time 28800 --output-path ./overnight_data/
```

### Quick Data Collection
```bash
WaveDemo_x743.exe --batch --max-events 5000
```

### Paths with or without trailing separator (both work)
```bash
# Both of these are equivalent:
WaveDemo_x743.exe --batch --output-path ./my_data
WaveDemo_x743.exe --batch --output-path ./my_data/

# Windows paths also work:
WaveDemo_x743.exe --batch --output-path D:\Data\Experiment1
WaveDemo_x743.exe --batch --output-path D:\Data\Experiment1\
```

### Scheduled Acquisition (Windows Task Scheduler)
```bash
C:\Path\To\WaveDemo_x743.exe C:\Config\acquisition.ini --batch-mode 2 --max-time 3600 --output-path C:\Data\%DATE%\
```

## Output

All runs create output in the specified data path:
- Raw data files (`.dat`)
- Waveform files (`.txt` or `.bin`)
- Histogram files (`.txt`)
- List files (`.txt`)
- TDC list files (`.txt`)
- **Run info file (`.txt`)** - always created in batch mode

## Stop Conditions

Acquisition stops when **first** condition is met:
- Maximum events reached
- Maximum time elapsed
- Manual stop (Mode 0 and 1 only via keyboard)

Setting a parameter to `0` means unlimited (must have at least one limit set for auto-stop).

## Help Command

For full help message:
```bash
WaveDemo_x743.exe --help
```

Output:
```
Syntax: WaveDemo_x743.exe [options] [ConfigFileName]

Options:
  ConfigFileName              : configuration file (default is WaveDemoConfig.ini)
  --version                   : Print program version
  -h, --help                  : Show this help message

Batch Mode Options:
  --batch                     : Enable batch mode 2 (no visualization)
  --batch-mode <0|1|2>        : Set batch mode (0=interactive, 1=with vis, 2=no vis)
  --max-events <N>            : Maximum events to record (overrides config)
  --max-time <seconds>        : Maximum time in seconds (overrides config)
  --output-path <path>        : Output data path (overrides config)

Examples:
  WaveDemo_x743.exe --batch --max-events 10000 --output-path ./my_data/
  WaveDemo_x743.exe myconfig.ini --batch-mode 1 --max-time 300
```
