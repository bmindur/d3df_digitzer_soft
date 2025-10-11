import pandas as pd
import numpy as np
import sys
import os
import h5py

def simple_transpose_csv(csv_file, output_prefix=None, sampling_rate=40_000_000_000):
    """
    Simple function to transpose CSV data.
    
    Args:
        csv_file: Path to CSV file
        output_prefix: Prefix for output files (optional)
    
    Returns:
        even_array: Numpy array with even columns as rows
        odd_array: Numpy array with odd columns as rows
        reference: First column data
    """

    # Check if output file exists and prepare filename 
    if output_prefix is None:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_prefix = f"{base_name}"
    
    # HDF5 file in the same directory as the source CSV
    csv_dir = os.path.dirname(csv_file)
    hdf5_filename = os.path.join(csv_dir, f"{output_prefix}.h5")
    
    # Check if HDF5 file already exists
    if os.path.exists(hdf5_filename):
        print(f"HDF5 file already exists: {hdf5_filename}")
        print("Skipping conversion. Delete file if you want to regenerate.")
        return 

    
    print(f"Loading: {csv_file}")
    
    # Read CSV
    df = pd.read_csv(csv_file, header=None)
    print(f"Shape: {df.shape} ({df.shape[0]} rows, {df.shape[1]} columns)")
    
    # Extract data
    reference = df.iloc[:, 0].values
    
    # Even columns (2, 4, 6, ...)
    even_indices = list(range(0, df.shape[1], 2))
    odd_indices = list(range(1, df.shape[1], 2))
    
    print(f"Even columns: {len(even_indices)} ({even_indices[:5]}...)")
    print(f"Odd columns: {len(odd_indices)} ({odd_indices[:5]}...)")
    
    # Transpose: columns become rows
    if even_indices:
        even_array = df.iloc[:, even_indices].values.T
    else:
        even_array = np.array([])
        
    if odd_indices:
        odd_array = df.iloc[:, odd_indices].values.T
    else:
        odd_array = np.array([])
    
    print(f"Even array shape: {even_array.shape}")
    print(f"Odd array shape: {odd_array.shape}")
    
    # Extract first column from each row of even_array and convert to 1D
    if even_array.size > 0:
        first_column_1d = even_array[:, 0].flatten()
        print(f"First column 1D shape: {first_column_1d.shape}")
    
    # Save files
    if output_prefix is None:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_prefix = f"{base_name}"
    
    # Save as pandas DataFrames (pickle format)
    if even_array.size > 0:
        even_df = pd.DataFrame(even_array)
        even_df.index.name = 'timestamp_all'
        even_df.columns.name = 'data'

        timestamps = pd.DataFrame(first_column_1d)
        timestamps.index.name = 'timestamp'
        timestamps.columns.name = 'data'
        # even_df.to_pickle(f"{output_prefix}_TS.pkl")
        # print(f"Saved: {output_prefix}_TS.pkl")

    if odd_array.size > 0:
        odd_df = pd.DataFrame(odd_array)
        odd_df.index.name = 'ADC'
        odd_df.columns.name = 'data'
        # odd_df.to_pickle(f"{output_prefix}_ADC.pkl")
        # print(f"Saved: {output_prefix}_ADC.pkl")

    
    with h5py.File(hdf5_filename, 'w') as f:
        # Save timestamps as 1D array
        f.create_dataset('timestamps', data=first_column_1d, compression='gzip')
        f.create_dataset('timestamps_all', data=even_df, compression='gzip')
        # Save ADC data
        f.create_dataset('adc_data', data=odd_df,
                         compression='gzip')
        # Save metadata
        f.attrs['num_events'] = len(even_df)
        f.attrs['num_samples_per_event'] = even_df.shape[1]
        f.attrs['sampling_rate'] = sampling_rate
        f.attrs['adc_voltage_scaling'] = 1.0  # Placeholder
        f.attrs['event_timestamp_unit'] = 'ns'
        f.attrs['digitizer'] = 'Agilent DSOX6004A'
        f.attrs['source_file'] = csv_file
    print(f"Saved: {hdf5_filename}")


    return even_array, odd_array


def main():
    if len(sys.argv) != 2:
        print("Usage: python simple_transpose.py <csv_file>")
        print("\nAvailable CSV files:")
        csv_files = [f for f in os.listdir('.') if f.endswith('.csv')]
        for i, f in enumerate(csv_files[:10], 1):
            print(f"  {i}. {f}")
        if len(csv_files) > 10:
            print(f"  ... and {len(csv_files)-10} more")
        return
    
    csv_file = sys.argv[1]
    
    if not os.path.exists(csv_file):
        print(f"Error: File '{csv_file}' not found")
        return
    
    try:
        even_array, odd_array = simple_transpose_csv(csv_file)
        
        print(f"\nSUMMARY:")
        print(f"Input: {csv_file}")
        print(f"Even array: {even_array.shape} (each row = one even column)")
        print(f"Odd array: {odd_array.shape} (each row = one odd column)")
        print("Files saved as pandas DataFrame pickles (.pkl files)")
        
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    main()
