import argparse
from .converter import convert_folder


def main():
    parser = argparse.ArgumentParser(
        description='Convert DT5743 waveform CSV files to HDF5.'
    )
    parser.add_argument(
        'folder', nargs='?', default='.',
        help='Folder to search (default: .)'
    )
    parser.add_argument(
        '--sampling-rate', type=float, default=3_200_000_000,
        help='Sampling rate in Hz (default: 3.2 GHz)'
    )
    parser.add_argument(
        '--pattern', default='Wave_0_0.txt',
        help='Filename suffix pattern (default: Wave_0_0.txt)'
    )
    args = parser.parse_args()

    outputs = convert_folder(
        args.folder,
        sampling_rate=args.sampling_rate,
        pattern=args.pattern,
    )
    if outputs:
        print(f'Converted {len(outputs)} file(s).')
    else:
        print('No files converted.')


if __name__ == '__main__':
    main()
