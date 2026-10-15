#!/usr/bin/env python3
"""Summarize complete before/after runs without using instrumented timings."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import statistics


def table(path, cpu):
    data = json.loads(path.read_text())
    runs = defaultdict(list)
    for run in data['runs']:
        runs[run['profile']].append(run)
    assert set(runs) >= {'zune68', 'candidate'}
    for profile, rows in runs.items():
        assert len(rows) == 3 and {r['trial'] for r in rows} == {0, 1, 2}, profile
        assert len({r['geometry'] for r in rows}) == 1
        assert len({r['eclock'] for r in rows}) == 1
        assert all(len(r['ticks']) == 8 for r in rows)
    assert runs['zune68'][0]['geometry'] == runs['candidate'][0]['geometry']
    phases = list(runs['zune68'][0]['ticks'])
    def median(profile, phase):
        return statistics.median(r['ticks'][phase] * 1000 / r['eclock']
                                 for r in runs[profile])
    lines = [f'### {cpu}', '',
             '| Batch | Before ms | After ms | Time change |' +
             (' After vs MUI 3.8 |' if 'mui38' in runs else ''),
             '|---|---:|---:|---:|' + ('---:|' if 'mui38' in runs else '')]
    values = {}
    for phase in phases:
        before, after = (median(p, phase) for p in ('zune68', 'candidate'))
        delta = (after / before - 1) * 100
        row = f'| {phase} | {before:.2f} | {after:.2f} | {delta:+.2f}% |'
        values[phase] = dict(before_ms=before, after_ms=after, change_percent=delta)
        if 'mui38' in runs:
            ref = median('mui38', phase)
            gap = (after / ref - 1) * 100
            row += f' {gap:+.2f}% |'
            values[phase].update(mui38_ms=ref, mui38_gap_percent=gap)
        lines.append(row)
    lines += ['', 'Negative percentages mean less elapsed time.', '']
    return data, values, '\n'.join(lines)


def replace_section(text, name, value):
    start, end = f'<!-- {name} -->', f'<!-- /{name} -->'
    a, b = text.index(start) + len(start), text.index(end)
    return text[:a] + '\n' + value.rstrip() + '\n' + text[b:]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--m68020', type=Path, required=True)
    p.add_argument('--m68000', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--summary', type=Path, required=True)
    a = p.parse_args()
    d20, v20, t20 = table(a.m68020, '68020')
    d00, v00, t00 = table(a.m68000, '68000')
    for key in ('binary_sha256', 'emulator_sha256', 'pristine_disk_sha256'):
        assert d20[key] == d00[key], key
    for profile in ('zune68', 'candidate'):
        assert d20['profiles'][profile] == d00['profiles'][profile], profile
    a.report.write_text(replace_section(a.report.read_text(), 'RESULTS', t20 + '\n' + t00))
    a.summary.write_text(json.dumps({'68020': v20, '68000': v00}, indent=2) + '\n')


if __name__ == '__main__':
    main()
