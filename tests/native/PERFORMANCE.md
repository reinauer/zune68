# Performance

Zune68 against original MUI on the perfbench workload. Milliseconds per
batch, lower is better; the percentages are Zune68's time against each MUI.

| Batch | MUI 3.8 | MUI 5 | Zune68 | vs 3.8 | vs 5 |
|---|---:|---:|---:|---:|---:|
| Create/dispose widget tree | 105.53 | 253.28 | 88.23 | -16.4% | -65.2% |
| Open/close window | 539.90 | 883.09 | 539.96 | 0.0% | -38.9% |
| Full redraw x10 | 1572.25 | 2150.47 | 1549.63 | -1.4% | -27.9% |
| Group change cycle x5 | 957.94 | 1286.92 | 932.98 | -2.6% | -27.5% |
| Insert 200 entries | 1538.56 | 4721.42 | 994.65 | -35.4% | -78.9% |
| Selection/autoscroll x50 | 1126.56 | 2109.61 | 1045.61 | -7.2% | -50.4% |
| Notification chain x100 | 699.52 | 1229.53 | 672.05 | -3.9% | -45.3% |
| Text update x100 | 180.34 | 217.22 | 179.44 | -0.5% | -17.4% |

Zune68 beats MUI 5 in every batch. It is level with MUI 3.8 on open/close
and text update and faster in the other six. Differences under 1% count as
unchanged; boot-to-boot spread is under 0.8%, except 2.6% for Zune68
create/dispose.

Measured at `2aed03dc`. Not covered:

- `042ddb6d` (titlebar gadgets), which changed `window.c` afterwards.
- 68000: no timing of the current library. MUI 5 needs a 68020 anyway.
- RTG, physical hardware and complete applications.

MUI 3.9 is slower than MUI 3.8 in every batch (2-35%, one boot on the older
screen setup), so MUI 3.8 is the speed reference. GCC 16.2 brings no clear
gain over GCC 13.4: every batch is within 1%, except create/dispose at
-1.5% with overlapping boot ranges, and the loaded library is 2348 bytes
larger (388220 against 385872).

## What is measured

`perfbench screen-fixed` runs one executable, built with GCC 13.4, on all
toolkits. It opens a 640x480 depth-2 screen in HIRESLACE_KEY and rejects
screens or windows outside the visible area.

- The tree uses only classes and APIs that MUI 3.8 has. Creation starts
  with a warm class cache; library loading is not timed.
- Group change brackets unchanged children with InitChange/ExitChange. It
  is neither a structural change nor a window resize.
- Selection/autoscroll steps through 200 entries in jumps of seven.
- Notification and text update run with the window open, drawing included.
- Each toolkit uses its defaults, so window sizes differ (MUI 3.8 238x419,
  MUI 5 244x429, Zune68 232x421) and redraw does not compare identical
  pixels.

Toolkits: MUI 3.8 19.35 (`mui38usr.lha`), MUI 5 21.227
(`MUI-5.0-20210831-os3.lha`), and Zune68 built with `MUI5_TEST=1 TRACE=0
LTO=0`, which reports the same 21.227.

Machine: WinUAE, 68020, AGA, AmigaOS 3.2.3, 2 MiB Chip, 4 MiB Fast, 16 MiB
Z3. JIT off, `cpu_speed=real`, `cpu_multiplier=4`, memory and blitter cycle
accuracy on, CPU cycle accuracy off.

Each figure is the median of three fresh boots, each the median of 15
repetitions after a 10 s settle. Boot order rotates, the OS disk is restored
before every boot, and only one emulator runs.

## What costs time

Kickstart 3.2.3 fills rectangles with the CPU on this machine. A word-store
loop in graphics.library is the hottest code in every drawing phase, at
about a third of all samples for both toolkits, so overdraw is the first
cost to look at.

For short lines the per-call work (layer lock, clipping, fill setup)
outweighs the pixels, so primitive calls matter more than line length.
Microseconds per call on the depth-2 screen:

| Primitive | RectFill | Move/Draw |
|---|---:|---:|
| Horizontal line, 50 px | 219 | 311 |
| Vertical line, 20 px | 314 | 305 |
| Vertical line, 50 px | 491 | 303 |

A single pixel costs 75 with WritePixel and 199 with RectFill, SetAPen with
the same pen 13, and a 101x61 RectFill 2500.

Two small known costs remain, and neither is worth changing:

- Up to six bevel corner WritePixels per redraw, where Area's fill does not
  cover the corner (titled frames, DRAWALL redraws).
- One rectangular clip per list activation, where MUI 3.8 truncates the
  row text instead.

Do not retry these:

- Dropping the bevel corner pixels. MUI 3.9 paints them even on a white
  parent and under a pressed button (`cornerbg.c`).
- Replacing Move/Draw frame lines with RectFill or blits. See the table;
  a blit of a prepared 1-pixel bitmap is slower still.

## Fast paths

Check the fallback or condition when changing one of these paths.

| Fast path | Fallback or condition | Check |
|---|---|---|
| List scroll copies rows (`List_CanScroll`) | Full repaint for a non-solid background, custom backfill, disabled list, virtual group, object cells, buffered drawing, or a covered or damaged layer | `list_repaint_probe.c` |
| Plain list cells borrow the display string | Formatted cells use the parser | `check_textengine.py` |
| Plain text skips the escape parser (`plain_length`) | Escapes or more than one line use the parser | `check_textengine.py` |
| List-only tags are not forwarded to children (`List_SuperSet`) | Any other tag in the list, including `MUIA_Group_Forward`, forwards as usual | `list_repaint_probe.c` |
| Background fills skip the dispatch chain (`zune_draw_background`) | A custom class down to Area, or a virtual group, dispatches | `pixdump.c` |
| Default key bindings parse once (`Configdata__SetKey`) | Saved bindings parse per object | `config_init_probe.c` |
| File notifications start at the first input | A file written before that is caught by its date | `config_init_probe.c` |
| Cached built-in classes are found under Forbid | A cache miss takes the semaphore | `check_class_lifetime.py` |
| Scrollbar updates keep `ICA_TARGET` set | propgclass must not notify for attributes a program sets | `propnotify.c`, `listscrollmsgs.c` |

`make -C workbench/libs/muimaster check-core` runs the `check_*.py` tests
but only compiles the `.c` probes. Run those in a guest with `run_probe.py`.
For `pixdump.c`, compare the output of two library builds.

## Running the benchmark

The fixture is disposable: `run.uae` mounts `guest/` as Bench: and uses
`system.hdf`, `pristine.hdf` is a clean OS copy whose startup executes
`Bench:run-tests`, and each toolkit lives in `guest/<profile>/` with its
`Libs/muimaster.library`.

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os \
  -I<SDK>/MUI/C/include -Wl,-u,___stkinit \
  tests/native/perfbench.c -o perfbench -lamiga

python3 tests/native/compare_toolkits.py \
  --fixture <fixture> --emulator <winuae> --binary perfbench \
  --profiles mui38 mui5 zune68 --trials 3 --settle-seconds 10 \
  --output <results>
```

- `--settle-seconds` defaults to 0, and without a settle period Zune68's
  creation times are unstable.
- With `-Os`, the SDK's inline `MUI_NewObject` reads stack garbage as tags,
  and `-fno-inline` does not fix that. Use perfbench's `va_start` wrapper.
- A 68000 fixture needs `cpu_cycle_exact=true` and `cycle_exact=true`, since
  `cycle_exact=memory` overrides the CPU flag. Confirm that the emulator log
  says `prefetch and cycle-exact`.

To compare two Zune68 builds, use `compare_perf.py` with `--baseline` and
`--candidate` instead of `--profiles`. Each is a directory with both library
names, and the fixture needs `guest/package/Libs`. It runs `perfbench screen`
on a 640x256 screen, so its times do not mix with the table above. Build with
`-DPERFBENCH_REPS=5` for shorter 68000 runs.

## Profiling

| Tool | Reports | Overhead |
|---|---|---:|
| `census_toolkits.py`, `perfcensus.c`; read with `census_report.py`, `census_callers.mjs` | Calls, exclusive time and callers of 646 OS functions per phase | 12 us per call (`count`), 49 us (`time`) |
| `sample_pcs.mjs`, `perfcensus loop-<phase>`; read with `symbolize_samples.mjs` | PC samples over gdbserver, port 2355 | none in the guest |
| `profile_toolkits.py`, `perfprofile.c` | Method counts and times per built-in class | large on short methods |
| `gfxprims.c` | Cost of single graphics primitives | none |

- Instrumented times locate work. Quote speed only from perfbench.
- `perfcensus.c`, `perfprofile.c` and `gfxprims.c` do not select a display
  mode yet, so their window is partly off screen in a nominal 320x256 mode.
- `perfcensus` and `perfprofile` patch the running system. Use them only in
  a disposable guest with no other MUI client.
- `perfprofile.c` has `-DPROFILE_FOCUS=1` to wrap only Application and
  Configdata new/dispose.
- `gen_census_lvos.py` generates `perfcensus_lvos.h`.

## Results

[Measurements and hashes](results/toolkit-compilers-20261016.json),
[guest transcripts](results/toolkit-compilers-20261016-transcripts.txt) and
[build provenance](results/toolkit-compilers-20261016-provenance.json).
