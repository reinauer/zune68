# Zune68 versus original MUI 3.8 and MUI 5

## Compared implementations

- Original MUI 3.8, muimaster.library 19.35 (12 February 1997), freshly
  extracted from mui38usr.lha.
- Original MUI 5, muimaster.library 21.227 (31 August 2021), freshly
  extracted from the release archive MUI-5.0-20210831-os3.lha.
- Zune68 mui5-dev at c70d226a, using its opt-in version 21.227 build.

The proprietary MUI 5 and Zune68 builds happen to report the same library
version; they are distinct implementations. Full version strings, archive
and library hashes are retained with the results. No proprietary binaries
are redistributed. The supplied April 2025 OS3 snapshot archive contains
SDK/support files but no muimaster.library; it is not the measured MUI 5.

## Result overview

In this common-widget, default-style workload:

- Zune68 uses less time than MUI 5 in seven of eight measured batches.
  Selection/autoscroll is the exception: Zune68 takes 12.48% longer.
- Against MUI 3.8, Zune68 takes 23.99% less time to insert entries,
  but tree creation/disposal takes 43.72% longer and selection/autoscroll takes
  111.80% longer. Notification timings overlap across runs; text updates
  take about 1% less time.
- Original MUI 5 takes more time than MUI 3.8 in all eight batches:
  approximately 2.40 times as long for creation and 3.11 times as long
  for insertion. The test does not cover complete applications.

Selection/autoscroll is the clearest next profiling target. This
comparison also identifies creation, drawing and group-change costs
relative to MUI 3.8. Use the separate Zune68 before/after measurements to
assess the cost of our MUI 5 additions.

## Method and limits

All implementations run the identical benchmark executable on the same
WinUAE 68020/AGA/AmigaOS 3.2.3 configuration. The MUI 5 release binary
requires a 68020, so the three-way comparison does not use a 68000.
The benchmark itself uses GCC 13.4 with -m68000 -msoft-float -noixemul -Os.
Its workload uses classes and APIs available in MUI 3.8. MUI 5-specific
classes/features, custom themes and RTG rendering are outside this test.

JIT is disabled. The configuration uses cpu_speed=real, cpu_multiplier=4,
cpu_memory_cycle_exact=true, blitter_cycle_exact=true and
cpu_cycle_exact=false. These are matched emulator measurements, not a
physical CPU clock-rate claim or cycle-accurate CPU benchmarks.
The machine has 2 MiB Chip, 4 MiB Fast and 16 MiB Z3 memory.

Each toolkit has its own installation, library search path and supplied
resources. There are no global or perfbench MUI preferences in the OS
image; each toolkit uses its defaults. A pristine OS disk is restored
before every run, and only one test emulator runs at a time.

The private screen is 640x480 at depth 2. The benchmark requests fixed
600x400 sizing, but MUI constrains fixed-size tags to the object tree's
minimum and maximum dimensions and adds toolkit-specific inner spacing.
Actual geometry is recorded below; it is not assumed to match the request.
The benchmark rejects a root larger than the window interior. The earlier
256-line pilots clipped the MUI 5 layout and are excluded from results.
This is a common application workload using default toolkit styling,
not a pixel-identical rendering comparison. Visible row counts, padding
and widget geometry can influence drawing/list timings.

Three fresh boots per implementation rotate through:

1. MUI 3.8, MUI 5, Zune68.
2. MUI 5, Zune68, MUI 3.8.
3. Zune68, MUI 3.8, MUI 5.

Each operation takes the median of 15 repetitions within a boot. The
summary takes the median of the three boot medians and retains their
range. Those ranges are not confidence intervals. The class cache is
warmed before timing creation; library loading/startup is not timed.

The creation result is for the whole widget tree, not one object.
List insertion adds 200 entries and checks the resulting entry count.
Active-entry changes step through the 200 entries in jumps of seven,
including automatic scrolling and wraparound. Notification chains and
text updates also happen with the window open and include any synchronous widget drawing. The group
change phase brackets unchanged children with InitChange/ExitChange five
times: it measures that API path, not a structural layout change or a
window resize. There is no meaningful single average across these
heterogeneous workloads.

## Results

All numbers below are medians of the three boot medians.

| Timed batch | MUI 3.8 (ms) | MUI 5 (ms) | Zune68 (ms) |
| --- | ---: | ---: | ---: |
| Create/dispose widget tree | 105.67 | 253.68 | 151.86 |
| Open/close window | 540.84 | 867.83 | 580.83 |
| Full redraw x10 | 1578.27 | 2138.46 | 1784.78 |
| Group change cycle x5 | 961.29 | 1285.04 | 1132.76 |
| Insert 200 entries | 1520.19 | 4724.55 | 1155.51 |
| Selection/autoscroll x50 | 1119.59 | 2108.29 | 2371.32 |
| Notification chain x100 | 705.31 | 1234.72 | 705.17 |
| Text update x100 | 179.48 | 216.61 | 177.77 |

### Relative time

Positive means more time/slower; negative means less time/faster.

| Timed batch | Zune68 vs MUI 3.8 | Zune68 vs MUI 5 | MUI 5 vs MUI 3.8 |
| --- | ---: | ---: | ---: |
| Create/dispose widget tree | +43.72% | -40.14% | +140.07% |
| Open/close window | +7.39% | -33.07% | +60.46% |
| Full redraw x10 | +13.08% | -16.54% | +35.49% |
| Group change cycle x5 | +17.84% | -11.85% | +33.68% |
| Insert 200 entries | -23.99% | -75.54% | +210.79% |
| Selection/autoscroll x50 | +111.80% | +12.48% | +88.31% |
| Notification chain x100 | -0.02% | -42.89% | +75.06% |
| Text update x100 | -0.95% | -17.93% | +20.69% |

### Observed run ranges

EClock ticks per batch, minimum to maximum of the three run medians.

| Timed batch | MUI 3.8 | MUI 5 | Zune68 |
| --- | ---: | ---: | ---: |
| Create/dispose widget tree | 74,697–75,090 | 179,855–180,498 | 107,180–107,819 |
| Open/close window | 383,566–383,859 | 614,845–616,089 | 411,996–412,123 |
| Full redraw x10 | 1,111,809–1,120,239 | 1,513,489–1,521,639 | 1,265,783–1,266,165 |
| Group change cycle x5 | 677,934–682,576 | 903,522–915,996 | 803,108–803,869 |
| Insert 200 entries | 1,076,984–1,078,452 | 3,336,099–3,355,545 | 818,384–821,745 |
| Selection/autoscroll x50 | 791,255–794,378 | 1,487,836–1,495,704 | 1,682,025–1,683,704 |
| Notification chain x100 | 499,773–500,929 | 873,793–876,913 | 500,045–500,669 |
| Text update x100 | 127,284–127,746 | 153,332–154,092 | 125,725–126,266 |

### Actual geometry

The size requests are constrained to the class tree's natural limits;
MUIA_FixWidth/Height do not override those limits. Each toolkit also adds
its default inner spacing. All measured roots fit within their windows.
The raw `list-visible` metadata is queried while the list is empty, before
insertion; it is not a normalized count of visible populated rows.

| Toolkit | Window | Root |
| --- | --- | --- |
| MUI 3.8 | 238x419 | 230x406 |
| MUI 5 | 244x429 | 236x416 |
| Zune68 | 232x421 | 224x408 |

This geometry/style difference matters for drawing-dependent workloads.
Do not interpret these results as isolated rendering speed on identical
pixels, or apply the ratios to entire applications and other screen modes.

Raw run medians, input manifests and provenance are in
`results/toolkit-perf-20261014-68020.json` and
`results/toolkit-perf-20261014-provenance.json`. All nine guest outputs
are retained in `results/toolkit-perf-20261014-transcripts.txt`.

The separate [Zune68 before/after report](PERFORMANCE.mui5.md) measures
the cost of adding our MUI 5 compatibility work. Its smaller screen and
different geometry make its absolute times a separate data set.

## Reproduce

Build tests/native/perfbench.c with the flags above and MUI SDK headers.
Use a disposable fixture whose run.uae mounts fixture/guest as Bench: and
fixture/system.hdf as its OS disk. Place a clean OS copy at
fixture/pristine.hdf. Its startup must execute Bench:run-tests.
Install the complete toolkits into guest/mui38, guest/mui5 and guest/zune68,
each with Libs/muimaster.library. No application may preload a different
muimaster.library before the benchmark.

```sh
python3 tests/native/compare_toolkits.py \
  --fixture /path/to/disposable-fixture \
  --emulator /path/to/winuae \
  --binary /path/to/perfbench \
  --profiles mui38 mui5 zune68 --trials 3 \
  --output /path/to/results
```

The runner locks its fixture, checks that toolkit files remain unchanged,
records hashes and validates all eight result rows. It restores the guest
startup and benchmark files. The OS disk is disposable and deliberately
replaced from pristine.hdf before each run.
