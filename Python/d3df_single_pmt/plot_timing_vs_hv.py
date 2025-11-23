"""Plot pulse timing parameters vs PMT HV grouped by source and scintillator.

This module reads analysis_results_*.h5 files produced by analysis.py and creates
plots showing rise time, fall time, and pulse width (in ns) as functions of PMT HV,
grouped by source and scintillator combinations.
"""

import os
import argparse
import numpy as np
import pandas as pd
import h5py
import matplotlib.pyplot as plt
from pathlib import Path


def _decode_value(val):
    """Decode bytes to string if needed."""
    if isinstance(val, bytes):
        return val.decode('utf-8')
    return val


def load_analysis_results(hdf5_file):
    """Load analysis results from HDF5 file.
    
    Args:
        hdf5_file: Path to analysis_results_*.h5 file
        
    Returns:
        pandas.DataFrame with all analysis data
    """
    try:
        with h5py.File(hdf5_file, 'r') as f:
            # Get column names from attributes
            columns = f.attrs.get('columns', [])
            if isinstance(columns, bytes):
                columns = [columns.decode('utf-8')]
            elif isinstance(columns, np.ndarray):
                columns = [col.decode('utf-8') if isinstance(col, bytes) else col 
                          for col in columns]
            
            # Load all datasets
            data = {}
            for col in columns:
                if col in f:
                    data[col] = f[col][:]
            
            df = pd.DataFrame(data)
            print(f"Loaded {len(df)} analysis results from {os.path.basename(hdf5_file)}")
            return df
    except Exception as e:
        print(f"Error loading {hdf5_file}: {e}")
        return None


def plot_timing_vs_hv(df, output_folder='.'):
    """Create plots of timing parameters vs HV grouped by source and scintillator.
    
    Args:
        df: DataFrame with analysis results
        output_folder: Folder to save plots
    """
    # Check required columns
    required_cols = ['pmt_hv', 'rise_time_ns', 'fall_time_ns', 'pulse_width_ns']
    missing = [col for col in required_cols if col not in df.columns]
    if missing:
        print(f"Error: Missing required columns: {missing}")
        return
    
    # Group columns
    group_cols = []
    if 'scintilator' in df.columns:
        group_cols.append('scintilator')
    if 'source' in df.columns:
        group_cols.append('source')
    
    if not group_cols:
        print("Warning: No scintillator or source columns found. Plotting all data together.")
        groups = [('All Data', df)]
    else:
        # Create groups
        grouped = df.groupby(group_cols)
        groups = [(name, group) for name, group in grouped]
    
    # Create figure with 3 subplots (rise, fall, width)
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    fig.suptitle('Pulse Timing vs PMT HV', fontsize=16, fontweight='bold')
    
    # Colors and markers for different groups
    colors = plt.cm.tab10(np.linspace(0, 1, len(groups)))
    markers = ['o', 's', '^', 'v', 'D', 'p', '*', 'h', '+', 'x']
    
    for idx, (group_name, group_df) in enumerate(groups):
        # Build label from group name, decode bytes if needed
        if isinstance(group_name, tuple):
            label = ' / '.join(str(_decode_value(g)) for g in group_name)
        else:
            label = str(_decode_value(group_name))
        
        color = colors[idx % len(colors)]
        marker = markers[idx % len(markers)]
        
        # Filter out NaN values and sort by HV
        valid_data = group_df.dropna(subset=['pmt_hv', 'rise_time_ns', 
                                              'fall_time_ns', 'pulse_width_ns'])
        if len(valid_data) == 0:
            print(f"Warning: No valid data for group {label}")
            continue
        
        valid_data = valid_data.sort_values('pmt_hv')
        hv = valid_data['pmt_hv'].values
        
        # Plot rise time
        axes[0].plot(hv, valid_data['rise_time_ns'], 
                    marker=marker, color=color, label=label, 
                    linestyle='-', linewidth=1.5, markersize=8, alpha=0.7)
        
        # Plot fall time
        axes[1].plot(hv, valid_data['fall_time_ns'], 
                    marker=marker, color=color, label=label,
                    linestyle='-', linewidth=1.5, markersize=8, alpha=0.7)
        
        # Plot pulse width
        axes[2].plot(hv, valid_data['pulse_width_ns'], 
                    marker=marker, color=color, label=label,
                    linestyle='-', linewidth=1.5, markersize=8, alpha=0.7)
    
    # Customize subplots
    axes[0].set_xlabel('PMT HV (V)', fontsize=12, fontweight='bold')
    axes[0].set_ylabel('Rise Time (ns)', fontsize=12, fontweight='bold')
    axes[0].set_title('Rise Time vs HV', fontsize=14)
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(fontsize=10, loc='best')
    
    axes[1].set_xlabel('PMT HV (V)', fontsize=12, fontweight='bold')
    axes[1].set_ylabel('Fall Time (ns)', fontsize=12, fontweight='bold')
    axes[1].set_title('Fall Time vs HV', fontsize=14)
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(fontsize=10, loc='best')
    
    axes[2].set_xlabel('PMT HV (V)', fontsize=12, fontweight='bold')
    axes[2].set_ylabel('Pulse Width (ns)', fontsize=12, fontweight='bold')
    axes[2].set_title('Pulse Width vs HV', fontsize=14)
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(fontsize=10, loc='best')
    
    plt.tight_layout()
    
    # Save plot
    output_path = os.path.join(output_folder, 'timing_vs_hv.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved timing vs HV plot: {output_path}")
    
    # Create individual plots for each timing parameter
    _plot_individual_timing_params(groups, output_folder)


def _plot_individual_timing_params(groups, output_folder):
    """Create separate plots for each timing parameter.
    
    Args:
        groups: List of (name, dataframe) tuples
        output_folder: Folder to save plots
    """
    colors = plt.cm.tab10(np.linspace(0, 1, len(groups)))
    markers = ['o', 's', '^', 'v', 'D', 'p', '*', 'h', '+', 'x']
    
    timing_params = [
        ('rise_time_ns', 'Rise Time (ns)', 'rise_time_vs_hv.png'),
        ('fall_time_ns', 'Fall Time (ns)', 'fall_time_vs_hv.png'),
        ('pulse_width_ns', 'Pulse Width (ns)', 'pulse_width_vs_hv.png'),
    ]
    
    for param_col, param_label, filename in timing_params:
        fig, ax = plt.subplots(figsize=(10, 7))
        
        for idx, (group_name, group_df) in enumerate(groups):
            # Build label, decode bytes if needed
            if isinstance(group_name, tuple):
                label = ' / '.join(str(_decode_value(g)) for g in group_name)
            else:
                label = str(_decode_value(group_name))
            
            color = colors[idx % len(colors)]
            marker = markers[idx % len(markers)]
            
            # Filter and sort
            valid_data = group_df.dropna(subset=['pmt_hv', param_col])
            if len(valid_data) == 0:
                continue
            
            valid_data = valid_data.sort_values('pmt_hv')
            hv = valid_data['pmt_hv'].values
            timing = valid_data[param_col].values
            
            # Plot with error bars if we have multiple measurements at same HV
            ax.plot(hv, timing, marker=marker, color=color, label=label,
                   linestyle='-', linewidth=2, markersize=10, alpha=0.8)
        
        ax.set_xlabel('PMT HV (V)', fontsize=14, fontweight='bold')
        ax.set_ylabel(param_label, fontsize=14, fontweight='bold')
        ax.set_title(f'{param_label} vs PMT HV', fontsize=16, fontweight='bold')
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.legend(fontsize=11, loc='best', framealpha=0.9)
        
        plt.tight_layout()
        output_path = os.path.join(output_folder, filename)
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"Saved {param_label} plot: {output_path}")


def main():
    """CLI entry point for plotting timing vs HV."""
    parser = argparse.ArgumentParser(
        description='Plot pulse timing parameters vs PMT HV from analysis results.'
    )
    parser.add_argument(
        'file',
        nargs='?',
        help='Path to analysis_results_*.h5 file (if not provided, searches current directory)',
    )
    parser.add_argument(
        '--output',
        '-o',
        default=None,
        help='Output folder for plots (default: same folder as HDF5 file)',
    )
    args = parser.parse_args()
    
    # Find HDF5 file
    if args.file:
        h5_file = args.file
        if not os.path.exists(h5_file):
            print(f"Error: File '{h5_file}' not found.")
            return
    else:
        # Search for analysis_results_*.h5 in current directory
        h5_files = list(Path('.').glob('analysis_results_*.h5'))
        if not h5_files:
            print("Error: No analysis_results_*.h5 files found in current directory.")
            print("Please run 'analyze-dt-hdf <folder>' first to generate analysis results.")
            return
        
        # Use the most recent file
        h5_file = max(h5_files, key=lambda p: p.stat().st_mtime)
        print(f"Using most recent analysis file: {h5_file}")
    
    # Determine output folder: use HDF5 file's directory if not specified
    if args.output is None:
        output_folder = os.path.dirname(os.path.abspath(h5_file))
        if not output_folder:
            output_folder = '.'
    else:
        output_folder = args.output
    
    # Load data
    df = load_analysis_results(h5_file)
    if df is None or df.empty:
        print("Error: Failed to load data or data is empty.")
        return
    
    # Print summary
    print(f"\nData summary:")
    print(f"  Total measurements: {len(df)}")
    if 'pmt_hv' in df.columns:
        hv_values = df['pmt_hv'].dropna().unique()
        print(f"  HV values: {sorted(hv_values)}")
    if 'scintilator' in df.columns:
        scints = df['scintilator'].dropna().unique()
        print(f"  Scintillators: {list(scints)}")
    if 'source' in df.columns:
        sources = df['source'].dropna().unique()
        print(f"  Sources: {list(sources)}")
    
    # Create output folder if needed
    os.makedirs(output_folder, exist_ok=True)
    
    # Generate plots
    plot_timing_vs_hv(df, output_folder)
    
    print(f"\nPlotting complete. Plots saved to {output_folder}")


if __name__ == '__main__':
    main()
