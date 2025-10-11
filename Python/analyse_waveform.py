import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os
import h5py


def load_hdf5_data(hdf5_file):
    """
    Load data from HDF5 file created by convert_to_h5_DT5743.py.
    
    Args:
        hdf5_file: Path to the HDF5 file
    
    Returns:
        ADC_df, timestamps_df, metadata (pandas DataFrames, dict)
        - ADC_df: ADC data with voltage scaling applied
        - timestamps_df: Event timestamps corresponding to each ADC row
        - metadata: HDF5 file attributes including sampling_rate
    """
    
    try:
        with h5py.File(hdf5_file, 'r') as f:
            # Load datasets
            timestamps = f['timestamps'][:]
            adc_data = f['adc_data'][:]
            
            # Load metadata from attributes
            metadata = {}
            for key in f.attrs.keys():
                metadata[key] = f.attrs[key]
            
            print(f"Loaded HDF5 file: {hdf5_file}")
            print(f"  Number of events: {metadata.get('num_events', len(timestamps))}")
            print(f"  Samples per event: {metadata.get('num_samples_per_event', adc_data.shape[1])}")
            print(f"  Sampling rate: {metadata.get('sampling_rate', 'Unknown')} Hz")
            print(f"  ADC voltage scaling: {metadata.get('adc_voltage_scaling', 'Unknown')} V/count")
            print(f"  Source file: {metadata.get('source_file', 'Unknown')}")
            
    except FileNotFoundError:
        print(f"HDF5 file '{hdf5_file}' not found")
        return None, None, None
    except Exception as e:
        print(f"Error loading HDF5 file: {e}")
        return None, None, None
    
    # Convert to pandas DataFrames
    # Calculate timing information from sampling rate
    sampling_rate = metadata.get('sampling_rate', 3.2e9)
    time_per_sample = 1.0 / sampling_rate
    n_samples = adc_data.shape[1]
    time_axis = np.arange(n_samples) * time_per_sample
    
    # Create timestamps_df with event timestamps
    timestamps_df = pd.DataFrame(
        {'timestamp': timestamps},
        index=pd.Index(range(len(timestamps)), name='Event')
    )
    
    # Apply voltage scaling to ADC data if available
    adc_voltage_scaling = metadata.get('adc_voltage_scaling', 1.0)
    scaled_adc_data = adc_data * adc_voltage_scaling
    
    ADC_df = pd.DataFrame(
        scaled_adc_data,
        index=pd.Index(range(len(timestamps)), name='Event'),
        columns=pd.Index(range(n_samples), name='Sample')
    )
    
    print(f"Created DataFrames:")
    print(f"  ADC_df shape: {ADC_df.shape}")
    print(f"  timestamps_df shape: {timestamps_df.shape}")
    print(f"  Time range: {time_axis[0]:.9e} to {time_axis[-1]:.9e} s")
    print(f"  ADC data range: {scaled_adc_data.min():.6f} to {scaled_adc_data.max():.6f} V")
    print(f"  Event timestamps [s]: {timestamps[0]/1e9:.6f} to {timestamps[-1]/1e9:.6f}")
    
    return ADC_df, timestamps_df, metadata



def demonstrate_dataframe_operations(ADC_df):
    """
    Show example operations with the pandas DataFrames.
    """
    
    print("\n" + "="*50)
    print("PANDAS DATAFRAME OPERATIONS")
    print("="*50)
    
    if ADC_df is not None:
        print("\nADC DataFrame Operations:")
        print(f"Shape: {ADC_df.shape}")
        print(f"Mean per row (event): \n{ADC_df.mean(axis=1).head()}")
        print(f"Standard deviation per row: \n{ADC_df.std(axis=1).head()}")
        print(f"Maximum value per row: \n{ADC_df.max(axis=1).head()}")
        
        # Example: Get statistics summary
        print("\nStatistics summary for ADC data:")
        stats = ADC_df.describe().T  # Transpose to get stats per sample
        print(stats.head())


def plot_dataframe_data(ADC_df, prefix, metadata=None):
    """
    Create plots from the DataFrame data.
    """
    
    fig, axes = plt.subplots(1, figsize=(12, 8))
    fig.suptitle(f'DataFrame Analysis: {prefix}')
    
    # Get time axis from metadata if available
    if metadata is not None and 'sampling_rate' in metadata:
        sampling_rate = metadata['sampling_rate']
        time_per_sample = 1.0 / sampling_rate
        x_axis = np.arange(ADC_df.shape[1]) * time_per_sample
        x_label = 'Time (s)'
    else:
        x_axis = range(ADC_df.shape[1])
        x_label = 'Sample Points'

    # Plot first few ADC events
    if ADC_df is not None:
        ax2 = axes
        n_plot = min(5, ADC_df.shape[0])
        for i in range(n_plot):
            ax2.plot(x_axis, ADC_df.iloc[i, :], 's-', 
                    label=f'Event {i}', alpha=0.7, markersize=3)
        ax2.set_xlabel(x_label)
        ax2.set_ylabel('ADC Values')
        ax2.set_title('First 5 ADC Events')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
    plt.tight_layout()
    plt.savefig(f'{prefix}_dataframe_analysis.png', dpi=300, bbox_inches='tight')
    plt.show()
    print(f"Saved plot: {prefix}_dataframe_analysis.png")


def plot_adc_diagram(ADC_df, prefix, alpha=0.1, max_pulses=None):
    """
    Create an diagram-style plot showing all ADC pulses overlaid.
    Similar to oscilloscope diagram - all pulses plotted on top of each other.
    
    Args:
        ADC_df: pandas DataFrame where each row is an ADC pulse/channel
        prefix: prefix for saving the plot
        alpha: transparency for individual pulses (0.1 = very transparent)
        max_pulses: maximum number of pulses to plot (None = plot all)
    """
    
    if ADC_df is None:
        print("No ADC DataFrame available for diagram")
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 8))
    
    # Determine how many pulses to plot
    n_pulses = ADC_df.shape[0]
    if max_pulses is not None:
        n_pulses = min(n_pulses, max_pulses)
    
    # Create x-axis (sample points)
    x_axis = range(ADC_df.shape[1])
    
    # Plot all pulses overlaid
    for i in range(n_pulses):
        ax.plot(x_axis, ADC_df.iloc[i, :], 'b-', alpha=alpha, linewidth=0.5)
    
    # Calculate and plot average pulse
    avg_pulse = ADC_df.iloc[:n_pulses, :].mean(axis=0)
    ax.plot(x_axis, avg_pulse, 'r-', linewidth=2, label=f'Average ({n_pulses} pulses)')
    
    # Calculate and plot standard deviation envelope
    std_pulse = ADC_df.iloc[:n_pulses, :].std(axis=0)
    ax.fill_between(x_axis, 
                    avg_pulse - std_pulse, 
                    avg_pulse + std_pulse, 
                    alpha=0.2, color='red', label='±1σ envelope')
    
    ax.set_xlabel('Sample Points')
    ax.set_ylabel('ADC Values')
    ax.set_title(f'ADC Diagram - {n_pulses} Pulses Overlaid')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{prefix}_ADC_diagram.png', dpi=300, bbox_inches='tight')
    plt.show()
    print(f"Saved ADC diagram: {prefix}_ADC_diagram.png")


def normalize_pulses_to_max(ADC_df, method='individual'):
    """
    Normalize pulses to their maximum amplitude.
    
    Args:
        ADC_df: pandas DataFrame where each row is an ADC pulse
        method: 'individual' - normalize each pulse to its own max (amplitude=1)
                'global' - normalize all pulses to the global maximum
                'baseline' - subtract baseline (min) then normalize to max
    
    Returns:
        normalized_df: pandas DataFrame with normalized pulses
    """
    
    if ADC_df is None or ADC_df.empty:
        print("No ADC data available for normalization")
        return None
    
    normalized_df = ADC_df.copy()
    
    if method == 'individual':
        # Normalize each pulse to its own maximum (0 to 1 range)
        for i in range(ADC_df.shape[0]):
            pulse = ADC_df.iloc[i, :]
            pulse_max = pulse.max()
            pulse_min = pulse.min()
            if pulse_max != pulse_min:  # Avoid division by zero
                normalized_df.iloc[i, :] = (pulse - pulse_min) / (pulse_max - pulse_min)
            else:
                normalized_df.iloc[i, :] = 0  # Flat pulse becomes zero
                
    elif method == 'global':
        # Normalize all pulses to the global maximum
        global_max = ADC_df.values.max()
        global_min = ADC_df.values.min()
        if global_max != global_min:
            normalized_df = (ADC_df - global_min) / (global_max - global_min)
        else:
            normalized_df = ADC_df * 0  # All data is the same value
            
    elif method == 'baseline':
        # Subtract baseline (minimum) from each pulse, then normalize to max
        for i in range(ADC_df.shape[0]):
            pulse = ADC_df.iloc[i, :]
            baseline = pulse.min()
            baseline_corrected = pulse - baseline
            pulse_max = baseline_corrected.max()
            if pulse_max > 0:
                normalized_df.iloc[i, :] = baseline_corrected / pulse_max
            else:
                normalized_df.iloc[i, :] = 0
    
    return normalized_df


def plot_adc_diagram_normalized(ADC_df, prefix, normalize=True, 
                               norm_method='individual', alpha=0.1, max_pulses=None):
    """
    Create an eye diagram with optional pulse normalization.
    
    Args:
        ADC_df: pandas DataFrame where each row is an ADC pulse
        prefix: prefix for saving the plot
        normalize: whether to normalize pulses (True/False)
        norm_method: 'individual', 'global', or 'baseline'
        alpha: transparency for individual pulses
        max_pulses: maximum number of pulses to plot
    """
    
    if ADC_df is None:
        print("No ADC DataFrame available for normalized diagram")
        return
    
    # Apply normalization if requested
    if normalize:
        plot_df = normalize_pulses_to_max(ADC_df, method=norm_method)
        norm_suffix = f"_normalized_{norm_method}"
        y_label = f"Normalized ADC Values ({norm_method})"
        title_suffix = f" - Normalized ({norm_method})"
    else:
        plot_df = ADC_df
        norm_suffix = "_raw"
        y_label = "ADC Values"
        title_suffix = " - Raw Values"
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 8))
    
    # Determine how many pulses to plot
    n_pulses = plot_df.shape[0]
    if max_pulses is not None:
        n_pulses = min(n_pulses, max_pulses)
    
    # Create x-axis (sample points)
    x_axis = range(plot_df.shape[1])
    
    # Plot all pulses overlaid
    for i in range(n_pulses):
        ax.plot(x_axis, plot_df.iloc[i, :], 'b-', alpha=alpha, linewidth=0.5)
    
    # Calculate and plot average pulse
    avg_pulse = plot_df.iloc[:n_pulses, :].mean(axis=0)
    ax.plot(x_axis, avg_pulse, 'r-', linewidth=2, 
            label=f'Average ({n_pulses} pulses)')
    
    # Calculate and plot standard deviation envelope
    std_pulse = plot_df.iloc[:n_pulses, :].std(axis=0)
    ax.fill_between(x_axis, 
                    avg_pulse - std_pulse, 
                    avg_pulse + std_pulse, 
                    alpha=0.2, color='red', label='±1σ envelope')
    
    ax.set_xlabel('Sample Points')
    ax.set_ylabel(y_label)
    ax.set_title(f'ADC Eye Diagram{title_suffix} - {n_pulses} Pulses')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Add normalization info as text
    if normalize:
        norm_info = f'Normalization: {norm_method}'
        ax.text(0.02, 0.02, norm_info, transform=ax.transAxes, 
                bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{prefix}_ADC_diagram{norm_suffix}.png', dpi=300, bbox_inches='tight')
    plt.show()
    print(f"Saved normalized ADC diagram: {prefix}_ADC_diagram{norm_suffix}.png")


def analyze_pulse_timing(ADC_df, sampling_rate, method='individual',
                         threshold_low=0.1, threshold_high=0.9):
    """
    Analyze rise time, fall time, and pulse width of the mean signal.
    Handles both positive and negative pulses automatically.
    Uses only sampling rate from HDF5 metadata for timing calculations.
    
    Args:
        ADC_df: pandas DataFrame where each row is an ADC pulse
        sampling_rate: sampling rate in Hz from HDF5 metadata
        method: normalization method ('individual', 'global', 'baseline')
        threshold_low: lower threshold for timing measurements (0.1 = 10%)
        threshold_high: upper threshold for timing measurements (0.9 = 90%)
    
    Returns:
        timing_info: dictionary with timing analysis results
    """
    
    if ADC_df is None or ADC_df.empty:
        print("No ADC data available for timing analysis")
        return None
    
    if sampling_rate is None or sampling_rate <= 0:
        print("No valid sampling rate available from HDF5 metadata - cannot perform timing analysis")
        return None
    
    # Calculate timing information from sampling rate
    sample_rate = sampling_rate
    time_per_sample = 1.0 / sample_rate
    n_samples = ADC_df.shape[1]
    time_axis = np.arange(n_samples) * time_per_sample
    
    print("Time information from HDF5 sampling rate:")
    print(f"  Sampling rate: {sample_rate:.1f} Hz")
    print(f"  Time per sample: {time_per_sample:.9e} s")
    print(f"  Time range: {time_axis[0]:.6e} to {time_axis[-1]:.6e} s")
    
    # Normalize the data for consistent threshold measurements
    normalized_df = normalize_pulses_to_max(ADC_df, method=method)
    
    # Calculate mean pulse
    mean_pulse = normalized_df.mean(axis=0).values
    x_axis = np.arange(len(mean_pulse))
    
    # Determine if pulse is positive or negative
    baseline = mean_pulse.min() if method == 'baseline' else np.median(mean_pulse[:10])  # Use first few samples as baseline estimate
    peak_max = mean_pulse.max()
    peak_min = mean_pulse.min()
    
    # Determine pulse polarity
    if abs(peak_max - baseline) > abs(peak_min - baseline):
        # Positive pulse: peak is maximum
        is_positive_pulse = True
        peak = peak_max
        peak_idx = np.argmax(mean_pulse)
        amplitude = peak - baseline
    else:
        # Negative pulse: peak is minimum (most negative)
        is_positive_pulse = False
        peak = peak_min
        peak_idx = np.argmin(mean_pulse)
        amplitude = abs(peak - baseline)  # Make amplitude positive for consistent thresholds
    
    # Calculate threshold levels based on pulse polarity
    if is_positive_pulse:
        low_level = baseline + amplitude * threshold_low
        high_level = baseline + amplitude * threshold_high
        mid_level = baseline + amplitude * 0.5
    else:
        # For negative pulses, thresholds are inverted
        low_level = baseline - amplitude * threshold_low
        high_level = baseline - amplitude * threshold_high
        mid_level = baseline - amplitude * 0.5
    
    timing_info = {
        'baseline': baseline,
        'peak': peak,
        'amplitude': amplitude,
        'is_positive_pulse': is_positive_pulse,
        'threshold_low': threshold_low,
        'threshold_high': threshold_high,
        'low_level': low_level,
        'high_level': high_level,
        'mid_level': mid_level,
        'peak_idx': peak_idx,
        'mean_pulse': mean_pulse,
        'x_axis': x_axis,
        # Time information - always available from HDF5 sampling rate
        'time_axis': time_axis,
        'sample_rate': sample_rate,
        'time_per_sample': time_per_sample,
        'has_time_data': True  # Always true when using HDF5 sampling rate
    }
    
    # Measure rise time (adjusted for pulse polarity)
    if is_positive_pulse:
        rise_measurements = measure_rise_time_positive(mean_pulse, low_level, high_level)
    else:
        rise_measurements = measure_rise_time_negative(mean_pulse, low_level, high_level)
    timing_info.update(rise_measurements)
    
    # Measure fall time (adjusted for pulse polarity)
    if is_positive_pulse:
        fall_measurements = measure_fall_time_positive(mean_pulse, low_level, high_level, peak_idx)
    else:
        fall_measurements = measure_fall_time_negative(mean_pulse, low_level, high_level, peak_idx)
    timing_info.update(fall_measurements)
    
    # Measure pulse width (works for both polarities)
    width_measurements = measure_pulse_width_universal(mean_pulse, mid_level, is_positive_pulse)
    timing_info.update(width_measurements)
    
    return timing_info


def measure_rise_time_positive(mean_pulse, low_level, high_level):
    """Measure rise time for positive pulses."""
    rise_start_idx = None
    rise_end_idx = None
    
    # Find first crossing of low threshold (rising edge)
    for i in range(1, len(mean_pulse)):
        if mean_pulse[i-1] < low_level and mean_pulse[i] >= low_level:
            rise_start_idx = i
            break
    
    # Find first crossing of high threshold after rise start
    if rise_start_idx is not None:
        for i in range(rise_start_idx, len(mean_pulse)):
            if mean_pulse[i] >= high_level:
                rise_end_idx = i
                break
    
    if rise_start_idx is not None and rise_end_idx is not None:
        rise_time = rise_end_idx - rise_start_idx
        return {
            'rise_start_idx': rise_start_idx,
            'rise_end_idx': rise_end_idx,
            'rise_time': rise_time
        }
    else:
        return {'rise_time': None}


def measure_rise_time_negative(mean_pulse, low_level, high_level):
    """Measure rise time for negative pulses (going more negative)."""
    rise_start_idx = None
    rise_end_idx = None
    
    # For negative pulses, "rise" means going more negative
    # low_level is less negative, high_level is more negative
    for i in range(1, len(mean_pulse)):
        if mean_pulse[i-1] > low_level and mean_pulse[i] <= low_level:
            rise_start_idx = i
            break
    
    if rise_start_idx is not None:
        for i in range(rise_start_idx, len(mean_pulse)):
            if mean_pulse[i] <= high_level:
                rise_end_idx = i
                break
    
    if rise_start_idx is not None and rise_end_idx is not None:
        rise_time = rise_end_idx - rise_start_idx
        return {
            'rise_start_idx': rise_start_idx,
            'rise_end_idx': rise_end_idx,
            'rise_time': rise_time
        }
    else:
        return {'rise_time': None}


def measure_fall_time_positive(mean_pulse, low_level, high_level, peak_idx):
    """Measure fall time for positive pulses."""
    fall_start_idx = None
    fall_end_idx = None
    
    # Find first crossing of high threshold (falling edge) after peak
    for i in range(peak_idx, len(mean_pulse)-1):
        if mean_pulse[i] >= high_level and mean_pulse[i+1] < high_level:
            fall_start_idx = i
            break
    
    if fall_start_idx is not None:
        for i in range(fall_start_idx, len(mean_pulse)):
            if mean_pulse[i] <= low_level:
                fall_end_idx = i
                break
    
    if fall_start_idx is not None and fall_end_idx is not None:
        fall_time = fall_end_idx - fall_start_idx
        return {
            'fall_start_idx': fall_start_idx,
            'fall_end_idx': fall_end_idx,
            'fall_time': fall_time
        }
    else:
        return {'fall_time': None}


def measure_fall_time_negative(mean_pulse, low_level, high_level, peak_idx):
    """Measure fall time for negative pulses (returning to baseline)."""
    fall_start_idx = None
    fall_end_idx = None
    
    # For negative pulses, "fall" means returning from negative peak to baseline
    # Start from peak and look for crossing back toward baseline
    for i in range(peak_idx, len(mean_pulse)-1):
        if mean_pulse[i] <= high_level and mean_pulse[i+1] > high_level:
            fall_start_idx = i
            break
    
    if fall_start_idx is not None:
        for i in range(fall_start_idx, len(mean_pulse)):
            if mean_pulse[i] >= low_level:
                fall_end_idx = i
                break
    
    if fall_start_idx is not None and fall_end_idx is not None:
        fall_time = fall_end_idx - fall_start_idx
        return {
            'fall_start_idx': fall_start_idx,
            'fall_end_idx': fall_end_idx,
            'fall_time': fall_time
        }
    else:
        return {'fall_time': None}


def measure_pulse_width_universal(mean_pulse, mid_level, is_positive_pulse):
    """Measure pulse width at 50% level for both positive and negative pulses."""
    width_start_idx = None
    width_end_idx = None
    
    if is_positive_pulse:
        # For positive pulses: find crossings of mid_level
        # First crossing going up
        for i in range(1, len(mean_pulse)):
            if mean_pulse[i-1] < mid_level and mean_pulse[i] >= mid_level:
                width_start_idx = i
                break
        
        # Last crossing going down
        for i in range(len(mean_pulse)-2, -1, -1):
            if mean_pulse[i] >= mid_level and mean_pulse[i+1] < mid_level:
                width_end_idx = i
                break
    else:
        # For negative pulses: find crossings of mid_level
        # First crossing going down (more negative)
        for i in range(1, len(mean_pulse)):
            if mean_pulse[i-1] > mid_level and mean_pulse[i] <= mid_level:
                width_start_idx = i
                break
        
        # Last crossing going up (less negative)
        for i in range(len(mean_pulse)-2, -1, -1):
            if mean_pulse[i] <= mid_level and mean_pulse[i+1] > mid_level:
                width_end_idx = i
                break
    
    if width_start_idx is not None and width_end_idx is not None:
        pulse_width = width_end_idx - width_start_idx
        return {
            'width_start_idx': width_start_idx,
            'width_end_idx': width_end_idx,
            'pulse_width': pulse_width
        }
    else:
        return {'pulse_width': None}


def print_pulse_timing_info(timing_info, prefix="ADC"):
    """
    Print detailed timing information about the pulse.
    Handles both positive and negative pulses.
    Uses timing information from HDF5 sampling rate.
    
    Args:
        timing_info: dictionary from analyze_pulse_timing()
        prefix: prefix for output identification
    """
    
    if timing_info is None:
        print("No timing information available")
        return
    
    pulse_type = "Positive" if timing_info['is_positive_pulse'] else "Negative"
    
    print("\n" + "="*60)
    print(f"PULSE TIMING ANALYSIS - {prefix} ({pulse_type} Pulse)")
    print("="*60)
    
    # Basic pulse characteristics
    print("Pulse Characteristics:")
    print(f"  Pulse type: {pulse_type}")
    print(f"  Baseline level: {timing_info['baseline']:.4f}")
    print(f"  Peak level: {timing_info['peak']:.4f}")
    print(f"  Amplitude: {timing_info['amplitude']:.4f}")
    print(f"  Peak position: sample {timing_info['peak_idx']}")
    
    # Timing measurements in samples
    print("\nTiming Measurements (in samples):")
    
    if timing_info['is_positive_pulse']:
        thresh_low = timing_info['threshold_low']*100
        thresh_high = timing_info['threshold_high']*100
        rise_desc = f"Rise time ({thresh_low:.0f}%-{thresh_high:.0f}%)"
        fall_desc = f"Fall time ({thresh_high:.0f}%-{thresh_low:.0f}%)"
    else:
        thresh_low = timing_info['threshold_low']*100
        thresh_high = timing_info['threshold_high']*100
        rise_desc = (f"Rise time (going negative: "
                     f"{thresh_low:.0f}%-{thresh_high:.0f}%)")
        fall_desc = (f"Fall time (returning to baseline: "
                     f"{thresh_high:.0f}%-{thresh_low:.0f}%)")
    
    if timing_info['rise_time'] is not None:
        print(f"  {rise_desc}: {timing_info['rise_time']} samples")
        print(f"    From sample {timing_info['rise_start_idx']} "
              f"to {timing_info['rise_end_idx']}")
    else:
        print(f"  {rise_desc}: Could not measure (no clear rise edge)")
    
    if timing_info['fall_time'] is not None:
        print(f"  {fall_desc}: {timing_info['fall_time']} samples")
        print(f"    From sample {timing_info['fall_start_idx']} "
              f"to {timing_info['fall_end_idx']}")
    else:
        print(f"  {fall_desc}: Could not measure (no clear fall edge)")
    
    if timing_info['pulse_width'] is not None:
        print(f"  Pulse width (50%-50%): {timing_info['pulse_width']} samples")
        print(f"    From sample {timing_info['width_start_idx']} "
              f"to {timing_info['width_end_idx']}")
    else:
        print("  Pulse width: Could not measure (no clear 50% crossings)")
    
    # Convert to time units - always available from HDF5 sampling rate
    tps = timing_info['time_per_sample']
    sr = timing_info['sample_rate']
    print("\nTiming Measurements (in real time units):")
    print(f"  Time per sample: {tps:.12e} s")
    print(f"  Sample rate: {sr:.1f} Hz")
    
    if timing_info['rise_time'] is not None:
        rise_time_sec = timing_info['rise_time'] * tps
        print(f"  Rise time: {rise_time_sec*1e9:.1f} ns "
              f"({rise_time_sec*1e6:.3f} us)")
    
    if timing_info['fall_time'] is not None:
        fall_time_sec = timing_info['fall_time'] * tps
        print(f"  Fall time: {fall_time_sec*1e9:.1f} ns "
              f"({fall_time_sec*1e6:.3f} us)")
    
    if timing_info['pulse_width'] is not None:
        width_time_sec = timing_info['pulse_width'] * tps
        print(f"  Pulse width: {width_time_sec*1e9:.1f} ns "
              f"({width_time_sec*1e6:.3f} us)")
    
    # Threshold levels used
    print("\nThreshold Levels Used:")
    if timing_info['is_positive_pulse']:
        thresh_low = timing_info['threshold_low']*100
        thresh_high = timing_info['threshold_high']*100
        print(f"  Low threshold ({thresh_low:.0f}%): "
              f"{timing_info['low_level']:.4f}")
        print(f"  High threshold ({thresh_high:.0f}%): "
              f"{timing_info['high_level']:.4f}")
    else:
        thresh_low = timing_info['threshold_low']*100
        thresh_high = timing_info['threshold_high']*100
        print(f"  Low threshold ({thresh_low:.0f}%): "
              f"{timing_info['low_level']:.4f} (less negative)")
        print(f"  High threshold ({thresh_high:.0f}%): "
              f"{timing_info['high_level']:.4f} (more negative)")
    
    if 'mid_level' in timing_info:
        print(f"  Mid level (50%): {timing_info['mid_level']:.4f}")


def plot_pulse_timing_analysis(timing_info, prefix, save_plot=True):
    """
    Plot the timing analysis with marked measurement points.
    
    Args:
        timing_info: dictionary from analyze_pulse_timing()
        prefix: prefix for saving the plot
        save_plot: whether to save the plot
    """
    
    if timing_info is None:
        print("No timing information available for plotting")
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(14, 8))
    
    mean_pulse = timing_info['mean_pulse']
    x_axis = timing_info['x_axis']
    
    # Use time axis if available, otherwise use sample indices
    has_time_data = timing_info.get('has_time_data', False)
    time_axis_data = timing_info.get('time_axis')
    if has_time_data and time_axis_data is not None:
        x_plot = time_axis_data
        x_label = 'Time (s)'
        title_suffix = ' (Real Time)'
    else:
        x_plot = x_axis
        x_label = 'Sample Points'
        title_suffix = ' (Sample Index)'
    
    # Plot the mean pulse
    ax.plot(x_plot, mean_pulse, 'b-', linewidth=2, label='Mean Pulse')
    
    # Add threshold lines
    thresh_low = timing_info['threshold_low']*100
    thresh_high = timing_info['threshold_high']*100
    ax.axhline(y=timing_info['low_level'], color='orange', linestyle='--',
               label=f"Low threshold ({thresh_low:.0f}%)")
    ax.axhline(y=timing_info['high_level'], color='red', linestyle='--',
               label=f"High threshold ({thresh_high:.0f}%)")
    
    if 'mid_level' in timing_info:
        ax.axhline(y=timing_info['mid_level'], color='green', linestyle='--',
                   label="Mid level (50%)")
    
    # Mark timing measurement points
    def get_x_coordinate(index):
        """Convert sample index to x-coordinate (time or sample)"""
        if has_time_data and time_axis_data is not None:
            return time_axis_data[index]
        else:
            return index
    
    def format_timing_label(timing_samples, timing_name):
        """Format timing labels with time units when available"""
        if has_time_data and timing_info.get('time_per_sample') is not None:
            time_sec = timing_samples * timing_info['time_per_sample']
            if time_sec >= 1e-6:  # microseconds or larger
                return f"{timing_name}: {time_sec*1e6:.1f} us"
            else:  # nanoseconds
                return f"{timing_name}: {time_sec*1e9:.1f} ns"
        else:
            return f"{timing_name}: {timing_samples} samples"
    
    def format_annotation(timing_samples):
        """Format annotation text with time units when available"""
        if has_time_data and timing_info.get('time_per_sample') is not None:
            time_sec = timing_samples * timing_info['time_per_sample']
            if time_sec >= 1e-6:  # microseconds or larger
                return f"{time_sec*1e6:.1f} us"
            else:  # nanoseconds
                return f"{time_sec*1e9:.1f} ns"
        else:
            return f"{timing_samples}"
    
    if timing_info['rise_time'] is not None:
        rise_start = timing_info['rise_start_idx']
        rise_end = timing_info['rise_end_idx']
        rise_start_x = get_x_coordinate(rise_start)
        rise_end_x = get_x_coordinate(rise_end)
        rise_label = format_timing_label(timing_info['rise_time'], "Rise time")
        ax.plot([rise_start_x, rise_end_x],
                [timing_info['low_level'], timing_info['high_level']],
                'ro-', markersize=8, label=rise_label)
        rise_annotation = format_annotation(timing_info['rise_time'])
        ax.annotate(f"Rise: {rise_annotation}",
                   xy=((rise_start_x + rise_end_x)/2,
                       timing_info['high_level']),
                   xytext=(10, 10), textcoords='offset points',
                   bbox=dict(boxstyle='round,pad=0.3',
                            facecolor='yellow', alpha=0.7))
    
    if timing_info['fall_time'] is not None:
        fall_start = timing_info['fall_start_idx']
        fall_end = timing_info['fall_end_idx']
        fall_start_x = get_x_coordinate(fall_start)
        fall_end_x = get_x_coordinate(fall_end)
        fall_label = format_timing_label(timing_info['fall_time'], "Fall time")
        ax.plot([fall_start_x, fall_end_x],
                [timing_info['high_level'], timing_info['low_level']],
                'mo-', markersize=8, label=fall_label)
        fall_annotation = format_annotation(timing_info['fall_time'])
        ax.annotate(f"Fall: {fall_annotation}",
                   xy=((fall_start_x + fall_end_x)/2,
                       timing_info['low_level']),
                   xytext=(10, -20), textcoords='offset points',
                   bbox=dict(boxstyle='round,pad=0.3',
                            facecolor='cyan', alpha=0.7))
    
    if timing_info['pulse_width'] is not None:
        width_start = timing_info['width_start_idx']
        width_end = timing_info['width_end_idx']
        width_start_x = get_x_coordinate(width_start)
        width_end_x = get_x_coordinate(width_end)
        width_label = format_timing_label(timing_info['pulse_width'],
                                          "Pulse width")
        ax.plot([width_start_x, width_end_x],
                [timing_info['mid_level'], timing_info['mid_level']],
                'go-', markersize=6, linewidth=3, label=width_label)
        width_annotation = format_annotation(timing_info['pulse_width'])
        ax.annotate(f"Width: {width_annotation}",
                   xy=((width_start_x + width_end_x)/2,
                       timing_info['mid_level']),
                   xytext=(0, 15), textcoords='offset points',
                   bbox=dict(boxstyle='round,pad=0.3',
                            facecolor='lightgreen', alpha=0.7))
    
    # Mark peak
    peak_idx = timing_info['peak_idx']
    if has_time_data and time_axis_data is not None:
        peak_x = time_axis_data[peak_idx]
    else:
        peak_x = peak_idx
    ax.plot(peak_x, timing_info['peak'], 'r*', markersize=15, label='Peak')
    
    ax.set_xlabel(x_label)
    ax.set_ylabel('Normalized ADC Values')
    ax.set_title(f'Pulse Timing Analysis - {prefix}{title_suffix}')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    if save_plot:
        plt.savefig(f'{prefix}_pulse_timing_analysis.png', dpi=300, bbox_inches='tight')
        print(f"Saved timing analysis plot: {prefix}_pulse_timing_analysis.png")
    
    # plt.show()


def plot_adc_diagram_advanced(ADC_df, prefix, alpha=0.05, max_pulses=10000, normalize=True, norm_method='individual', show=False):
    """
    Create an advanced diagram with multiple views and statistics.
    
    Args:
        ADC_df: pandas DataFrame where each row is an ADC pulse/channel
        prefix: prefix for saving the plot
        alpha: transparency for individual pulses
        max_pulses: maximum number of pulses to plot
    """
    
    if ADC_df is None:
        print("No ADC DataFrame available for advanced diagram")
        return
    
    ADC_df = normalize_pulses_to_max(ADC_df, method=norm_method) if normalize else ADC_df
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 10))
    fig.suptitle(f'ADC Diagram Analysis: {prefix}', fontsize=16)
    
    # Determine how many pulses to plot
    n_pulses = min(ADC_df.shape[0], max_pulses) if max_pulses else ADC_df.shape[0]
    x_axis = range(ADC_df.shape[1])
    
    # Plot 1: All pulses overlaid  diagram)
    ax1 = axes[0, 0]
    for i in range(n_pulses):
        ax1.plot(x_axis, ADC_df.iloc[i, :], 'b-', alpha=alpha, linewidth=0.3)
    
    # Add average
    avg_pulse = ADC_df.iloc[:n_pulses, :].mean(axis=0)
    ax1.plot(x_axis, avg_pulse, 'r-', linewidth=2, label=f'Average')
    ax1.set_xlabel('Sample Points')
    ax1.set_ylabel('ADC Values')
    ax1.set_title(f'ADC Diagram ({n_pulses} pulses)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Average pulse with error bars
    ax2 = axes[0, 1]
    std_pulse = ADC_df.iloc[:n_pulses, :].std(axis=0)
    ax2.errorbar(x_axis[::10], avg_pulse[::10], yerr=std_pulse[::10], 
                 fmt='ro-', capsize=3, alpha=0.7, markersize=3)
    ax2.plot(x_axis, avg_pulse, 'r-', linewidth=1)
    ax2.set_xlabel('Sample Points')
    ax2.set_ylabel('ADC Values')
    ax2.set_title('Average Pulse ± Standard Deviation')
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: Pulse statistics over time
    ax3 = axes[1, 0]
    ax3.plot(x_axis, avg_pulse, 'g-', linewidth=2, label='Mean')
    ax3.fill_between(x_axis, avg_pulse - std_pulse, avg_pulse + std_pulse, 
                     alpha=0.3, color='green', label='±1σ')
    ax3.fill_between(x_axis, avg_pulse - 2*std_pulse, avg_pulse + 2*std_pulse, 
                     alpha=0.2, color='yellow', label='±2σ')
    ax3.set_xlabel('Sample Points')
    ax3.set_ylabel('ADC Values')
    ax3.set_title('Statistical Envelope')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # Plot 4: First few individual pulses for comparison
    ax4 = axes[1, 1]
    n_individual = min(10, n_pulses)
    colors = plt.cm.tab10(range(n_individual))
    for i in range(n_individual):
        ax4.plot(x_axis, ADC_df.iloc[i, :], color=colors[i], 
                linewidth=1, alpha=0.8, label=f'Pulse {i}')
    ax4.plot(x_axis, avg_pulse, 'k--', linewidth=2, label='Average')
    ax4.set_xlabel('Sample Points')
    ax4.set_ylabel('ADC Values')
    ax4.set_title(f'Individual Pulses (first {n_individual})')
    if n_individual <= 5:
        ax4.legend()
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{prefix}_ADC_diagram_{"normalize" if normalize else "raw"}.png', dpi=300, bbox_inches='tight')
    plt.draw()
    print(f"Saved advanced ADC diagram: {prefix}_ADC_diagram.png")
    if show:
        plt.show()

def plot_adc_heatmap(ADC_df, prefix):
    """
    Create a heatmap of all ADC_df rows (channels) on a single plot.
    
    Args:
        ADC_df: pandas DataFrame with ADC data (each row is a channel)
        prefix: prefix for saving the plot
    """
    
    if ADC_df is None:
        print("No ADC DataFrame available for heatmap")
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(14, 8))
    
    # Create heatmap using imshow
    im = ax.imshow(ADC_df.values, aspect='auto', cmap='viridis', interpolation='nearest')
    
    # Set labels and title
    ax.set_xlabel('Data Points (Time/Frame)')
    ax.set_ylabel('ADC Channel Index')
    ax.set_title(f'ADC Channels Heatmap - All {ADC_df.shape[0]} Channels')
    
    # Add colorbar
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label('ADC Values')
    
    # Optional: Add some tick labels
    if ADC_df.shape[1] > 20:
        # Show fewer ticks for large datasets
        x_ticks = range(0, ADC_df.shape[1], max(1, ADC_df.shape[1]//10))
        ax.set_xticks(x_ticks)
    
    if ADC_df.shape[0] > 20:
        # Show fewer ticks for many channels
        y_ticks = range(0, ADC_df.shape[0], max(1, ADC_df.shape[0]//10))
        ax.set_yticks(y_ticks)
    
    plt.tight_layout()
    plt.savefig(f'{prefix}_ADC_heatmap.png', dpi=300, bbox_inches='tight')
    # plt.show()
    print(f"Saved ADC heatmap: {prefix}_ADC_heatmap.png")


def plot_adc_heatmap_advanced(ADC_df, prefix):
    """
    Create an advanced heatmap with statistics and multiple views of ADC data.
    
    Args:
        ADC_df: pandas DataFrame with ADC data
        prefix: prefix for saving the plot
    """
    
    if ADC_df is None:
        print("No ADC DataFrame available for advanced heatmap")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 10))
    fig.suptitle(f'ADC Data Analysis: {prefix}', fontsize=16)
    
    # Plot 1: Main heatmap
    ax1 = axes[0, 0]
    im1 = ax1.imshow(ADC_df.values, aspect='auto', cmap='viridis', interpolation='nearest')
    ax1.set_xlabel('Data Points')
    ax1.set_ylabel('ADC Channel')
    ax1.set_title(f'All {ADC_df.shape[0]} ADC Channels')
    plt.colorbar(im1, ax=ax1)
    
    # Plot 2: Channel means heatmap
    ax2 = axes[0, 1]
    channel_means = ADC_df.mean(axis=1).values.reshape(-1, 1)
    im2 = ax2.imshow(channel_means, aspect='auto', cmap='plasma')
    ax2.set_xlabel('Mean Value')
    ax2.set_ylabel('ADC Channel')
    ax2.set_title('Channel Mean Values')
    ax2.set_xticks([0])
    ax2.set_xticklabels(['Mean'])
    plt.colorbar(im2, ax=ax2)
    
    # Plot 3: Channel standard deviations
    ax3 = axes[1, 0]
    channel_stds = ADC_df.std(axis=1).values.reshape(-1, 1)
    im3 = ax3.imshow(channel_stds, aspect='auto', cmap='coolwarm')
    ax3.set_xlabel('Std Dev')
    ax3.set_ylabel('ADC Channel')
    ax3.set_title('Channel Standard Deviations')
    ax3.set_xticks([0])
    ax3.set_xticklabels(['Std'])
    plt.colorbar(im3, ax=ax3)
    
    # Plot 4: First few channels as line plots
    ax4 = axes[1, 1]
    n_channels_to_plot = min(10, ADC_df.shape[0])
    for i in range(n_channels_to_plot):
        ax4.plot(ADC_df.iloc[i, :], alpha=0.7, label=f'Ch {i}')
    ax4.set_xlabel('Data Points')
    ax4.set_ylabel('ADC Values')
    ax4.set_title(f'First {n_channels_to_plot} Channels')
    if n_channels_to_plot <= 5:
        ax4.legend()
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{prefix}_ADC_advanced_analysis.png', dpi=300, bbox_inches='tight')
    # plt.show()
    print(f"Saved advanced ADC analysis: {prefix}_ADC_advanced_analysis.png")


def main():
    """
    Main demonstration function - processes HDF5 files only
    """
    
    # Look for HDF5 files
    h5_files = [f for f in os.listdir('.') if f.endswith('.h5')]
    
    print(f"Found {len(h5_files)} HDF5 files")
    
    if not h5_files:
        print("No HDF5 files found. Run convert_to_h5_DT5743.py first.")
        return
    
    print("\nLoading HDF5 data...")
    
    aggregate_results = []
    for h5_file in h5_files:
        print(f"Loading HDF5 file: {h5_file}")
        
        ADC_df, timestamps_df, metadata = load_hdf5_data(h5_file)
        
        if ADC_df is not None:
            prefix = os.path.splitext(h5_file)[0]
            print(f"\nHDF5 data loaded successfully!")
            print(f"Using sampling rate from metadata: {metadata.get('sampling_rate', 'Unknown')} Hz")
            print(f"ADC voltage scaling applied: {metadata.get('adc_voltage_scaling', 'Unknown')} V/count")
            
            # Run analysis with HDF5 data and collect results
            timing_info = run_analysis(ADC_df, timestamps_df, prefix, metadata)
            if timing_info is not None:
                # Store a minimal summary per file
                summary = {
                    'file': h5_file,
                    'prefix': prefix,
                    'num_events': int(metadata.get('num_events', ADC_df.shape[0])),
                    'num_samples_per_event': int(metadata.get('num_samples_per_event', ADC_df.shape[1])),
                    'sampling_rate': float(metadata.get('sampling_rate')),
                    'baseline': float(timing_info.get('baseline', np.nan)),
                    'peak': float(timing_info.get('peak', np.nan)),
                    'amplitude': float(timing_info.get('amplitude', np.nan)),
                    'is_positive_pulse': bool(timing_info.get('is_positive_pulse', False)),
                    'peak_idx': int(timing_info.get('peak_idx', -1)),
                    'rise_samples': int(timing_info.get('rise_time')) if timing_info.get('rise_time') is not None else -1,
                    'fall_samples': int(timing_info.get('fall_time')) if timing_info.get('fall_time') is not None else -1,
                    'width_samples': int(timing_info.get('pulse_width')) if timing_info.get('pulse_width') is not None else -1,
                    'time_per_sample': float(timing_info.get('time_per_sample', np.nan))
                }
                aggregate_results.append({'summary': summary, 'timing_info': timing_info})
            
        else:
            print(f"Failed to load HDF5 data from {h5_file}")

    # Save aggregate results to HDF5 if we have any
    if aggregate_results:
        out_name = f"aggregate_results_{pd.Timestamp.now().strftime('%Y%m%d_%H%M%S')}.h5"
        save_aggregate_results_hdf5(aggregate_results, out_name)
        print(f"Saved aggregate results to {out_name}")


def run_analysis(ADC_df, timestamps_df, prefix, metadata=None):
    """
    Run the complete analysis pipeline on loaded HDF5 data.
    
    Args:
        ADC_df: ADC data DataFrame
        timestamps_df: Event timestamps DataFrame
        prefix: File prefix for saving outputs
        metadata: Metadata dictionary from HDF5 (required for timing analysis)
    """
    
    # Demonstrate operations
    # demonstrate_dataframe_operations(ADC_df)

    # Create plots
    # plot_dataframe_data(ADC_df, prefix, metadata)
    
    # Create ADC diagrams (oscilloscope-style)
    # Raw pulse diagram
    # plot_adc_diagram(ADC_df, prefix, alpha=0.1, max_pulses=500)
    
    # Normalized pulse diagrams
    # plot_adc_diagram_normalized(ADC_df, prefix, normalize=True, 
    #                           norm_method='individual', alpha=0.1, max_pulses=500)
    # plot_adc_diagram_normalized(ADC_df, prefix, normalize=True, 
    #                           norm_method='baseline', alpha=0.1, max_pulses=500)
    
    # Advanced analysis
    # plot_adc_diagram_advanced(ADC_df, prefix, alpha=0.05, normalize=False)
    plot_adc_diagram_advanced(ADC_df, prefix, alpha=0.05, max_pulses=1000, 
                             normalize=True, norm_method='individual')
    
    # Analyze pulse timing characteristics
    print(f"\nAnalyzing pulse timing for {prefix}...")
    
    # Get sampling rate from metadata - required for timing analysis
    sampling_rate = None
    if metadata is not None:
        sampling_rate = metadata.get('sampling_rate', None)
    
    if sampling_rate is None:
        print("Warning: No sampling rate available in metadata - skipping timing analysis")
        timing_info = None
    else:
        timing_info = analyze_pulse_timing(
            ADC_df, sampling_rate,
            method='individual',
            threshold_low=0.1,
            threshold_high=0.9)
    
    if timing_info is not None:
        print_pulse_timing_info(timing_info, prefix=prefix)
        plot_pulse_timing_analysis(timing_info, prefix, save_plot=True)

    # Return timing_info for aggregation (None if not available)
    return timing_info


def save_aggregate_results_hdf5(results, out_file):
    """
    Save aggregate results to an HDF5 file.

    Args:
        results: list of dicts with keys 'summary' and 'timing_info'
        out_file: path to output HDF5 file
    """
    # Create or overwrite HDF5 file
    with h5py.File(out_file, 'w') as f:
        for i, entry in enumerate(results):
            summary = entry.get('summary', {})
            timing = entry.get('timing_info', {})

            # Make a safe group name from file or prefix
            grp_name = summary.get('prefix', f'file_{i}')
            # Replace problematic characters
            grp_name = grp_name.replace('/', '_').replace('\\', '_')

            g = f.create_group(grp_name)

            # Store summary as attributes on the group
            for k, v in summary.items():
                try:
                    # convert numpy types to native
                    if isinstance(v, (np.generic,)):
                        v = np.asscalar(v)
                except Exception:
                    pass
                # Store as attribute
                try:
                    g.attrs[k] = v
                except Exception:
                    # fallback to string
                    g.attrs[k] = str(v)

            # Store timing arrays (mean_pulse) if present
            mean_pulse = timing.get('mean_pulse')
            if mean_pulse is not None:
                g.create_dataset('mean_pulse', data=np.asarray(mean_pulse), compression='gzip')

            # Store threshold levels
            for key in ('low_level', 'high_level', 'mid_level'):
                if key in timing:
                    g.attrs[key] = float(timing[key])

            # Store raw timing (samples)
            for key in ('rise_time', 'fall_time', 'pulse_width'):
                if key in timing and timing[key] is not None:
                    g.attrs[key] = int(timing[key])

        # Build a single pandas DataFrame summary and store as native HDF5 datasets
        try:
            summaries = []
            for entry in results:
                s = dict(entry.get('summary', {}))
                # Derive folder and filename
                file_path = s.get('file', '')
                abs_path = os.path.abspath(file_path) if file_path else ''
                s['folder'] = os.path.basename(os.path.dirname(abs_path)) if abs_path else ''
                s['filename'] = os.path.basename(file_path)
                summaries.append(s)

            summary_df = pd.DataFrame(summaries)

            # Create a dedicated group for the summary DataFrame
            if 'summary' in f:
                del f['summary']
            sg = f.create_group('summary')

            # Store column order as attribute (ASCII bytes)
            try:
                sg.attrs['columns'] = np.array(summary_df.columns.astype(str), dtype='S')
            except Exception:
                sg.attrs['columns'] = np.array(summary_df.columns.astype(str), dtype=object)

            # Store each column as its own dataset with appropriate dtype
            for col in summary_df.columns:
                col_series = summary_df[col]
                if pd.api.types.is_integer_dtype(col_series.dtype):
                    sg.create_dataset(col, data=col_series.fillna(-1).astype('int64').values)
                elif pd.api.types.is_float_dtype(col_series.dtype):
                    sg.create_dataset(col, data=col_series.astype('float64').values)
                else:
                    # store as variable-length unicode strings
                    dt = h5py.string_dtype(encoding='utf-8')
                    sg.create_dataset(col, data=col_series.fillna('').astype(str).values, dtype=dt)

        except Exception as e:
            print(f"Warning: failed to write summary DataFrame into HDF5: {e}")


if __name__ == "__main__":
    main()
    
    # Example of how to load HDF5 data directly:
    # ADC_df, timestamps_df, metadata = load_hdf5_data('test.h5')
    # if ADC_df is not None:
    #     print(f"Loaded {ADC_df.shape[0]} events with {ADC_df.shape[1]} samples each")
    #     print(f"Sampling rate: {metadata.get('sampling_rate', 'Unknown')} Hz")
    #     print(f"ADC voltage scaling: {metadata.get('adc_voltage_scaling', 'Unknown')} V/count")
    #     if timestamps_df is not None:
    #         print(f"Event timestamps available: {len(timestamps_df)} events")
    #         print(f"Timestamp range: {timestamps_df['timestamp'].min():.6f} to {timestamps_df['timestamp'].max():.6f}")
    #     
    #     # Run analysis
    #     run_analysis(ADC_df, timestamps_df, 'test_hdf5', metadata)
