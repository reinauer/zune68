# MUI5 development performance comparison

This compares Zune68's published classic baseline `7b847fba` with
`mui5-dev` at `38df06fc`, using the explicit version 21.227 profile.
The candidate includes Filepanel/Fontpanel, Text marking, menu copying
and nested popup handling, List formatting/object cells, and the creation
and layout optimizations. This does not compare proprietary MUI 3.8
against proprietary MUI 5.

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

## Results after the selector and interaction work

On 68000, creation/disposal changed **-0.02%** and layout
changed **+1.71%** relative to the classic baseline.

On 68020, creation/disposal changed **+2.37%** and layout
changed **+1.27%** relative to the classic baseline.

The earlier 68000 creation regression is no longer apparent in this
fixture. The largest remaining median increase is 68020 creation at
2.37%, compared with 1.55% before the selector/interaction extensions.
Layout remains slower on both CPUs, and 68000 text changes increased
1.19%. The optimizations have not eliminated every compatibility cost.

Positive percentages mean more time/slower. Creation and some notification
runs show appreciable variation; retain the ranges below when interpreting
small differences. These medians do not establish statistical confidence
intervals or physical-hardware performance.

All time columns are EClock ticks per reported batch, not per item.

| CPU | Operation | Classic median | MUI5 median | Change | Classic run range | MUI5 run range |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 68000 | create+dispose | 674,331 | 674,213 | -0.02% | 664,906–678,827 | 662,613–674,914 |
| 68000 | open+close | 1,048,127 | 1,052,449 | +0.41% | 1,041,974–1,048,891 | 1,052,429–1,052,612 |
| 68000 | redraw x10 | 3,856,927 | 3,882,542 | +0.66% | 3,856,593–3,859,254 | 3,882,464–3,882,562 |
| 68000 | relayout x5 | 2,884,223 | 2,933,631 | +1.71% | 2,883,700–2,886,099 | 2,933,584–2,934,014 |
| 68000 | list insert200 | 2,218,570 | 2,214,645 | -0.18% | 2,208,231–2,227,704 | 2,210,909–2,214,760 |
| 68000 | list active x50 | 4,355,932 | 4,353,836 | -0.05% | 4,344,424–4,364,961 | 4,351,802–4,354,008 |
| 68000 | notify x100 | 1,754,309 | 1,750,163 | -0.24% | 1,752,262–1,754,516 | 1,749,908–1,751,905 |
| 68000 | text set x100 | 521,545 | 527,767 | +1.19% | 521,502–521,618 | 527,730–528,603 |
| 68020 | create+dispose | 104,697 | 107,175 | +2.37% | 104,372–104,974 | 106,886–107,925 |
| 68020 | open+close | 383,616 | 383,586 | -0.01% | 383,545–383,655 | 383,524–383,649 |
| 68020 | redraw x10 | 1,120,605 | 1,125,273 | +0.42% | 1,120,524–1,122,553 | 1,125,267–1,125,382 |
| 68020 | relayout x5 | 719,877 | 729,031 | +1.27% | 719,752–720,922 | 728,746–729,199 |
| 68020 | list insert200 | 422,953 | 424,197 | +0.29% | 422,782–426,692 | 423,801–426,263 |
| 68020 | list active x50 | 868,868 | 872,044 | +0.37% | 868,040–871,361 | 871,909–873,374 |
| 68020 | notify x100 | 500,365 | 485,867 | -2.90% | 486,989–500,418 | 485,737–499,097 |
| 68020 | text set x100 | 127,586 | 128,586 | +0.78% | 127,302–127,719 | 127,981–128,847 |

Raw final results: [68000](results/mui5-perf-20261014-panels-68000.json)
and [68020](results/mui5-perf-20261014-panels-68020.json).

The final comparison uses only the frozen candidate above. Earlier
intermediate optimization runs are excluded, including the diagnostic
pair that briefly overlapped a separately launched emulator. The final
fixtures explicitly load Kickstart 3.2.3 (47.115): A500/A600/A2000/CDTV
ROM CRC E1F50B0B on 68000 and A1200 ROM CRC B18D3B67 on 68020.

## What changed in the hot paths

- Title objects carry an Area flag, avoiding a class-name walk when a
  Group receives its first child.
- Internal relative layout calls share the layout implementation directly.
  Built-in groups avoid the redundant public Area wrapper; custom Group
  subclasses still receive MUIM_Layout and can reject or replace layout.
- Built-in class lookup uses a 32-entry cache with full-name validation,
  costing 128 bytes per library base. It borrows classes without adding
  references and clears before any partial or complete class expunge.
- List column accessors are inlined. Optional Text marking allocates its
  selection state only when enabled.

Linked 68000 checks, native custom-Group/layout-hook probes and Title tests
passed after the dispatch changes. The cache test covers name collisions,
mutable input names and recreation after partial expunge. The final
selector build also passed the 68000/no-FPU requester probe. These checks
cover those behaviors, not every application or MUI 5 API.

## Earlier measurement

Before the selector and interaction work, candidate `c0e9b043` showed
+2.83% creation/disposal and +1.98% layout on 68000, and +1.55% creation
and +1.32% layout on 68020. Its original data remains available: [68000](results/mui5-perf-20261014-68000.json)
and [68020](results/mui5-perf-20261014-68020.json). The tables above compare
the final candidate against freshly measured classic runs, rather than
mixing medians from the two sessions.

## Size

The stripped muimaster library grows from **334,840 to 383,536 bytes**:
+48,696 bytes (+14.54%). Linked text/data/BSS totals grow from 314,736
to 360,656 bytes (+45,920). The per-library lookup cache adds 128 bytes
to the allocated library base. This is not a complete live-memory
measurement: object instances, pools, display buffers and loader
allocations also matter. Selector class storage is allocated when used.

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
