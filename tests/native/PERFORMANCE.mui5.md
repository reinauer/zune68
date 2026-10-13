# MUI5 development performance comparison

This compares Zune68's published classic baseline `7b847fba` with the
MUI5 development implementation `c0e9b043`, using the explicit version
21.227 profile. It does not compare proprietary MUI 3.8 against MUI 5.

## Method

Both versions use the same benchmark executable within each CPU fixture,
built with GCC 13.4, `-m68000 -msoft-float -Os -noixemul`. The libraries
use the same native makefile optimization and alignment flags and
`TRACE=0`. The benchmark opens a private 640x256 depth-2 planar screen.
It exercises ordinary widgets and existing APIs, so the measurement asks
whether adding MUI5 support slows classic workloads.

Each CPU has three fresh-boot runs per version, alternating the order
baseline/candidate, candidate/baseline, baseline/candidate. Each reported
run value is a median: 15 repetitions per phase on 68020 and 5 on 68000.
The table summarizes the median of those three run medians and retains
the full observed ranges. Positive percentages mean more time/slower.

The guests run AmigaOS 3.2.3 in WinUAE without JIT. Both configurations use
`cpu_speed=real`, `cpu_multiplier=4`, memory-cycle and blitter-cycle
emulation, and `cpu_cycle_exact=false`. The 68000 fixture uses ECS and no
FPU; the 68020 fixture uses AGA. The fixtures have 2 MiB Chip and 4 MiB
Fast RAM; the 68020 also has 16 MiB Z3 RAM. No physical CPU clock rate is
inferred from these emulator settings. EClock frequency was 709379 Hz.

The two CPU fixtures are independent. Comparisons are within one CPU
fixture, with matched OS, preferences and screen settings. These are
emulator results, not cycle-accurate CPU benchmarks or physical-hardware
measurements. They do not cover new MUI5-only features, complex themes,
RTG drawing or complete applications. Small differences can include
frame scheduling and emulator timing variation.

The final runs use exclusively locked disposable fixtures and frozen
library copies. The runner records binary, library, emulator and config
SHA-256 hashes and rejects changed library inputs. Earlier diagnostic
runs, including overlapping 68020 runners, are excluded.

## Results

The largest median increase was **2.83%**, for creating and disposing the
widget tree on 68000. Layout increased **1.98% on 68000** and **1.32% on
68020**. Drawing, list operations and text changes were below 1% in this
fixture. The 68020 notification median increased 2.36%, with overlapping
run ranges; 68000 notification timing changed by -0.07%.

This workload shows a small compatibility cost, with no large slowdown.
Creation and layout are the clearest targets for subsequent profiling.
Keep these measurements as a development baseline and repeat them after
changes to frequently used dispatch, layout or drawing paths. Three run
medians do not establish a statistical confidence interval.

All time columns below are EClock ticks per reported batch, not per item.
For example, list insert200 times insertion of 200 entries; redraw x10
contains ten redraws.

| CPU | Operation | Classic median | MUI5 median | Change | Classic run range | MUI5 run range |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 68000 | create+dispose | 658,355 | 676,993 | +2.83% | 656,059–677,407 | 672,859–679,181 |
| 68000 | open+close | 1,039,000 | 1,052,744 | +1.32% | 1,038,994–1,052,501 | 1,052,611–1,056,052 |
| 68000 | redraw x10 | 3,856,904 | 3,872,339 | +0.40% | 3,856,539–3,857,002 | 3,872,277–3,873,045 |
| 68000 | relayout x5 | 2,884,949 | 2,941,975 | +1.98% | 2,883,925–2,885,664 | 2,941,893–2,942,111 |
| 68000 | list insert200 | 2,204,780 | 2,213,618 | +0.40% | 2,204,619–2,229,018 | 2,202,883–2,214,259 |
| 68000 | list active x50 | 4,339,375 | 4,348,682 | +0.21% | 4,338,370–4,365,504 | 4,341,749–4,349,906 |
| 68000 | notify x100 | 1,753,176 | 1,752,031 | -0.07% | 1,752,541–1,753,262 | 1,750,089–1,752,036 |
| 68000 | text set x100 | 521,509 | 522,727 | +0.23% | 521,495–521,531 | 522,686–522,731 |
| 68020 | create+dispose | 106,405 | 108,049 | +1.55% | 105,611–107,079 | 107,615–108,523 |
| 68020 | open+close | 383,552 | 383,567 | +0.00% | 383,490–383,574 | 383,565–383,643 |
| 68020 | redraw x10 | 1,120,969 | 1,124,088 | +0.28% | 1,120,152–1,121,159 | 1,123,490–1,124,108 |
| 68020 | relayout x5 | 720,017 | 729,526 | +1.32% | 719,511–720,039 | 729,095–729,607 |
| 68020 | list insert200 | 422,350 | 425,552 | +0.76% | 420,206–423,121 | 424,039–425,642 |
| 68020 | list active x50 | 867,246 | 870,680 | +0.40% | 866,106–868,780 | 869,664–872,924 |
| 68020 | notify x100 | 486,575 | 498,038 | +2.36% | 485,960–487,788 | 486,410–499,357 |
| 68020 | text set x100 | 126,766 | 127,705 | +0.74% | 126,681–126,806 | 127,113–127,714 |

Raw results: [68000](results/mui5-perf-20261014-68000.json) and
[68020](results/mui5-perf-20261014-68020.json).


## Size

The stripped muimaster library grows from **334,840 to 365,668 bytes**:
+30,828 bytes (+9.21%). The linked text/data/BSS totals grow from 314,736
to 343,972 bytes (+29,236). This is not a measurement of complete live
application memory; objects, pools, display buffers and library-loader
allocations also matter. New class storage is allocated when used, and
ordinary Lists avoid the column-mapping scratch allocation.

## Reproduce

Use `perfbench.c` from this revision and the MUI SDK headers. The explicit
`va_start` object wrapper avoids an older SDK inline-varargs issue with
optimizing GCC builds. Do not reuse a binary that hangs during initial
object creation. Output is unbuffered so failures retain the last phase.

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os \
  -I/path/to/SDK/MUI/C/include -Wl,-u,___stkinit \
  tests/native/perfbench.c -o /tmp/perfbench -lamiga
```

For the shorter 68000 runs add `-DPERFBENCH_REPS=5`; the default is 15.
Build both revisions into separate directories, with MUI5_TEST=1 for the
candidate. Keep those library files immutable during the comparison.
Prepare a disposable fixture containing `run.uae`, its private OS disk,
and `guest/package/Libs`. Configure Bench: to mount `guest`, and make
startup execute `Bench:run-tests`. Keep preferences identical for both
versions. Never use a fixture already open in another emulator.

```sh
python3 tests/native/compare_perf.py \
  --fixture /tmp/private-perf-fixture \
  --emulator /path/to/winuae --binary /tmp/perfbench \
  --baseline /tmp/classic-build --candidate /tmp/mui5-build \
  --output /tmp/perf-results --trials 3
```

The runner restores the fixture's library files, script and benchmark
binary on exit. It preserves the private OS disk's normal guest writes.
All successful phases and `rc=0` are required; an end-of-script marker
alone does not count as a successful measurement. Raw per-run medians,
configuration choices and input hashes are included under `results/`.
