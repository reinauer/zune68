# MUI5 development performance comparison

This compares Zune68's published classic baseline `7b847fba` with
`mui5-dev` at `6abcf70c`, using the explicit version 21.227 profile.
The candidate includes the requester/process, font-family, String action,
List and Text/RTG completion pass, followed by plain-text and layout
optimizations. This compares Zune68 revisions, not proprietary MUI 3.8
against proprietary MUI 5.

A subsequent requester-worker naming fix is validated separately; it is
not included in these frozen timings. The benchmark does not invoke ASL
requesters.

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

## Results after the completion and optimization pass

| Operation | 68000 change | 68020 change |
| --- | ---: | ---: |
| Creation/disposal | -1.30% | +2.37% |
| Layout | +1.38% | +1.09% |
| List insertion | -4.89% | -2.71% |
| Text changes | -0.44% | -0.53% |

The text parsing/drawing changes improve list insertion and text updates.
Layout still has a small cost, and 68020 creation/disposal remains slower.
The work does not establish complete speed parity with the classic build.
Creation and notification runs show variation; retain their ranges when
interpreting the medians. These are not confidence intervals.

All time columns are EClock ticks per reported batch, not per item.

| CPU | Operation | Classic median | MUI5 median | Change | Classic run range | MUI5 run range |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 68000 | create+dispose | 663,678 | 655,024 | -1.30% | 658,361–680,105 | 652,046–661,646 |
| 68000 | open+close | 1,043,091 | 1,054,524 | +1.10% | 1,037,009–1,052,468 | 1,051,410–1,054,590 |
| 68000 | redraw x10 | 3,857,124 | 3,884,585 | +0.71% | 3,857,037–3,857,364 | 3,884,382–3,884,730 |
| 68000 | relayout x5 | 2,884,298 | 2,924,059 | +1.38% | 2,883,999–2,885,404 | 2,923,800–2,924,603 |
| 68000 | list insert200 | 2,207,733 | 2,099,692 | -4.89% | 2,204,924–2,227,285 | 2,097,058–2,101,050 |
| 68000 | list active x50 | 4,344,581 | 4,207,487 | -3.16% | 4,338,723–4,366,795 | 4,203,089–4,208,616 |
| 68000 | notify x100 | 1,754,408 | 1,751,816 | -0.15% | 1,752,700–1,756,189 | 1,750,192–1,754,178 |
| 68000 | text set x100 | 521,531 | 519,253 | -0.44% | 521,503–523,404 | 519,245–519,298 |
| 68020 | create+dispose | 105,558 | 108,061 | +2.37% | 105,528–106,547 | 107,644–108,162 |
| 68020 | open+close | 383,605 | 383,548 | -0.01% | 383,549–383,640 | 383,538–383,615 |
| 68020 | redraw x10 | 1,121,117 | 1,127,141 | +0.54% | 1,120,269–1,121,283 | 1,127,062–1,127,763 |
| 68020 | relayout x5 | 719,725 | 727,557 | +1.09% | 719,492–720,159 | 727,531–727,578 |
| 68020 | list insert200 | 420,492 | 409,084 | -2.71% | 419,064–422,856 | 408,898–409,817 |
| 68020 | list active x50 | 864,459 | 851,784 | -1.47% | 862,635–870,379 | 849,198–851,980 |
| 68020 | notify x100 | 499,427 | 487,304 | -2.43% | 487,061–500,385 | 487,078–500,103 |
| 68020 | text set x100 | 126,951 | 126,272 | -0.53% | 126,659–127,673 | 125,704–126,706 |

Raw results: [68000](results/mui5-perf-20261014-completion-68000.json)
and [68020](results/mui5-perf-20261014-completion-68020.json).

The final comparison uses only the frozen candidate above. Intermediate
optimization runs are excluded. The fixtures explicitly load Kickstart
3.2.3 (47.115): ROM CRC E1F50B0B on 68000 and B18D3B67 on 68020.

## What changed in the hot paths

- Title objects carry an Area flag, avoiding a class-name walk when a
  Group receives its first child.
- Internal relative layout calls share the layout implementation directly.
  Built-in groups avoid the redundant public Area wrapper; custom Group
  subclasses still receive MUIM_Layout and can reject or replace layout.
- Built-in class lookup uses a 32-entry cache with full-name validation,
  costing 128 bytes per library base. It borrows classes without adding
  references and clears before any partial or complete class expunge.
- Plain text uses a compact arena and bypasses the general escape parser
  and chunk renderer. Elided views retain the general renderer.
- Text construction scans tags once and avoids allocating an empty
  preparse string.
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

## Size and live RAM

The stripped library grows from 334,840 to 404,056 bytes (+20.67%).
Loadable CODE/DATA/BSS grows from 314,736 to 379,444 bytes (+20.56%).
These fixed costs are separate from per-object allocations and window
setup storage. The native memory comparison and implemented savings are
in [MEMORY.mui5.md](MEMORY.mui5.md).

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
