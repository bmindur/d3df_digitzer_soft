import os
import re
import numpy as np
import h5py


__all__ = [
    'parse_run_info',
    'convert_csv_to_hdf5',
    'convert_dt_to_h5',
    'convert_folder'
]

RUN_INFO_PATTERN = 'run_info'
WAVE_SUFFIX = 'Wave_0_0.txt'

def parse_run_info(run_info_path):
    if not run_info_path or not os.path.isfile(run_info_path):
        return {}
    results = {}
    current_section = None
    channel_thresholds = {}
    common_threshold = None
    p_pmt_hv = re.compile(r'^\s*PMT_HV\s*=\s*([+-]?\d+)')
    p_source = re.compile(r'^\s*SOURCE\s*=\s*(.+)')
    p_scint = re.compile(r'^\s*SCINTILLATOR\s*=\s*(.+)')
    p_trigger = re.compile(r'^\s*TRIGGER_THRESHOLD\s*=\s*([+-]?[0-9]*\.?[0-9]+(?:[Ee][+-]?\d+)?)')
    p_section = re.compile(r'^\s*\[(.+?)\]\s*$')
    p_board_channel = re.compile(r'BOARD\s*(\d+)\s*-\s*CHANNEL\s*(\d+)', re.IGNORECASE)
    try:
        with open(run_info_path, 'r', encoding='utf-8', errors='ignore') as fh:
            for raw_line in fh:
                line = raw_line.strip()
                if not line:
                    continue
                m_sec = p_section.match(line)
                if m_sec:
                    current_section = m_sec.group(1).strip().upper()
                    continue
                m_pmt = p_pmt_hv.match(line)
                if m_pmt and 'pmt_hv' not in results:
                    results['pmt_hv'] = int(m_pmt.group(1))
                m_src = p_source.match(line)
                if m_src and 'source' not in results:
                    results['source'] = m_src.group(1).strip()
                m_sci = p_scint.match(line)
                if m_sci and 'scintillator' not in results:
                    results['scintillator'] = m_sci.group(1).strip()
                m_trig = p_trigger.match(line)
                if m_trig:
                    val = float(m_trig.group(1))
                    if current_section == 'COMMON':
                        common_threshold = val
                    else:
                        m_bc = p_board_channel.search(current_section or '')
                        if m_bc:
                            board = m_bc.group(1)
                            channel = m_bc.group(2)
                            channel_thresholds[(board, channel)] = val
    except Exception:
        return results
    if common_threshold is not None:
        results['trigger_threshold_common'] = common_threshold
    for (board, channel), thresh in channel_thresholds.items():
        results[f'trigger_threshold_board{board}_ch{channel}'] = thresh
    return results

def _derive_run_info(csv_path):
    base = os.path.splitext(os.path.basename(csv_path))[0]
    guess = base.replace('Wave_0_0', 'run_info') + '.txt'
    return os.path.join(os.path.dirname(csv_path), guess)

def convert_csv_to_hdf5(
    csv_file,
    output_prefix=None,
    sampling_rate=3_200_000_000,
):
    if output_prefix is None:
        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        output_prefix = base_name
    csv_dir = os.path.dirname(csv_file)
    hdf5_filename = os.path.join(csv_dir, f"{output_prefix}.h5")
    run_info_path = _derive_run_info(csv_file)
    run_info_data = parse_run_info(run_info_path)
    if os.path.exists(hdf5_filename):
        print(f"HDF5 exists; skip: {hdf5_filename}")
        return hdf5_filename
    try:
        data = np.loadtxt(csv_file, dtype=float, ndmin=2)
    except ValueError:
        data = np.genfromtxt(csv_file, dtype=float, filling_values=np.nan)
    if data.size == 0 or data.shape[1] < 4:
        print(f"Warning: invalid data in {csv_file}")
        return None
    event_TS = data[:, 0] * 5
    event_fine_TS = data[:, 1]
    event_energy = data[:, 2]
    valid_mask = (
        ~np.isnan(event_TS)
        & ~np.isnan(event_fine_TS)
        & ~np.isnan(event_energy)
    )
    if not np.any(valid_mask):
        print(f"Warning: no valid rows in {csv_file}")
        return None
    TS_filtered = event_TS[valid_mask]
    fine_TS_filtered = event_fine_TS[valid_mask]
    energies_filtered = event_energy[valid_mask]
    data_filtered = data[valid_mask]
    with h5py.File(hdf5_filename, 'w') as f:
        f.create_dataset(
            'timestamps', data=TS_filtered, dtype='uint64', compression='gzip'
        )
        f.create_dataset(
            'fine_timestamps', data=fine_TS_filtered, compression='gzip'
        )
        f.create_dataset(
            'energies', data=energies_filtered, compression='gzip'
        )
        f.create_dataset(
            'adc_data',
            data=data_filtered[:, 4:],
            dtype='int16',
            compression='gzip',
        )
        f.attrs['num_events'] = len(TS_filtered)
        f.attrs['num_samples_per_event'] = data_filtered.shape[1] - 4
        f.attrs['sampling_rate'] = sampling_rate
        f.attrs['adc_voltage_scaling'] = (2.5 / 4096.0)
        f.attrs['event_timestamp_unit'] = 'ns'
        f.attrs['digitizer'] = 'CAEN DT5743'
        f.attrs['source_file'] = csv_file
        for k, v in run_info_data.items():
            f.attrs[k] = v
        if run_info_data:
            f.attrs['run_info_file'] = run_info_path
    print(f"Saved: {hdf5_filename}")
    return hdf5_filename

def convert_dt_to_h5(
    csv_file,
    output_prefix=None,
    sampling_rate=3_200_000_000,
):
    return convert_csv_to_hdf5(
        csv_file, output_prefix=output_prefix, sampling_rate=sampling_rate
    )

# Backward compatibility alias (old __init__ expected convert_dt_to_hdf5)
def convert_dt_to_hdf5(
    csv_file,
    output_prefix=None,
    sampling_rate=3_200_000_000,
):
    return convert_dt_to_h5(
        csv_file, output_prefix=output_prefix, sampling_rate=sampling_rate
    )

def convert_folder(
    folder_path,
    sampling_rate=3_200_000_000,
    pattern=WAVE_SUFFIX,
):
    if not os.path.isdir(folder_path):
        raise FileNotFoundError(f"Folder not found: {folder_path}")
    files = [f for f in os.listdir(folder_path) if f.endswith(pattern)]
    if not files:
        print(f"No files matching '{pattern}' in {folder_path}")
        return []
    outputs = []
    for fname in files:
        full = os.path.join(folder_path, fname)
        print(f"Processing: {fname}")
        out = convert_csv_to_hdf5(full, sampling_rate=sampling_rate)
        if out:
            outputs.append(out)
    return outputs
