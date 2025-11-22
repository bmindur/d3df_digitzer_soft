"""Plotting utilities for waveform analysis results.

This module provides visualization functions for analyzing DT5743 HDF5
waveform data, leveraging the analysis functions from d3df_single_pmt.analysis.
"""

import os
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

from .analysis import (
    load_hdf5_data,
    align_pulses_by_peak,
    normalize_pulses_to_max,
    analyze_pulse_timing,
)


__all__ = [
    'plot_adc_overlay',
    'plot_adc_diagram_advanced',
    'plot_pulse_timing_analysis',
    'plot_waveform_analysis',
]


def plot_adc_overlay(
    ADC_df,
    prefix,
    alpha=0.1,
    max_pulses=None,
    folder_path='.',
    sampling_rate=None,
):
    """Plot all ADC pulses overlaid (oscilloscope-style diagram).

    Args:
        ADC_df: DataFrame where each row is an ADC pulse
        prefix: prefix for saving the plot
        alpha: transparency for individual pulses (0.1 = very transparent)
        max_pulses: maximum number of pulses to plot (None = all)
        folder_path: folder to save the plot
        sampling_rate: sampling rate in Hz (if None, uses sample points)
    """
    if ADC_df is None or ADC_df.empty:
        print("No ADC DataFrame available for overlay")
        return

    fig, ax = plt.subplots(1, 1, figsize=(12, 8))

    n_pulses = ADC_df.shape[0]
    if max_pulses is not None:
        n_pulses = min(n_pulses, max_pulses)

    n_samples = ADC_df.shape[1]
    
    # Calculate time axis
    if sampling_rate is not None:
        time_per_sample = 1.0 / sampling_rate
        x_axis = np.arange(n_samples) * time_per_sample * 1e9  # Convert to ns
        x_label = 'Time (ns)'
    else:
        x_axis = np.arange(n_samples)
        x_label = 'Sample Points'

    # Plot all pulses overlaid
    for i in range(n_pulses):
        ax.plot(x_axis, ADC_df.iloc[i, :], 'b-', alpha=alpha, linewidth=0.5)

    # Calculate and plot average pulse
    avg_pulse = ADC_df.iloc[:n_pulses, :].mean(axis=0)
    ax.plot(x_axis, avg_pulse, 'r-', linewidth=2,
            label=f'Average ({n_pulses} pulses)')

    # Calculate and plot standard deviation envelope
    std_pulse = ADC_df.iloc[:n_pulses, :].std(axis=0)
    ax.fill_between(
        x_axis,
        avg_pulse - std_pulse,
        avg_pulse + std_pulse,
        alpha=0.2,
        color='red',
        label='±1σ envelope',
    )

    ax.set_xlabel(x_label)
    ax.set_ylabel('ADC Values')
    ax.set_title(f'ADC Overlay - {n_pulses} Pulses')
    ax.legend()
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    output_path = os.path.join(folder_path, f'{prefix}_ADC_overlay.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved ADC overlay: {output_path}")


def plot_adc_diagram_advanced(
    ADC_df,
    prefix,
    alpha=0.05,
    max_pulses=10000,
    normalize=True,
    norm_method='individual',
    folder_path='.',
    align_data=True,
    sampling_rate=None,
):
    """Create advanced diagram with multiple views and statistics.

    Args:
        ADC_df: DataFrame where each row is an ADC pulse
        prefix: prefix for saving the plot
        alpha: transparency for individual pulses
        max_pulses: maximum number of pulses to plot
        normalize: whether to normalize pulses
        norm_method: 'individual', 'global', or 'baseline'
        folder_path: folder to save the plot
        align_data: whether to align pulses by peak position
        sampling_rate: sampling rate in Hz (if None, uses sample points)
    """
    if ADC_df is None:
        print("No ADC DataFrame available for advanced diagram")
        return

    # Align pulses by peak position first
    if align_data:
        print(f"Aligning {ADC_df.shape[0]} pulses by peak position...")
        ADC_df, peak_positions = align_pulses_by_peak(ADC_df)
        if ADC_df is None:
            print("Failed to align pulses")
            return
        print(
            f"  Peak positions: min={peak_positions.min()}, "
            f"max={peak_positions.max()}, "
            f"median={int(np.median(peak_positions))}"
        )

    if normalize:
        ADC_df = normalize_pulses_to_max(ADC_df, method=norm_method)

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))
    fig.suptitle(f'ADC Diagram Analysis: {prefix}', fontsize=16)

    n_pulses = (
        min(ADC_df.shape[0], max_pulses) if max_pulses else ADC_df.shape[0]
    )
    n_samples = ADC_df.shape[1]
    
    # Calculate time axis
    if sampling_rate is not None:
        time_per_sample = 1.0 / sampling_rate
        x_axis = np.arange(n_samples) * time_per_sample * 1e9  # Convert to ns
        x_label = 'Time (ns)'
    else:
        x_axis = np.arange(n_samples)
        x_label = 'Sample Points'

    # Plot 1: All pulses overlaid (eye diagram)
    ax1 = axes[0, 0]
    for i in range(n_pulses):
        ax1.plot(
            x_axis, ADC_df.iloc[i, :], 'b-', alpha=alpha, linewidth=0.3
        )

    # Add average
    avg_pulse = ADC_df.iloc[:n_pulses, :].mean(axis=0)
    ax1.plot(x_axis, avg_pulse, 'r-', linewidth=2, label='Average')
    ax1.set_xlabel(x_label)
    ax1.set_ylabel('ADC Values')
    ax1.set_title(f'ADC Diagram ({n_pulses} pulses)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: Average pulse with error bars
    ax2 = axes[0, 1]
    std_pulse = ADC_df.iloc[:n_pulses, :].std(axis=0)
    ax2.errorbar(
        x_axis[::10],
        avg_pulse[::10],
        yerr=std_pulse[::10],
        fmt='ro-',
        capsize=3,
        alpha=0.7,
        markersize=3,
    )
    ax2.plot(x_axis, avg_pulse, 'r-', linewidth=1)
    ax2.set_xlabel(x_label)
    ax2.set_ylabel('ADC Values')
    ax2.set_title('Average Pulse ± Standard Deviation')
    ax2.grid(True, alpha=0.3)

    # Plot 3: Pulse statistics over time
    ax3 = axes[1, 0]
    ax3.plot(x_axis, avg_pulse, 'g-', linewidth=2, label='Mean')
    ax3.fill_between(
        x_axis,
        avg_pulse - std_pulse,
        avg_pulse + std_pulse,
        alpha=0.3,
        color='green',
        label='±1σ',
    )
    ax3.fill_between(
        x_axis,
        avg_pulse - 2 * std_pulse,
        avg_pulse + 2 * std_pulse,
        alpha=0.2,
        color='yellow',
        label='±2σ',
    )
    ax3.set_xlabel(x_label)
    ax3.set_ylabel('ADC Values')
    ax3.set_title('Statistical Envelope')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # Plot 4: First few individual pulses for comparison
    ax4 = axes[1, 1]
    n_individual = min(10, n_pulses)
    colors = plt.cm.tab10(range(n_individual))
    for i in range(n_individual):
        ax4.plot(
            x_axis,
            ADC_df.iloc[i, :],
            color=colors[i],
            linewidth=1,
            alpha=0.8,
            label=f'Pulse {i}',
        )
    ax4.plot(x_axis, avg_pulse, 'k--', linewidth=2, label='Average')
    ax4.set_xlabel(x_label)
    ax4.set_ylabel('ADC Values')
    ax4.set_title(f'Individual Pulses (first {n_individual})')
    if n_individual <= 5:
        ax4.legend()
    ax4.grid(True, alpha=0.3)

    plt.tight_layout()
    norm_suffix = 'normalized' if normalize else 'raw'
    output_path = os.path.join(
        folder_path, f'{prefix}_ADC_diagram_{norm_suffix}.png'
    )
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved advanced ADC diagram: {output_path}")


def plot_pulse_timing_analysis(
    timing_info,
    prefix,
    save_plot=True,
    folder_path='.',
):
    """Plot timing analysis with marked measurement points.

    Args:
        timing_info: dictionary from analyze_pulse_timing()
        prefix: prefix for saving the plot
        save_plot: whether to save the plot
        folder_path: folder to save the plot
    """
    if timing_info is None:
        print("No timing information available for plotting")
        return

    fig, ax = plt.subplots(1, 1, figsize=(14, 8))

    mean_pulse = timing_info['mean_pulse']
    x_axis = np.arange(len(mean_pulse))

    # Use time axis if available
    has_time_data = timing_info.get('has_time_data', False)
    time_axis_data = timing_info.get('time_axis')
    if has_time_data and time_axis_data is not None:
        x_plot = time_axis_data * 1e9  # Convert seconds to nanoseconds
        x_label = 'Time (ns)'
        title_suffix = ''
    else:
        x_plot = x_axis
        x_label = 'Sample Points'
        title_suffix = ' (Sample Index)'

    # Plot the mean pulse
    ax.plot(x_plot, mean_pulse, 'b-', linewidth=2, label='Mean Pulse')

    # Add threshold lines
    thresh_low = timing_info['threshold_low'] * 100
    thresh_high = timing_info['threshold_high'] * 100
    ax.axhline(
        y=timing_info['low_level'],
        color='orange',
        linestyle='--',
        label=f"Low threshold ({thresh_low:.0f}%)",
    )
    ax.axhline(
        y=timing_info['high_level'],
        color='red',
        linestyle='--',
        label=f"High threshold ({thresh_high:.0f}%)",
    )

    if 'mid_level' in timing_info:
        ax.axhline(
            y=timing_info['mid_level'],
            color='green',
            linestyle='--',
            label="Mid level (50%)",
        )

    def get_x_coordinate(index):
        """Convert sample index to x-coordinate (time or sample)"""
        if has_time_data and time_axis_data is not None:
            return time_axis_data[index] * 1e9  # Convert to nanoseconds
        return index

    def format_annotation(timing_samples, kind=None):
        """Format annotation text with time units in nanoseconds"""
        # Prefer *_ns values from timing_info if available
        if kind and timing_info.get(f'{kind}_ns') is not None:
            return f"{timing_info[f'{kind}_ns']:.1f} ns"
        # Otherwise, try to convert from seconds
        if has_time_data and timing_info.get('time_per_sample') is not None:
            time_sec = timing_samples * timing_info['time_per_sample']
            return f"{time_sec*1e9:.1f} ns"
        return f"{timing_samples} samples"

    # Mark rise time
    if timing_info.get('rise_time') is not None:
        rise_start = timing_info['rise_start_idx']
        rise_end = timing_info['rise_end_idx']
        rise_start_x = get_x_coordinate(rise_start)
        rise_end_x = get_x_coordinate(rise_end)
        ax.plot(
            [rise_start_x, rise_end_x],
            [timing_info['low_level'], timing_info['high_level']],
            'ro-',
            markersize=8,
            label=f"Rise: {format_annotation(timing_info['rise_time'], 'rise_time')}",
        )
        ax.annotate(
            f"Rise: {format_annotation(timing_info['rise_time'], 'rise_time')}",
            xy=((rise_start_x + rise_end_x) / 2, timing_info['high_level']),
            xytext=(10, 10),
            textcoords='offset points',
            bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow', alpha=0.7),
        )

    # Mark fall time
    if timing_info.get('fall_time') is not None:
        fall_start = timing_info['fall_start_idx']
        fall_end = timing_info['fall_end_idx']
        fall_start_x = get_x_coordinate(fall_start)
        fall_end_x = get_x_coordinate(fall_end)
        ax.plot(
            [fall_start_x, fall_end_x],
            [timing_info['high_level'], timing_info['low_level']],
            'mo-',
            markersize=8,
            label=f"Fall: {format_annotation(timing_info['fall_time'], 'fall_time')}",
        )
        ax.annotate(
            f"Fall: {format_annotation(timing_info['fall_time'], 'fall_time')}",
            xy=((fall_start_x + fall_end_x) / 2, timing_info['low_level']),
            xytext=(10, -20),
            textcoords='offset points',
            bbox=dict(boxstyle='round,pad=0.3', facecolor='cyan', alpha=0.7),
        )

    # Mark pulse width
    if timing_info.get('pulse_width') is not None:
        width_start = timing_info['width_start_idx']
        width_end = timing_info['width_end_idx']
        width_start_x = get_x_coordinate(width_start)
        width_end_x = get_x_coordinate(width_end)
        ax.plot(
            [width_start_x, width_end_x],
            [timing_info['mid_level'], timing_info['mid_level']],
            'go-',
            markersize=6,
            linewidth=3,
            label=f"Width: {format_annotation(timing_info['pulse_width'], 'pulse_width')}",
        )
        ax.annotate(
            f"Width: {format_annotation(timing_info['pulse_width'], 'pulse_width')}",
            xy=((width_start_x + width_end_x) / 2, timing_info['mid_level']),
            xytext=(0, 15),
            textcoords='offset points',
            bbox=dict(
                boxstyle='round,pad=0.3', facecolor='lightgreen', alpha=0.7
            ),
        )

    # Mark peak
    peak_idx = timing_info['peak_idx']
    peak_x = get_x_coordinate(peak_idx)
    ax.plot(peak_x, timing_info['peak'], 'r*', markersize=15, label='Peak')

    ax.set_xlabel(x_label)
    ax.set_ylabel('Normalized ADC Values')
    ax.set_title(f'Pulse Timing Analysis - {prefix}{title_suffix}')
    ax.legend()
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    if save_plot:
        output_path = os.path.join(
            folder_path, f'{prefix}_pulse_timing_analysis.png'
        )
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"Saved timing analysis plot: {output_path}")

    # Create a separate zoomed-in plot around the pulse peak
    peak_idx = timing_info['peak_idx']
    window_samples = 20
    if has_time_data and time_axis_data is not None:
        time_per_sample = (time_axis_data[1] - time_axis_data[0]) * 1e9
        window_ns = 6  # ns
        window = int(window_ns / time_per_sample)
    else:
        window = window_samples
    start = max(0, peak_idx - window)
    end = min(len(mean_pulse), peak_idx + window)
    fig_zoom, ax_zoom = plt.subplots(1, 1, figsize=(8, 5))
    ax_zoom.plot(x_plot[start:end], mean_pulse[start:end], 'b-', linewidth=2)
    ax_zoom.set_xlabel(x_label)
    ax_zoom.set_ylabel('Normalized ADC Values')
    ax_zoom.set_title(f'Pulse Timing Zoom - {prefix}')
    ax_zoom.grid(True, alpha=0.3)

    # Add scatter points and annotations for rise, fall, width
    # Rise
    if timing_info.get('rise_time') is not None:
        rise_start = timing_info['rise_start_idx']
        rise_end = timing_info['rise_end_idx']
        rise_start_x = get_x_coordinate(rise_start)
        rise_end_x = get_x_coordinate(rise_end)
        ax_zoom.scatter([rise_start_x, rise_end_x], [timing_info['low_level'], timing_info['high_level']], color='red', zorder=5)
        ax_zoom.plot([rise_start_x, rise_end_x], [timing_info['low_level'], timing_info['high_level']], 'ro-', alpha=0.5)
        ax_zoom.annotate(f"Rise: {format_annotation(timing_info['rise_time'], 'rise_time')}",
                        xy=((rise_start_x + rise_end_x) / 2, timing_info['high_level']),
                        xytext=(10, 10), textcoords='offset points',
                        bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow', alpha=0.7))
    # Fall
    if timing_info.get('fall_time') is not None:
        fall_start = timing_info['fall_start_idx']
        fall_end = timing_info['fall_end_idx']
        fall_start_x = get_x_coordinate(fall_start)
        fall_end_x = get_x_coordinate(fall_end)
        ax_zoom.scatter([fall_start_x, fall_end_x], [timing_info['high_level'], timing_info['low_level']], color='magenta', zorder=5)
        ax_zoom.plot([fall_start_x, fall_end_x], [timing_info['high_level'], timing_info['low_level']], 'mo-', alpha=0.5)
        ax_zoom.annotate(f"Fall: {format_annotation(timing_info['fall_time'], 'fall_time')}",
                        xy=((fall_start_x + fall_end_x) / 2, timing_info['low_level']),
                        xytext=(10, -20), textcoords='offset points',
                        bbox=dict(boxstyle='round,pad=0.3', facecolor='cyan', alpha=0.7))
    # Width
    if timing_info.get('pulse_width') is not None:
        width_start = timing_info['width_start_idx']
        width_end = timing_info['width_end_idx']
        width_start_x = get_x_coordinate(width_start)
        width_end_x = get_x_coordinate(width_end)
        ax_zoom.scatter([width_start_x, width_end_x], [timing_info['mid_level'], timing_info['mid_level']], color='green', zorder=5)
        ax_zoom.plot([width_start_x, width_end_x], [timing_info['mid_level'], timing_info['mid_level']], 'go-', alpha=0.5)
        ax_zoom.annotate(f"Width: {format_annotation(timing_info['pulse_width'], 'pulse_width')}",
                        xy=((width_start_x + width_end_x) / 2, timing_info['mid_level']),
                        xytext=(0, 15), textcoords='offset points',
                        bbox=dict(boxstyle='round,pad=0.3', facecolor='lightgreen', alpha=0.7))
    plt.tight_layout()
    output_zoom_path = os.path.join(folder_path, f'{prefix}_pulse_timing_zoom.png')
    plt.savefig(output_zoom_path, dpi=300, bbox_inches='tight')
    plt.close(fig_zoom)
    print(f"Saved timing zoom plot: {output_zoom_path}")


def plot_waveform_analysis(
    hdf5_file,
    output_folder='.',
    alpha=0.05,
    max_pulses=1000,
    normalize=True,
    norm_method='individual',
    align_data=True,
):
    """Complete waveform analysis with all plots for a single HDF5 file.

    Args:
        hdf5_file: Path to HDF5 file
        output_folder: Folder to save plots
        alpha: Transparency for overlay plots
        max_pulses: Maximum pulses to plot
        normalize: Whether to normalize pulses
        norm_method: Normalization method
        align_data: Whether to align pulses by peak
    """
    print(f"\nAnalyzing: {os.path.basename(hdf5_file)}")

    # Load data
    ADC_df, timestamps_df, metadata = load_hdf5_data(hdf5_file)

    if ADC_df is None:
        print(f"Failed to load {hdf5_file}")
        return None

    prefix = os.path.splitext(os.path.basename(hdf5_file))[0]

    print(
        f"Loaded {ADC_df.shape[0]} events with "
        f"{ADC_df.shape[1]} samples each"
    )
    
    # Extract sampling rate from metadata
    sampling_rate = metadata.get('sampling_rate') if metadata else None
    if metadata:
        print(f"Sampling rate: {metadata.get('sampling_rate', 'Unknown')} Hz")

    # Create advanced diagram
    plot_adc_diagram_advanced(
        ADC_df,
        prefix,
        alpha=alpha,
        max_pulses=max_pulses,
        normalize=normalize,
        norm_method=norm_method,
        folder_path=output_folder,
        align_data=align_data,
        sampling_rate=sampling_rate,
    )

    # Analyze pulse timing
    if sampling_rate:
        print(f"Analyzing pulse timing for {prefix}...")
        timing_info = analyze_pulse_timing(
            ADC_df,
            sampling_rate,
            method='individual',
            threshold_low=0.1,
            threshold_high=0.9,
            align=True,
        )

        if timing_info:
            # Print timing summary
            pulse_type = (
                "Positive" if timing_info['is_positive_pulse'] else "Negative"
            )
            print(f"  Pulse type: {pulse_type}")
            print(f"  Amplitude: {timing_info['amplitude']:.4f}")
            if timing_info.get('rise_time_ns'):
                print(f"  Rise time: {timing_info['rise_time_ns']:.1f} ns")
            if timing_info.get('fall_time_ns'):
                print(f"  Fall time: {timing_info['fall_time_ns']:.1f} ns")
            if timing_info.get('pulse_width_ns'):
                print(
                    f"  Pulse width: {timing_info['pulse_width_ns']:.1f} ns"
                )

            # Plot timing analysis
            plot_pulse_timing_analysis(
                timing_info, prefix, save_plot=True, folder_path=output_folder
            )

            return timing_info
    else:
        print("No sampling rate available - skipping timing analysis")

    return None


def main():
    """CLI entry point for plotting waveform analysis."""
    parser = argparse.ArgumentParser(
        description='Plot waveform analysis from HDF5 files.'
    )
    parser.add_argument(
        'folder',
        nargs='?',
        default='.',
        help='Folder containing .h5 files (default: current directory)',
    )
    parser.add_argument(
        '--alpha',
        type=float,
        default=0.05,
        help='Transparency for overlay plots (default: 0.05)',
    )
    parser.add_argument(
        '--max-pulses',
        type=int,
        default=1000,
        help='Maximum pulses to plot (default: 1000)',
    )
    parser.add_argument(
        '--no-normalize',
        action='store_true',
        help='Skip pulse normalization',
    )
    parser.add_argument(
        '--norm-method',
        choices=['individual', 'global', 'baseline'],
        default='individual',
        help='Normalization method (default: individual)',
    )
    parser.add_argument(
        '--no-align',
        action='store_true',
        help='Skip pulse alignment',
    )
    args = parser.parse_args()

    if not os.path.isdir(args.folder):
        print(f"Error: Folder '{args.folder}' does not exist.")
        return

    print(f"Processing files in: {os.path.abspath(args.folder)}")

    # Find HDF5 files
    h5_files = [
        f for f in os.listdir(args.folder) if f.endswith('.h5')
    ]

    if not h5_files:
        print("No HDF5 files found.")
        return

    print(f"Found {len(h5_files)} HDF5 file(s)")

    # Process each file
    for h5_file in h5_files:
        full_path = os.path.join(args.folder, h5_file)
        plot_waveform_analysis(
            full_path,
            output_folder=args.folder,
            alpha=args.alpha,
            max_pulses=args.max_pulses,
            normalize=not args.no_normalize,
            norm_method=args.norm_method,
            align_data=not args.no_align,
        )

    print(f"\nPlotting complete. Output saved to {args.folder}")


if __name__ == '__main__':
    main()
