import numpy as np
import matplotlib.pyplot as plt
import sys
import os
import h5py

def convert_csv_to_hdf5(csv_file, output_prefix=None, sampling_rate=3_200_000_000):
    """
    Load signal data from file where each line contains:
    - Column 1: Event ID/Timestamp (all signals are considered "on")
    - Column 2: Time
    - Column 3: Correction to TS
    - Column 4: Number of samples
    - Remaining columns: Sample values
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

    # Load all data using numpy
    try:
        # Load the entire file as a 2D array, treating missing values as NaN
        data = np.loadtxt(csv_file, dtype=float, ndmin=2)
    except ValueError:
        # If loadtxt fails due to inconsistent column numbers, use genfromtxt
        data = np.genfromtxt(csv_file, dtype=float, filling_values=np.nan)
    
    # Check if data was loaded successfully
    if data.size == 0 or data.shape[1] < 4:
        print(f"Warning: No valid data loaded from {csv_file}")
        return 
    
    # Extract the metadata columns
    event_TS = data[:, 0] * 5  # First column is event coarse timestamp in ns, * 5 from CAEN TDC
    event_fine_TS = data[:, 1] # Second column is fine timestamp correction in ns
    event_energy = data[:, 2] # Third column is energy
    # num_samples_raw = data[:, 3] # Fourth column is number of samples

    
    
    # Filter out rows with NaN in critical columns
    valid_data_mask = (~np.isnan(event_TS) &
                       ~np.isnan(event_fine_TS) &
                       ~np.isnan(event_energy))
                    #    ~np.isnan(num_samples_raw)
                       
    
    if not np.any(valid_data_mask):
        print(f"Warning: No valid rows found in {csv_file}")
        return
    
    # Apply the mask and convert to appropriate types
    TS_filtered = event_TS[valid_data_mask]
    fine_TS_filtered = event_fine_TS[valid_data_mask]
    energies_filtered = event_energy[valid_data_mask]
    # num_samples = num_samples_raw[valid_data_mask].astype(int)
    data_filtered = data[valid_data_mask] 

    
    with h5py.File(hdf5_filename, 'w') as f:
        # Save timestamps
        f.create_dataset('timestamps', data=TS_filtered, dtype='uint64', compression='gzip')
        f.create_dataset('fine_timestamps', data=fine_TS_filtered, compression='gzip')
        # Save energy values
        f.create_dataset('energies', data=energies_filtered, compression='gzip')
        # Save ADC data
        f.create_dataset('adc_data', data=data_filtered[:, 4:], dtype='int16',
                         compression='gzip')
        # Save metadata
        f.attrs['num_events'] = len(TS_filtered)
        f.attrs['num_samples_per_event'] = data_filtered.shape[1] - 4
        f.attrs['sampling_rate'] = sampling_rate
        f.attrs['adc_voltage_scaling'] = (2.5 / 4096.0)
        f.attrs['event_timestamp_unit'] = 'ns'
        f.attrs['digitizer'] = 'CAEN DT5743'
        f.attrs['source_file'] = csv_file
    print(f"Saved: {hdf5_filename}")


# Main execution
if __name__ == "__main__":
    # Replace 'your_filename.txt' with the actual filename
    # filename = '2025-281_160738_Wave_0_4.txt'
    # filename = '2025-282_115400_Wave_0_4.txt'
    # filename = r'data_output\2025-10-10_08-18-21_Wave_0_0.txt'
    # filename = r'data_output\2025-10-10_08-18-21_Wave_0_0.txt'
    # filename = r'2025-10-09_19-48-51_Wave_0_0.txt'
    # filename = r'test.csv'    


    csv_files = [f for f in os.listdir('.') if f.endswith('Wave_0_0.txt')]

    for filename in csv_files:
        print(f"Processing file: {filename}")
        try:
            convert_csv_to_hdf5(filename)
        except FileNotFoundError:
            print(f"File '{filename}' not found. "
                "Please check the filename and path.")
        except Exception as e:
            print(f"Error processing file: {e}")

