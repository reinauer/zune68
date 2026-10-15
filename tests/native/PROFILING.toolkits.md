# Creation and List profiling against MUI 3.8

These measurements capture the baseline before the creation/List fixes.
See [PERFORMANCE.optimizations.md](PERFORMANCE.optimizations.md) for the
implemented follow-up and its measured results.

This follows the larger gaps in [PERFORMANCE.toolkits.md](PERFORMANCE.toolkits.md).
The library binaries are unchanged: Zune68 c70d226a (development version
21.227) and original MUI 3.8 19.35, with the same frozen installations,
68020 WinUAE configuration and pristine AmigaOS 3.2.3 disk.

## Measurement boundaries

`perfprofile.c` includes the widget-tree builder from `perfbench.c`.
It separates creation from disposal and tests three List workloads:

- `active-scroll50`: 50 sets with `(j * 7) % 200`, including scrolling.
- `active-visible50`: 50 sets alternating entries 0 and 1, both visible.
- `active-closed50`: the same alternation with the window closed.

Each list batch starts at active entry 0 and first entry 0. There are 49
actual active-entry changes per batch. Unlike the original full benchmark,
these runs populate the list before opening the window and reset its
position before every repetition. Their absolute times and percentages
are diagnostic measurements, not replacements for the original report.

The default binary has two modes:

- `plain`: no dispatcher changes; 15 repetitions per phase per boot.
- `trace`: temporary wrappers around 23 available built-in class
  dispatchers, recording method counts and inclusive/exclusive EClock
  time for one repetition per phase. Wrappers are restored before exit.

Four extra application lifetimes warm filesystem/class activity before
measurement. The earlier pilot's first creation trace was a large outlier
and is excluded. Three completed fresh boots per toolkit and mode are
retained. An emulator exit before the third Zune trace completed produced
no usable sample; that boot was repeated from the pristine disk.

Instrumentation adds substantial overhead, especially to short methods.
Use plain runs for speed comparisons and trace runs for call counts and
hot-path localization. Inclusive times overlap through recursion and
must not be summed. Exclusive time subtracts wrapped child execution;
it still contains unwrapped callees, OS calls and residual instrumentation
cost. Original MUI may call internal helpers directly where Zune dispatches
through a superclass, so class-call counts are not an object-count census.
The wrappers preserve each original dispatcher and its incoming A6.

The fixtures contain no disabled controls or custom subclasses. Changing
dispatcher identity can affect optimized class-identity tests, so these
wrappers are unsuitable for general compatibility or layout timing claims.
The traced list dimensions match the plain runs: Zune 106x270 with 30
visible rows; MUI 3.8 92x285 with 31. Default drawing areas differ, as in
the original comparison. These are emulator results, not hardware tests.

## Plain timing results

<!-- TIMINGS -->
Median of three boot medians; milliseconds per batch. The range
columns show the minimum and maximum boot medians.

| Phase | MUI 3.8 ms (range) | Zune68 ms (range) | Zune time difference |
| --- | ---: | ---: | ---: |
| create | 74.57 (74.30–74.76) | 124.58 (124.49–124.60) | +67.06% |
| dispose | 30.93 (30.92–31.18) | 26.72 (26.69–26.81) | -13.62% |
| active-scroll50 | 1059.65 (1059.58–1059.82) | 2293.23 (2291.07–2302.53) | +116.41% |
| active-visible50 | 223.75 (223.70–223.85) | 365.25 (364.92–366.30) | +63.24% |
| active-closed50 | 11.59 (11.59–11.59) | 82.83 (82.28–83.26) | +614.83% |
<!-- /TIMINGS -->

## What the List trace shows

The following counts were identical in all three trace boots:

| Workload / dispatch | MUI 3.8 | Zune68 |
| --- | ---: | ---: |
| Visible selection: List Draw | 49 | 98 |
| Visible selection: Area OM_SET | 0 | 500 |
| Visible selection: Prop OM_SET | 0 | 100 |
| Closed selection: Area OM_SET | 0 | 500 |
| Closed selection: Prop OM_SET | 0 | 100 |
| Selection/autoscroll: List Draw | 90 | 57 |
| Selection/autoscroll: Area OM_SET | 41 | 1607 |
| Selection/autoscroll: Prop OM_SET | 41 | 346 |

Fewer Draw calls do not mean less drawing. Zune invokes List Display
1,246 times in the scrolling batch, versus 98 in the visible-only batch.
The MUI 3.8 implementation does not expose the newer List Display method
on this path; its row-render count is therefore not established by these
wrappers.

The source explains the observed paths:

1. `classes/list.c`, `List__OM_SET`, `MUIA_List_Active`: a visible selection
   change redraws the old entry and then redraws the new entry separately.
   This accounts for the 98 versus 49 Draw calls.
2. The same method's offscreen-active branch sets `UPDATEMODE_ALL` and
   redraws the visible list. Small forward moves therefore repaint rows
   that remain visible. `List__MUIM_Draw` already has a separate raster
   scrolling path for `MUIA_List_First`; active-entry changes bypass it.
3. `List__OM_SET` hands its tag list to Group. `Group__OM_SET` forwards
   tags to children by default. These list-specific updates traverse
   scrollbar, prop, image and area objects even with the window closed.
4. Each `List_DrawEntry` creates a ZText, measures it, draws it and destroys
   it. The 1,246 Display calls therefore also identify substantial repeated
   text preparation in this simple one-column workload. Caching display
   strings from arbitrary client hooks would require a separate contract.

The first optimization targets are therefore batching old/new selection
damage, avoiding irrelevant forwarding to internal list children, and
reusing unchanged raster rows during short scrolls. Redraw and forwarding
must be measured separately: neither alone explains the entire gap.

## Creation profile

<!-- CREATION -->
One fresh boot per toolkit, 15 repetitions, with only Application
and Configdata new/dispose wrapped. Times below are instrumented.

| Creation scope | MUI 3.8 ms | Zune68 ms |
| --- | ---: | ---: |
| whole tree | 85.28 | 132.90 |
| Application.mui | 46.09 | 75.32 |
| Configdata.mui | 0.33 | 64.55 |

Application includes Configdata; do not add these rows. MUI 3.8
initializes preferences later in its Application path, so its small
Configdata constructor is not an equivalent measure of preference work.

The Application constructor explains about 29 ms of the roughly 48 ms
whole-tree difference in this focused run. Zune's Configdata constructor
takes about 65 ms inside that path. The remaining difference includes
window/widget construction and library dispatch outside Application. The full
trace also identifies Notify, Area and Group construction as contributors.

Even this narrower instrumentation raises whole-tree creation time.
These figures localize work; the plain table remains the speed result.
<!-- /CREATION -->

The full method trace observes 160 Dataspace Find calls during Zune
creation, each going through ObtainShared and Release. Four Configdata
Load attempts probe application/global preferences. Configdata construction
also walks string, image, frame and numeric defaults, including repeated
default-table searches and hotkey parsing.

Candidate work is to initialize defaults in a single pass and overlay
stored settings without repeatedly resolving the same defaults. Preserve
application/global preference precedence, reload notifications, borrowed
string lifetimes, subclasses and synchronization. The profile does not
justify deleting preference loading or removing locks indiscriminately.

## Controlled forwarding experiment

<!-- FORWARDING -->
A separate test binary sets `MUIA_Group_Forward=FALSE` alongside each
timed `MUIA_List_Active` tag, using the unchanged Zune library. It verifies
the final active entry after each workload. Three fresh boots each take
15 repetitions. This is a diagnostic API experiment, not a production fix.

| Batch | Ordinary Zune ms | Forwarding disabled ms | Change |
| --- | ---: | ---: | ---: |
| active-scroll50 | 2293.23 | 2231.92 | -2.67% |
| active-visible50 | 365.25 | 299.32 | -18.05% |
| active-closed50 | 82.83 | 15.83 | -80.89% |

This only suppresses forwarding for the caller's active-entry tag;
internally generated updates and rendering still run. It does not establish
safe filtering for arbitrary mixed tag lists or application subclasses.

The large scrolling gap is mainly rendering work: suppressing this tag's
forwarding saves only about 3% of the scrolling batch. Reusing rows and
reducing repeated text preparation have higher potential for that workload.
<!-- /FORWARDING -->

## Optimization order and acceptance checks

1. Coalesce visible selection damage into one draw. Check active-off,
   multiselect, read-only, quiet mode and notification order.
2. Filter consumed list-specific tags from internal child forwarding.
   Keep inherited Area/Group attributes and external subclass behavior.
3. Scroll existing rows for short active-entry jumps and repaint exposed
   rows plus the old/new cursor. Compare pixels for partial final rows,
   titles, stripes, picture backgrounds, covered windows, refresh damage,
   RTG and full-range jumps. Do not remove the full-redraw fallback.
4. Reduce repeated Configdata initialization, then remeasure the complete
   tree and isolated object construction. Preserve preference reload and
   application lifetime behavior.

After production changes, rerun the original uninstrumented eight-phase
benchmark on 68000 and 68020 as well as the affected compatibility probes.
Keep these original-MUI gaps separate from the much smaller cost of adding
MUI5 support to Zune, measured in `PERFORMANCE.mui5.md`.

## Reproduction

Use the disposable fixture described in `PERFORMANCE.toolkits.md`.
Never install the dispatcher profiler in a normal Amiga setup or run
another MUI client alongside it.

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os -std=gnu17 \
  -Wall -Wno-pointer-sign -I/path/to/SDK/MUI/C/include \
  -Wl,-u,___stkinit tests/native/perfprofile.c -o /tmp/perfprofile -lamiga

python3 tests/native/profile_toolkits.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --binary /tmp/perfprofile --output /tmp/profile-results \
  --profiles zune68 mui38 --trials 3
```

Build a second binary with `-DPROFILE_FOCUS=1` and run it with
`--modes trace` to wrap only Application/Configdata new/dispose. It takes
15 creation/disposal repetitions and emits individual constructor traces.
Build with `-DPROFILE_NOFORWARD=1` for the tag-forwarding experiment.
These options are test-only and do not modify the library binaries.

The runner resets the OS disk before each boot, locks the fixture, records
binary/config/emulator/disk/toolkit hashes, verifies toolkit files have
not changed, and restores the launch script and probe binary on exit.

<!-- EVIDENCE -->
Raw transcripts: `results/toolkit-profile-20261014-transcripts.txt`.
Parsed timings and input hashes: `results/toolkit-profile-20261014.json`.
The library hashes match the original toolkit comparison. The default
probe rebuilt from this source was byte-identical to the measured binary.
<!-- /EVIDENCE -->
