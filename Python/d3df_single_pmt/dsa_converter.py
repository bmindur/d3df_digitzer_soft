"""Agilent DSA90804A CSV to HDF5 conversion utilities."""
from __future__ import annotations
import os
import numpy as np
import pandas as pd
import h5py

__all__ = ['convert_dsa_csv_to_hdf5', 'simple_transpose_csv']

def convert_dsa_csv_to_hdf5(
    csv_file: str,
    output_prefix: str | None = None,
    sampling_rate: float = 40e9,
    overwrite: bool = False,
):
    if not os.path.isfile(csv_file):
        raise FileNotFoundError(csv_file)
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    if output_prefix is None:
        output_prefix = base_name
    csv_dir = os.path.dirname(csv_file)
    hdf5_path = os.path.join(csv_dir, f"{output_prefix}.h5")
    if os.path.exists(hdf5_path) and not overwrite:
        print(f"[convert-dsa] Exists, skipping: {hdf5_path}")
        return None
    df = pd.read_csv(csv_file, header=None)
    even_indices = list(range(0, df.shape[1], 2))
    odd_indices = list(range(1, df.shape[1], 2))
    even_array = (
        df.iloc[:, even_indices].values.T if even_indices else np.empty((0, 0))
    )
    odd_array = (
        df.iloc[:, odd_indices].values.T if odd_indices else np.empty((0, 0))
    )
    if even_array.size > 0:
        timestamps = even_array[:, 0].astype(float)
    else:
        timestamps = np.array([], dtype=float)
    with h5py.File(hdf5_path, 'w') as f:
        f.create_dataset('timestamps', data=timestamps, compression='gzip')
        if even_array.size > 0:
            f.create_dataset(
                'timestamps_all', data=even_array, compression='gzip'
            )
        if odd_array.size > 0:
            f.create_dataset('adc_data', data=odd_array, compression='gzip')
        f.attrs['num_events'] = odd_array.shape[0] if odd_array.size > 0 else 0
        f.attrs['num_samples_per_event'] = (
            odd_array.shape[1] if odd_array.size > 0 else 0
        )
        f.attrs['sampling_rate'] = sampling_rate
        f.attrs['adc_voltage_scaling'] = 1.0
        f.attrs['event_timestamp_unit'] = 'ns'
        f.attrs['digitizer'] = 'Agilent DSA90804A'
        f.attrs['source_file'] = csv_file
    print(f"[convert-dsa] Saved: {hdf5_path}")
    return hdf5_path

def simple_transpose_csv(
    csv_file: str,
    output_prefix: str | None = None,
    sampling_rate: float = 40e9,
):
    base_name = os.path.splitext(os.path.basename(csv_file))[0]
    if output_prefix is None:
        output_prefix = base_name
    csv_dir = os.path.dirname(csv_file)
    hdf5_filename = os.path.join(csv_dir, f"{output_prefix}.h5")
    if os.path.exists(hdf5_filename):
        print(f"[simple-transpose] HDF5 exists, skipping: {hdf5_filename}")
        return None, None, None
    df = pd.read_csv(csv_file, header=None)
    even_indices = list(range(0, df.shape[1], 2))
    odd_indices = list(range(1, df.shape[1], 2))
    even_array = (
        df.iloc[:, even_indices].values.T if even_indices else np.empty((0, 0))
    )
    odd_array = (
        df.iloc[:, odd_indices].values.T if odd_indices else np.empty((0, 0))
    )
    convert_dsa_csv_to_hdf5(
        csv_file,
        output_prefix=output_prefix,
        sampling_rate=sampling_rate,
        overwrite=True,
    )
    return even_array, odd_array, hdf5_filename

def main():  # CLI entry point
    import argparse

    parser = argparse.ArgumentParser(
        description='Convert Agilent DSA90804A CSV waveform file to HDF5.'
    )
    parser.add_argument('csv_file', help='Path to CSV file')
    parser.add_argument(
        '--sampling-rate', type=float, default=40e9,
        help='Sampling rate in Hz (default: 40e9)'
    )
    parser.add_argument(
        '--overwrite', action='store_true',
        help='Overwrite existing HDF5 if present'
    )
    args = parser.parse_args()
    try:
        path = convert_dsa_csv_to_hdf5(
            args.csv_file,
            sampling_rate=args.sampling_rate,
            overwrite=args.overwrite,
        )
        if path:
            print(f"Conversion completed: {path}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    main()
