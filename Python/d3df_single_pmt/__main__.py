"""Module entry point for `python -m d3df_single_pmt`.

Provides a simple way to list available console script commands
exposing this package's functionality.

Usage examples:
    python -m d3df_single_pmt --list
    python -m d3df_single_pmt --list --verbose

Output shows the command name and the underlying entry point target.
"""

from __future__ import annotations

import argparse
import importlib.metadata as _md
from textwrap import dedent


def _discover_console_scripts():
    scripts = []
    try:
        entry_points = _md.entry_points(group='console_scripts')
    except TypeError:  # Python <3.10 compatibility if needed
        entry_points = _md.entry_points().get('console_scripts', [])  # type: ignore
    for ep in entry_points:
        if 'd3df_single_pmt.' in ep.value:
            scripts.append((ep.name, ep.value))
    scripts.sort(key=lambda t: t[0])
    return scripts


def main():  # noqa: D401 - CLI entry point
    parser = argparse.ArgumentParser(
        description='Utility entry point for d3df_single_pmt package.'
    )
    parser.add_argument(
        '--list', action='store_true', help='List available console commands.'
    )
    parser.add_argument(
        '--verbose', action='store_true', help='Show full entry point targets.'
    )
    args = parser.parse_args()

    if args.list:
        scripts = _discover_console_scripts()
        if not scripts:
            print('No console scripts found.')
            return
        print('Commands:')
        for name, target in scripts:
            if args.verbose:
                print(f'  {name:<18} -> {target}')
            else:
                print(f'  {name}')
        print('\nRun a command with --help for details, e.g.:')
        print('  convert-dt5743 --help')
        return

    # No flags: show short help + discovered commands summary.
    scripts = _discover_console_scripts()
    summary = '\n'.join(f'  - {n}' for n, _ in scripts) or '  (none)'
    print(dedent(
        f"""
        d3df_single_pmt module entry point
        Use --list to show all console commands.

        Available commands:
{summary}
        """
    ).strip())


if __name__ == '__main__':  # pragma: no cover
    main()
