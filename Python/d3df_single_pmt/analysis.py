import os
import numpy as np
import pandas as pd
import h5py
from .converter import parse_run_info


__all__ = [
    'load_hdf5_data',
    'align_pulses_by_peak',
    'normalize_pulses_to_max',
    'analyze_pulse_timing',
    'analyze_file',
    'analyze_folder'
]

def load_hdf5_data(hdf5_file):
    try:
        with h5py.File(hdf5_file, 'r') as f:
            timestamps = f['timestamps'][:]
            adc_data = f['adc_data'][:]
            metadata = {k: f.attrs[k] for k in f.attrs.keys()}
            need_run = any(
                k not in metadata
                for k in [
                    'pmt_hv',
                    'source',
                    'scintilator',
                    'trigger_threshold_common',
                ]
            )
            run_info_candidates = []
            if 'run_info_file' in metadata:
                run_info_candidates.append(str(metadata['run_info_file']))
            if 'source_file' in metadata:
                base = os.path.basename(str(metadata['source_file']))
                derived = base.replace('Wave_0_0', 'run_info') + '.txt'
                run_info_candidates.append(
                    os.path.join(os.path.dirname(hdf5_file), derived)
                )
            run_info_candidates.extend([
                os.path.join(os.path.dirname(hdf5_file), fn)
                for fn in os.listdir(os.path.dirname(hdf5_file))
                if fn.lower().startswith('run_info') and fn.lower().endswith('.txt')
            ])
            if need_run:
                for cand in run_info_candidates:
                    parsed = parse_run_info(cand)
                    if parsed:
                        metadata.update(parsed)
                        metadata.setdefault('run_info_file', cand)
                        break
        sampling_rate = metadata.get('sampling_rate', 3.2e9)
        n_samples = adc_data.shape[1]
        timestamps_df = pd.DataFrame(
            {'timestamp': timestamps},
            index=pd.RangeIndex(len(timestamps), name='Event'),
        )
        scaled_adc = adc_data * metadata.get('adc_voltage_scaling', 1.0)
        ADC_df = pd.DataFrame(
            scaled_adc,
            index=timestamps_df.index,
            columns=pd.RangeIndex(n_samples, name='Sample'),
        )
        return ADC_df, timestamps_df, metadata
    except Exception as e:
        print(f"Failed to load {hdf5_file}: {e}")
        return None, None, {}

def align_pulses_by_peak(ADC_df, reference_position=None, search_window=None):
    if ADC_df is None or ADC_df.empty:
        return None, None
    n_pulses, n_samples = ADC_df.shape
    if reference_position is None:
        reference_position = n_samples // 2
    if search_window is None:
        search_window = (0, n_samples)
    peak_positions = np.zeros(n_pulses, dtype=int)
    for i in range(n_pulses):
        pulse = ADC_df.iloc[i].values
        region = pulse[search_window[0]:search_window[1]]
        if region.size == 0:
            peak_positions[i] = reference_position
            continue
        if np.max(region) > abs(np.min(region)):
            peak_idx = np.argmax(region) + search_window[0]
        else:
            peak_idx = np.argmin(region) + search_window[0]
        peak_positions[i] = peak_idx
    aligned = np.zeros_like(ADC_df.values)
    for i in range(n_pulses):
        shift = reference_position - peak_positions[i]
        pulse = ADC_df.iloc[i].values
        if shift == 0:
            aligned[i] = pulse
        elif shift > 0:
            aligned[i, shift:] = pulse[: n_samples - shift]
            aligned[i, :shift] = pulse[0]
        else:
            aligned[i, :shift] = pulse[-shift:]
            aligned[i, shift:] = pulse[-1]
    return (
        pd.DataFrame(aligned, index=ADC_df.index, columns=ADC_df.columns),
        peak_positions,
    )

def normalize_pulses_to_max(ADC_df, method='individual'):
    if ADC_df is None or ADC_df.empty:
        return None
    out = ADC_df.copy()
    if method == 'individual':
        for i in range(out.shape[0]):
            pulse = out.iloc[i]
            pmax, pmin = pulse.max(), pulse.min()
            out.iloc[i] = (pulse - pmin) / (pmax - pmin) if pmax != pmin else 0
    elif method == 'global':
        gmax, gmin = out.values.max(), out.values.min()
        out = (out - gmin) / (gmax - gmin) if gmax != gmin else out*0
    elif method == 'baseline':
        for i in range(out.shape[0]):
            pulse = out.iloc[i]
            base = pulse.min()
            corr = pulse - base
            pmax = corr.max()
            out.iloc[i] = corr / pmax if pmax > 0 else 0
    return out

def _measure_rise_positive(mp, low, high):
    rs = re = None
    for i in range(1, len(mp)):
        if mp[i - 1] < low and mp[i] >= low:
            rs = i
            break
    if rs is not None:
        for i in range(rs, len(mp)):
            if mp[i] >= high:
                re = i
                break
    return {
        'rise_start_idx': rs,
        'rise_end_idx': re,
        'rise_time': (re - rs) if rs is not None and re is not None else None,
    }

def _measure_rise_negative(mp, low, high):
    rs = re = None
    for i in range(1, len(mp)):
        if mp[i - 1] > low and mp[i] <= low:
            rs = i
            break
    if rs is not None:
        for i in range(rs, len(mp)):
            if mp[i] <= high:
                re = i
                break
    return {
        'rise_start_idx': rs,
        'rise_end_idx': re,
        'rise_time': (re - rs) if rs is not None and re is not None else None,
    }

def _measure_fall_positive(mp, low, high, peak_idx):
    fs = fe = None
    for i in range(peak_idx, len(mp)-1):
        if mp[i] >= high and mp[i + 1] < high:
            fs = i
            break
    if fs is not None:
        for i in range(fs, len(mp)):
            if mp[i] <= low:
                fe = i
                break
    return {
        'fall_start_idx': fs,
        'fall_end_idx': fe,
        'fall_time': (fe - fs) if fs is not None and fe is not None else None,
    }

def _measure_fall_negative(mp, low, high, peak_idx):
    fs = fe = None
    for i in range(peak_idx, len(mp)-1):
        if mp[i] <= high and mp[i + 1] > high:
            fs = i
            break
    if fs is not None:
        for i in range(fs, len(mp)):
            if mp[i] >= low:
                fe = i
                break
    return {
        'fall_start_idx': fs,
        'fall_end_idx': fe,
        'fall_time': (fe - fs) if fs is not None and fe is not None else None,
    }

def _measure_width(mp, mid, positive):
    ws = we = None
    if positive:
        for i in range(1, len(mp)):
            if mp[i-1] < mid and mp[i] >= mid: ws = i; break
        for i in range(len(mp)-2, -1, -1):
            if mp[i] >= mid and mp[i+1] < mid: we = i; break
    else:
        for i in range(1, len(mp)):
            if mp[i-1] > mid and mp[i] <= mid: ws = i; break
        for i in range(len(mp)-2, -1, -1):
            if mp[i] <= mid and mp[i+1] > mid: we = i; break
    return {'width_start_idx': ws, 'width_end_idx': we, 'pulse_width': (we - ws) if ws is not None and we is not None else None}

def analyze_pulse_timing(ADC_df, sampling_rate, method='individual', threshold_low=0.1, threshold_high=0.9, align=True):
    if ADC_df is None or ADC_df.empty or sampling_rate <= 0:
        return None
    if align:
        ADC_df, _ = align_pulses_by_peak(ADC_df)
    tps = 1.0 / sampling_rate
    norm = normalize_pulses_to_max(ADC_df, method=method)
    mean_pulse = norm.mean(axis=0).values
    baseline = np.median(mean_pulse[:10])
    peak_max, peak_min = mean_pulse.max(), mean_pulse.min()
    if abs(peak_max - baseline) > abs(peak_min - baseline):
        positive = True; peak = peak_max; peak_idx = np.argmax(mean_pulse)
        amp = peak - baseline; low = baseline + amp * threshold_low; high = baseline + amp * threshold_high; mid = baseline + amp * 0.5
    else:
        positive = False; peak = peak_min; peak_idx = np.argmin(mean_pulse)
        amp = abs(peak - baseline); low = baseline - amp * threshold_low; high = baseline - amp * threshold_high; mid = baseline - amp * 0.5
    info = {
        'baseline': baseline, 'peak': peak, 'amplitude': amp, 'is_positive_pulse': positive, 'peak_idx': peak_idx,
        'threshold_low': threshold_low, 'threshold_high': threshold_high, 'low_level': low, 'high_level': high,
        'mid_level': mid, 'mean_pulse': mean_pulse, 'time_per_sample': tps, 'sample_rate': sampling_rate
    }
    info.update(_measure_rise_positive(mean_pulse, low, high) if positive else _measure_rise_negative(mean_pulse, low, high))
    info.update(_measure_fall_positive(mean_pulse, low, high, peak_idx) if positive else _measure_fall_negative(mean_pulse, low, high, peak_idx))
    info.update(_measure_width(mean_pulse, mid, positive))
    if info.get('rise_time') is not None: info['rise_time_ns'] = info['rise_time'] * tps * 1e9
    if info.get('fall_time') is not None: info['fall_time_ns'] = info['fall_time'] * tps * 1e9
    if info.get('pulse_width') is not None: info['pulse_width_ns'] = info['pulse_width'] * tps * 1e9
    return info

def analyze_file(hdf5_file, align=True):
    ADC_df, ts_df, meta = load_hdf5_data(hdf5_file)
    if ADC_df is None: return None
    sr = meta.get('sampling_rate', 0)
    timing = analyze_pulse_timing(ADC_df, sr, align=align)
    return {'file': hdf5_file, 'metadata': meta, 'timing': timing}

def analyze_folder(folder_path, pattern='.h5', align=True):
    files = [os.path.join(folder_path, f) for f in os.listdir(folder_path) if f.endswith(pattern)]
    results = []
    for fpath in files:
        print(f"Analyzing {os.path.basename(fpath)}")
        res = analyze_file(fpath, align=align)
        if res and res['timing']:
            t = res['timing']; meta = res['metadata']
            summary = {
                'file': os.path.basename(fpath), 'sampling_rate': meta.get('sampling_rate', np.nan), 'baseline': t.get('baseline', np.nan),
                'amplitude': t.get('amplitude', np.nan), 'rise_samples': t.get('rise_time', -1), 'fall_samples': t.get('fall_time', -1),
                'width_samples': t.get('pulse_width', -1), 'rise_time_ns': t.get('rise_time_ns', np.nan), 'fall_time_ns': t.get('fall_time_ns', np.nan),
                'pulse_width_ns': t.get('pulse_width_ns', np.nan), 'pmt_hv': meta.get('pmt_hv', np.nan), 'source': meta.get('source', ''),
                'scintilator': meta.get('scintilator', ''), 'trigger_threshold_common': meta.get('trigger_threshold_common', np.nan)
            }
            results.append(summary)
    return pd.DataFrame(results)

def main():
    import argparse
    from datetime import datetime
    
    parser = argparse.ArgumentParser(
        description='Analyze DT5743 HDF5 waveform files in folder.'
    )
    parser.add_argument(
        'folder', nargs='?', default='.',
        help='Folder containing .h5 files'
    )
    parser.add_argument(
        '--no-align', action='store_true',
        help='Disable peak alignment'
    )
    parser.add_argument(
        '--save-csv', action='store_true',
        help='Save summary CSV'
    )
    parser.add_argument(
        '--no-hdf5', action='store_true',
        help='Skip automatic HDF5 output (default: save HDF5)'
    )
    args = parser.parse_args()
    
    df = analyze_folder(args.folder, pattern='.h5', align=(not args.no_align))
    print(df)
    
    if df.empty:
        print('No results to save.')
        return
    
    # Save HDF5 by default (unless --no-hdf5 specified)
    if not args.no_hdf5:
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        hdf5_out = os.path.join(
            args.folder, f'analysis_results_{timestamp}.h5'
        )
        # Use h5py for saving (already a dependency)
        with h5py.File(hdf5_out, 'w') as f:
            for col in df.columns:
                f.create_dataset(
                    col, data=df[col].values, compression='gzip'
                )
            f.attrs['num_files'] = len(df)
            f.attrs['timestamp'] = timestamp
            f.attrs['columns'] = list(df.columns)
        print(f"Saved analysis results: {hdf5_out}")
    
    # Save CSV if requested
    if args.save_csv:
        csv_out = os.path.join(args.folder, 'dt5743_analysis_summary.csv')
        df.to_csv(csv_out, index=False)
        print(f"Saved summary CSV: {csv_out}")

if __name__ == '__main__':
    main()
