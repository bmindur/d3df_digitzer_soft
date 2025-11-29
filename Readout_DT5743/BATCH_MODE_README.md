# Batch Mode Feature for WaveDemo x743

## Overview

Batch mode allows the WaveDemo x743 program to run automatically without user interaction, stopping when specified conditions are met. This is useful for automated data collection, testing, and long-running experiments.

Batch mode can be configured either through the configuration file or via command-line arguments (which override the config file settings).

## Batch Mode Options

The batch mode can be configured in three ways:

- **Mode 0 (Interactive)**: Default mode with full keyboard control
- **Mode 1 (Batch with visualization)**: Auto-starts acquisition, enables plotting and statistics, stops automatically
- **Mode 2 (Batch without visualization)**: Auto-starts acquisition, minimal output, no plotting, stops automatically

## Configuration Methods

### Method 1: Configuration File

Add these parameters to the `[OPTIONS]` section of `WaveDemoConfig.ini`:

```ini
# BATCH_MODE: Run mode for the program
# options: 0 = interactive (default)
#          1 = batch with visualization
#          2 = batch without visualization
BATCH_MODE = 0

# BATCH_MAX_EVENTS: Maximum number of events before auto-stop (0 = unlimited)
BATCH_MAX_EVENTS = 0

# BATCH_MAX_TIME: Maximum time in seconds before auto-stop (0 = unlimited)
BATCH_MAX_TIME = 0
```

### Method 2: Command-Line Arguments

Command-line arguments override the configuration file settings:

```bash
# Basic options
--help, -h              Show help message with all options
--version               Print program version

# Batch mode control
--batch                 Enable batch mode 2 (no visualization)
--batch-mode <0|1|2>    Set batch mode explicitly
--max-events <N>        Maximum events to record (overrides config)
--max-time <seconds>    Maximum time in seconds (overrides config)
--output-path <path>    Output data path (overrides config)
```

## Termination Conditions

The acquisition automatically stops when **EITHER** of the following conditions is met:

1. **Event Count**: Total events across all enabled channels reaches `BATCH_MAX_EVENTS`
2. **Time Limit**: Elapsed time reaches `BATCH_MAX_TIME` seconds
3. **Manual Stop**: 
   - Mode 0 & 1: Press 's' (stop/start toggle) or 'q' (quit)
   - Mode 2: Press 'q' or 's' for soft stop (saves data and exits gracefully)

Set either parameter to `0` to disable that condition. At least one condition should be set to a non-zero value for automatic termination.

## Usage Examples

### Configuration File Examples

### Example 1: Collect 10,000 events with visualization
```ini
BATCH_MODE = 1
BATCH_MAX_EVENTS = 10000
BATCH_MAX_TIME = 0
```

### Example 2: Run for 5 minutes without visualization
```ini
BATCH_MODE = 2
BATCH_MAX_EVENTS = 0
BATCH_MAX_TIME = 300
```

### Example 3: Run for up to 1 hour or 100,000 events (whichever comes first)
```ini
BATCH_MODE = 1
BATCH_MAX_EVENTS = 100000
BATCH_MAX_TIME = 3600
```

### Command-Line Examples

### Example 1: Quick batch run with 10,000 events (no visualization)
```bash
WaveDemo_x743.exe --batch --max-events 10000
```

### Example 2: Batch run for 5 minutes with custom output path
```bash
WaveDemo_x743.exe --batch --max-time 300 --output-path ./my_experiment/
```

### Example 3: Use custom config with command-line overrides
```bash
WaveDemo_x743.exe myconfig.ini --batch-mode 1 --max-events 50000 --max-time 600
```

### Example 4: Override only output path (use config for batch settings)
```bash
WaveDemo_x743.exe --output-path D:/data/run_2025_11_26/
```

### Example 5: Full command-line control
```bash
WaveDemo_x743.exe myconfig.ini --batch-mode 2 --max-events 100000 --max-time 3600 --output-path ./batch_data/
```

### Example 6: Interactive mode (default behavior)
```ini
BATCH_MODE = 0
BATCH_MAX_EVENTS = 0
BATCH_MAX_TIME = 0
```

## Behavior Details

### Mode 1 (Batch with visualization):
- Acquisition starts automatically when program launches
- Waveform and histogram plots are enabled (if configured)
- **Statistics are updated and displayed every second**
- **Progress messages printed every 10 seconds**
- Keyboard commands still work (can manually stop or adjust)
- **Run info file automatically saved at completion**
- Program exits automatically when conditions are met

### Mode 2 (Batch without visualization):
- Acquisition starts automatically when program launches
- No waveform or histogram plotting
- **Statistics updated but not printed to console (except progress every 10 seconds)**
- **Soft stop: Press 'q' or 's' to stop acquisition early**
- **Run info file automatically saved at completion**
- Program exits automatically when conditions are met
- Ideal for headless/automated environments

## Progress Monitoring

In batch mode, the program prints progress updates showing:
- Elapsed time vs. maximum time
- Total events collected
- Automatic termination message when limits are reached

### Startup Information (Batch Mode):
```
========================================
BATCH MODE ENABLED (Mode 1)
========================================
  Maximum events: 10000
  Maximum time: 300 seconds
  Visualization: ENABLED
  Output path: ./data_output/
========================================

Output files being created:
  - Raw data file
  - TDC list files
  - Waveform files
  - List files
  - Histogram files
  - Run info file
All files in: ./data_output/
```

### Progress Updates (Every 10 seconds):
```
Batch mode progress: 10/300 seconds, 523 events
Batch mode progress: 20/300 seconds, 1047 events
Batch mode progress: 30/300 seconds, 1589 events
...
```

### Completion Summary:
```
Batch mode: Maximum event count reached (10000 events)

========================================
BATCH MODE COMPLETED
========================================
[Statistics table showing detailed per-channel performance]

Output files saved in: ./data_output/
========================================
```

## Output Files

All configured output files (raw data, waveforms, histograms, lists) are saved normally in batch mode, following the settings in the configuration file.

**Important:** In batch mode, the `SAVE_RUN_INFO` setting is automatically forced to `YES`, ensuring that a run info file is always created with:
- Run configuration details
- Copy of the configuration file used
- Acquisition statistics
- Timestamp and duration information

At the end of the batch run, the program displays the output directory where all files have been saved.

## Technical Details

### Implementation
- New function `CheckBatchModeConditions()` checks termination conditions each loop iteration
- Leverages existing `StartAcquisition()` and `StopAcquisition()` functions
- Integrates with existing event processing and file writing infrastructure
- Minimal code duplication - reuses `CheckKeyboardCommands()` logic

### Data Structures
- `WaveDemoConfig_t`: Added `BatchMode`, `BatchMaxEvents`, `BatchMaxTime` fields
- `WaveDemoRun_t`: Added `BatchStartTime`, `BatchEventsTotal` runtime tracking

### Parsing
- Config file parsing added to `parseOptions()` in `WDconfig.c`
- Default values set in `SetDefaultConfiguration()`

## Notes

- In batch mode 2 (without visualization), gnuplot is not invoked, reducing CPU/memory usage
- Event counting includes all enabled channels across all boards
- Time measurement uses system time (millisecond precision)
- The program gracefully closes files and cleans up resources on automatic termination
- Batch mode respects all other configuration settings (triggers, channels, file saving, etc.)
- **`SAVE_RUN_INFO` is automatically enabled in batch mode (forced to YES)**
- **Statistics are displayed at completion showing event counts, rates, and throughput**
- **Output file locations are printed at startup and completion for easy reference**
- **Command-line arguments override configuration file settings**
- **Command-line overrides are logged to the message log file**

## Command-Line Priority

When both configuration file and command-line arguments are provided:
1. Configuration file is loaded first
2. Command-line arguments override the corresponding config file values
3. All other settings from config file remain unchanged
4. Overrides are logged to MsgLog.txt for tracking

## Version

Added in: WaveDemo Release 1.2.2_BM (2025-11-26)
