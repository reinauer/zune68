# Creation and List optimization results

This follows the findings in [PROFILING.toolkits.md](PROFILING.toolkits.md).
The baseline is the unchanged library measured in that report, from the
`mui5-dev` tree at c70d226a (last library change 58a17519). The candidate
retains the MUI 5 APIs and the opt-in 21.227 version gate. The implementation
commits are aa79fd0a (Configdata) and d108c3ba (List/text).

## Changes

- Visible selection marks both cursor rows and paints them in one pass.
  Pending full redraws from quiet mode are retained.
- List-only attribute batches stop at the List's inherited Area/Group
  instead of being forwarded to its internal children. Mixed inherited
  tags, explicit Group_Forward requests and notifications retain their
  original route. External subclasses receive the original input.
- Short active-entry scrolls copy unchanged raster rows and repaint the
  exposed rows, old/new cursor and selected rows. A partially visible
  final row is repainted when scrolling it into full view. Picture and
  pattern backgrounds, custom backfills, disabled lists, virtual groups,
  object cells, buffering and damaged/covered layers use full repainting.
  The same safeguards apply to direct List_First changes.
- Plain List cells borrow their display string for synchronous measurement
  and drawing, using stack storage for ZText nodes. Formatted cells retain
  the parser/allocator. No display-hook string is cached across calls.
  TextLength before drawing remains intact for the AfA/AmiKit workaround.
- Configdata initialization walks each defaults table once. Numeric
  defaults carry their target field offset and width, so saved values and
  defaults follow one assignment path. The builtin constructor holds one
  shared Dataspace lock for its initial reads. Normal public lookup and
  subclass dispatch remain in place after construction. File loading,
  change notification and keymap-dependent ParseIX calls are unchanged.

## Timing results

<!-- RESULTS -->
### 68020

| Batch | Before ms | After ms | Time change | After vs MUI 3.8 |
|---|---:|---:|---:|---:|
| create+dispose | 152.59 | 130.03 | -14.78% | +23.19% |
| open+close | 580.82 | 580.88 | +0.01% | +7.40% |
| redraw x10 | 1784.43 | 1783.41 | -0.06% | +13.76% |
| relayout x5 | 1132.10 | 1117.18 | -1.32% | +16.90% |
| list insert200 | 1155.16 | 1023.90 | -11.36% | -32.55% |
| list active x50 | 2372.96 | 1134.69 | -52.18% | +1.70% |
| notify x100 | 704.59 | 703.80 | -0.11% | -0.12% |
| text set x100 | 178.11 | 177.63 | -0.27% | -1.18% |

Negative percentages mean less elapsed time.

### 68000

| Batch | Before ms | After ms | Time change |
|---|---:|---:|---:|
| create+dispose | 459.99 | 381.10 | -17.15% |
| open+close | 901.27 | 889.92 | -1.26% |
| redraw x10 | 2829.26 | 2828.10 | -0.04% |
| relayout x5 | 2112.13 | 2081.49 | -1.45% |
| list insert200 | 2791.67 | 2329.45 | -16.56% |
| list active x50 | 5583.22 | 2448.83 | -56.14% |
| notify x100 | 1237.62 | 1236.41 | -0.10% |
| text set x100 | 372.50 | 372.52 | +0.01% |

Negative percentages mean less elapsed time.
<!-- /RESULTS -->

Treat changes below 1% as approximately unchanged; these are small matched
emulator samples, not confidence intervals for hardware performance.

The original eight-phase `perfbench-equal` executable is used unchanged.
Each result is the median of three fresh-boot medians; each boot contains
15 measurements per phase. Toolkit order rotates between boots. The
baseline and candidate use separate installations with identical resources.
Both run without tracing, JIT or dispatcher instrumentation. The runners
restore a pristine OS disk before each boot, serialize access and record
all binary, configuration, disk and toolkit hashes.

The 68020 configuration is the original OS 3.2.3/AGA fixture from
[PERFORMANCE.toolkits.md](PERFORMANCE.toolkits.md). The 68000 comparison
uses the A600 OS 3.2.3 ROM, ECS, 24-bit addressing and no Z3 RAM,
with the same OS disk and benchmark. Both CPU and memory cycle accuracy
are enabled for the final 68000 comparison; the emulator log confirms
`prefetch and cycle-exact`. The original 68020 configuration is unchanged.
These are matched emulator measurements, not physical hardware results.
Toolkit defaults can produce different geometry; every run records it.
The small cost of adding MUI 5 support to Zune68 is a separate comparison
in [PERFORMANCE.mui5.md](PERFORMANCE.mui5.md).

## 68000 timing investigation

The initial 68000 configuration enabled memory cycle accuracy but disabled
CPU cycle accuracy (`cycle_exact=memory`). All three complete runs in that
mode reported a large creation/disposal slowdown for the candidate, despite
improvements in List selection and insertion. Those results are retained in
[the memory-only measurements](results/toolkit-optimization-20261015-68000-memory-only.json)
and [diagnostic transcripts](results/toolkit-optimization-20261015-diagnostics.txt).

The benchmark rebuilt byte-for-byte identically. Debugger checks found the
library code in Fast RAM and the saved start timestamp preserved across
creation/disposal. A diagnostic run had two slow creation samples followed
by thirteen near 0.83 seconds. A copy of the original executable that skips
later phases, preserving the creation loop and Hunk allocation sizes, also
alternated between fast and slow candidate results. Focused candidate
runs were fast, including combined creation/disposal with only one
warmup. The discrepancy was therefore not explained by splitting the
timer interval or by extra warmups.

With full CPU cycle accuracy, both preliminary creation-only comparisons
improved. The complete before/after comparison above uses that verified
mode with the original, unchanged benchmark. Absolute times from the two
68000 configurations must not be combined.

A subsequent check kept the original memory-only timing mode and added
`Wait 10` before launching the same shortened executable. Both fresh-boot
pairs then improved: 656927 to 558679 ticks (-14.96%) and 688512 to 584082
ticks (-15.17%). Thus the earlier result is sensitive to startup conditions;
the particular background activity or scheduling effect has not been
isolated. This does not establish a WinUAE defect. The full tables above
use the cycle-accurate runs without an added startup delay.

`compare_toolkits.py --settle-seconds 10` exposes the settling control and
records it in new manifests. Its default is zero, preserving the original
launch sequence. The settled diagnostic transcripts and their input hashes
are retained alongside the earlier measurements.

## Call counts

<!-- COUNTS -->
| Phase / dispatch | Before | After |
|---|---:|---:|
| Creation: public Dataspace Find | 160 | 1 |
| Creation: Semaphore ObtainShared / Release | 160 / 160 | 2 / 2 |
| Visible selection: List Draw | 98 | 49 |
| Visible selection: Area Set / Prop Set | 500 / 100 | 50 / 0 |
| Closed selection: Area Set / Prop Set | 500 / 100 | 50 / 0 |
| Selection/autoscroll: List Draw | 57 | 49 |
| Selection/autoscroll: List Display | 1246 | 366 |
| Selection/autoscroll: Area Set / Prop Set | 1607 / 346 | 419 / 82 |

The final trace has no overflow or underflow. Initial Dataspace reads still
happen: the builtin constructor performs them directly under one shared
lock, while the subsequent window-position lookup retains normal dispatch.
The table counts dispatched methods, not all underlying memory lookups.
<!-- /COUNTS -->

Dispatcher traces establish which work disappeared; their timings are
not used for speed claims. The profiler populates the List before opening
and resets it between batches, unlike the eight-phase benchmark. Do not
substitute its absolute timings for that benchmark.

## Correctness and memory

`list_repaint_probe.c` compares the pixels after an incremental change
with a complete repaint of the identical state. It covers plain and bold
cells, partial final rows, titles, stripes, a generated ILBM picture,
pattern backgrounds, custom backfill, a virtual group, multiselection,
quiet mode, active-off, read-only mode, disabled state, covered windows,
full-range jumps, close/reopen, NoNotify and explicit forwarding. A custom
List subclass checks one Active notification per change and that First
notifications precede it. The test also exercises mixed inherited tags.

`config_init_probe.c` checks real application construction, saved word and
longword settings, false boolean overrides, neighboring defaults, custom
frames, parsed keyboard bindings, filesystem-triggered preference reload,
disposal/recreation and removing saved settings. A Configdata subclass
checks inherited defaults, semaphore dispatch and string updates while
protecting its own instance data with a canary. It uses an exclusively
owned fixture and an application-specific temporary preference file.

The linked 68000 `check_textengine.py` checks that borrowed plain text
matches the allocated parser representation without allocating or taking
ownership of the source buffer; formatted strings take the fallback.
`make check-core` also compiles both new native probes.

<!-- VALIDATION -->
- `make check-core`: passed for both library names on linked 68000 code.
- Pixel equivalence: 189 comparisons each on 68000 planar, 68020 planar
  and 68020 P96 8-bit mode 0x50031000; all 567 passed.
- Configdata defaults, saved overrides, filesystem notification reload
  and subclass isolation: passed in both 68000 and 68020 OS guests.
- Existing modern List editing/sorting/title-click and column/object-cell
  lifetime probes: passed on 68020.

These checks do not establish behavior on physical hardware or AfA OS.
<!-- /VALIDATION -->

The candidate's loaded CODE segment grows by 856 bytes (376872 to 377728,
about 0.23%); DATA and BSS remain 1260 and 1300 bytes. Configdata adds one
four-byte constructor-only pointer per application. List gains no instance
or per-entry storage. Plain-cell rendering trades a transient heap
allocation for 100 bytes of stack nodes; formatted cells keep their existing
allocation path. These are linked segment/structure sizes, not a whole-OS
peak-RAM measurement.

## Reproduction

Build with `MUI5_TEST=1 TRACE=0` and install the two libraries in a copy of
the baseline toolkit profile. Preserve the baseline installation.

```sh
python3 tests/native/run_probe.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --binary /path/to/list-repaint-probe --profile candidate \
  --output /tmp/list-pixels --screen

python3 tests/native/compare_toolkits.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --binary /path/to/perfbench-equal --profiles zune68 candidate \
  --output /tmp/perf-after --trials 3

python3 tests/native/profile_toolkits.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --binary /path/to/perfprofile --profiles candidate --modes trace \
  --output /tmp/profile-after --trials 1
```

For the 68000 timing fixture, set both `cpu_cycle_exact=true` and
`cycle_exact=true`, retaining memory/blitter cycle accuracy. A later
`cycle_exact=memory` line overrides the individual CPU flag. Confirm the
emulator logs `CPU=68000 ... prefetch and cycle-exact` before interpreting
the result. The complete recorded machine settings are in the provenance.

Use `--rtg` for the P96 640x480, depth-8 test screen (mode 0x50031000);
the probe rejects an unavailable mode. Run `config-init-probe`,
`mui5-list-probe` and `mui5-list_format-probe` with the same probe runner.
The config probe needs the repository's private headers; its make target
provides them. Keep all test emulators and CPU-intensive builds stopped
during timing runs.

## Evidence

- [68020 measurements](results/toolkit-optimization-20261015-68020.json)
  and [68000 measurements](results/toolkit-optimization-20261015-68000.json)
- [Guest output](results/toolkit-optimization-20261015-transcripts.txt)
  and [build/configuration provenance](results/toolkit-optimization-20261015-provenance.json)
- [Validation output](results/toolkit-optimization-20261015-validation.txt)
  and [validation inputs](results/toolkit-optimization-20261015-validation.json)
- [Dispatcher trace](results/toolkit-optimization-20261015-trace.txt)
  and [trace inputs](results/toolkit-optimization-20261015-trace.json)
- [Diagnostic inputs](results/toolkit-optimization-20261015-diagnostics.json)

The creation-only profiler modes are diagnostic: `create` uses five warmups
and separate timers; `create-cold` uses one warmup; `create-combined` uses
one warmup and a combined interval; `create-combined-warm` uses five.
`create-trace` adds the dispatcher wrappers. Use `-DPROFILE_FOCUS=1` when
only Application/Configdata constructor and disposal traces are wanted.
