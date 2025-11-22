"""Waveform conversion and analysis utilities for single PMT workflows.

Main features:
* CAEN DT5743 CSV to HDF5 conversion (metadata enrichment).
* Agilent DSA90804A CSV conversion utilities.
* Pulse alignment, normalization and timing analysis helpers.
* Simple CAEN HV serial command wrapper.
"""

from .converter import (
    parse_run_info,
    convert_csv_to_hdf5,
    convert_dt_to_h5,
    convert_dt_to_hdf5,  # compatibility alias
    convert_folder,
)
from .analysis import (
    load_hdf5_data,
    align_pulses_by_peak,
    normalize_pulses_to_max,
    analyze_pulse_timing,
    analyze_file,
    analyze_folder,
)
from .dsa_converter import convert_dsa_csv_to_hdf5, simple_transpose_csv
from .caen_hv import send_caen_command
from .plot_analysis import (
    plot_adc_overlay,
    plot_adc_diagram_advanced,
    plot_pulse_timing_analysis,
    plot_waveform_analysis,
)

__all__ = [
    'parse_run_info',
    'convert_csv_to_hdf5',
    'convert_dt_to_h5',
    'convert_dt_to_hdf5',
    'convert_folder',
    'load_hdf5_data', 'align_pulses_by_peak', 'normalize_pulses_to_max',
    'analyze_pulse_timing', 'analyze_file', 'analyze_folder',
    'convert_dsa_csv_to_hdf5', 'simple_transpose_csv', 'send_caen_command',
    'plot_adc_overlay', 'plot_adc_diagram_advanced',
    'plot_pulse_timing_analysis', 'plot_waveform_analysis',
]
