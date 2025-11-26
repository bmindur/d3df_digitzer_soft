# Batch Mode Implementation Summary

## Changes Made

### 1. Header File Modifications (`WaveDemo.h`)

**Added to `WaveDemoConfig_t` structure:**
```c
// Batch mode parameters
int BatchMode;          // 0=interactive, 1=batch with visualization, 2=batch without visualization
uint64_t BatchMaxEvents; // Maximum number of events to record (0=unlimited)
uint64_t BatchMaxTime;   // Maximum time in seconds (0=unlimited)
```

**Added to `WaveDemoRun_t` structure:**
```c
// Batch mode runtime variables
uint64_t BatchStartTime;    // Start time for batch mode in ms
uint64_t BatchEventsTotal;  // Total events processed in batch mode
```

### 2. Core Functionality (`WaveDemo.c`)

**New Function: `CheckBatchModeConditions()`**
- Similar to `CheckKeyboardCommands()` but checks time and event limits
- Returns 0 to stop acquisition, -1 to continue
- Monitors total events across all boards/channels
- Prints progress messages every 10 seconds
- Stops acquisition when limits are reached

**Main Loop Modifications:**
- Auto-starts acquisition in batch mode
- Skips keyboard checking in batch mode 2 (no visualization)
- Calls `CheckBatchModeConditions()` when in batch mode
- Automatically exits when batch conditions met
- Disables plotter initialization in batch mode 2
- Conditionally prints statistics based on batch mode

**Startup Modifications:**
- Prints batch mode configuration at startup
- Initializes batch mode timers
- Auto-starts acquisition if batch mode enabled

**Statistics Updates:**
- Forces statistics updates in batch mode
- Skips printing statistics in batch mode 2
- Disables "press 'f' to enable" message in batch mode

**Plotting Modifications:**
- Skips plotter initialization in batch mode 2
- Disables histogram plotting in batch mode 2
- Reduces CPU/memory usage when visualization disabled

### 3. Configuration Parsing (`WDconfig.c`)

**Default Configuration:**
```c
WDcfg->BatchMode = 0;           // Interactive mode (default)
WDcfg->BatchMaxEvents = 0;      // Unlimited
WDcfg->BatchMaxTime = 0;        // Unlimited
```

**Config File Parsing (`parseOptions()`):**
- Added parsing for `BATCH_MODE` (validates 0-2)
- Added parsing for `BATCH_MAX_EVENTS` (uint64_t)
- Added parsing for `BATCH_MAX_TIME` (uint64_t)
- Input validation for batch mode parameter

### 4. Configuration File (`WaveDemoConfig.ini`)

Added new section with documentation:
```ini
##                  ##
### Batch mode      ##
##                  ##

# BATCH_MODE: Run mode for the program
# options: 0 = interactive (default - normal keyboard control)
#          1 = batch with visualization (auto-start, stops when conditions met, plots enabled)
#          2 = batch without visualization (auto-start, stops when conditions met, no plots/minimal output)
BATCH_MODE = 0

# BATCH_MAX_EVENTS: Maximum number of events to record before auto-stopping (0 = unlimited)
# Note: This counts total events across all enabled channels
BATCH_MAX_EVENTS = 0

# BATCH_MAX_TIME: Maximum time in seconds before auto-stopping (0 = unlimited)
# Note: Both time and event conditions can be set; acquisition stops when EITHER condition is met
BATCH_MAX_TIME = 0
```

### 5. Documentation (`BATCH_MODE_README.md`)

Created comprehensive documentation covering:
- Overview and purpose
- Configuration parameters
- Termination conditions
- Usage examples
- Behavior details for each mode
- Progress monitoring
- Technical implementation details

## Key Features

### Reuse of Existing Functionality
- Leverages `StartAcquisition()` and `StopAcquisition()`
- Uses existing event processing pipeline
- Integrates with existing statistics system
- Works with all existing file saving mechanisms
- Compatible with all trigger modes and configurations

### Minimal Code Changes
- One new function (`CheckBatchModeConditions`)
- Strategic modifications to main loop
- Conditional logic based on batch mode flags
- No breaking changes to existing functionality

### Flexible Operation
- Three distinct modes for different use cases
- Independent time and event limits
- Can set both limits (stops when either is reached)
- Can set one or neither limit
- Backward compatible (mode 0 = original behavior)

### Resource Optimization
- Mode 2 disables plotting for reduced CPU usage
- Mode 2 disables gnuplot invocation
- Statistics still collected but not printed
- All data still saved to files

## Testing Recommendations

1. **Interactive Mode (Mode 0):**
   - Verify normal operation unchanged
   - Test all keyboard commands still work

2. **Batch with Visualization (Mode 1):**
   - Test event count limit
   - Test time limit
   - Test both limits together
   - Verify plots work correctly
   - Test keyboard override

3. **Batch without Visualization (Mode 2):**
   - Verify no plots created
   - Test event count limit
   - Test time limit
   - Verify reduced CPU usage
   - Confirm files still saved correctly

## Build Status

✅ **Compilation Successful**
- No build errors
- Only pre-existing warnings (70 warnings related to potentially uninitialized variables)
- Release configuration builds successfully

## Files Modified

1. `Readout_DT5743/include/WaveDemo.h` - Added batch mode structure fields
2. `Readout_DT5743/src/WaveDemo.c` - Added batch mode logic
3. `Readout_DT5743/src/WDconfig.c` - Added config parsing
4. `Readout_DT5743/bin/WaveDemoConfig.ini` - Added config documentation

## Files Created

1. `Readout_DT5743/BATCH_MODE_README.md` - User documentation

## Version

Implementation Date: 2025-11-26
Target Release: WaveDemo 1.2.2_BM
