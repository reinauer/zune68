# Creation, window, redraw and relayout profiling against MUI 3.8

This follows [PERFORMANCE.optimizations.md](PERFORMANCE.optimizations.md),
where Zune68 (mui5-dev at 1eec8ac0, library identical to the measured
`candidate` build) still takes more time than original MUI 3.8 for four
of the eight perfbench batches on the 68020 fixture:

| Batch | MUI 3.8 | Zune68 | Zune68 vs 3.8 |
| --- | ---: | ---: | ---: |
| Create/dispose widget tree | 105.55 ms | 130.03 ms | +23.2% |
| Open/close window | 540.84 ms | 580.88 ms | +7.4% |
| Full redraw x10 | 1567.69 ms | 1783.41 ms | +13.8% |
| Group change cycle x5 | 955.70 ms | 1117.18 ms | +16.9% |

The question here is where those milliseconds go, measured the same way
on both toolkits, and which Zune68 code paths are responsible.

## Method

Three instruments, all in `tests/native/perfcensus.c`, which reuses the
perfbench widget tree, the 640x480 depth-2 screen and the fixed geometry:

- **OS-call census** (`count` and `time` modes). After warming the class
  cache, 646 functions of exec, graphics, layers, intuition, utility, dos,
  diskfont, keymap and commodities are wrapped with `SetFunction`
  (interrupt-level, scheduler and stack-switching functions excluded).
  `count` counts calls per function and records the return addresses of
  the callers of about 40 hot functions. `time` keeps a shadow stack per
  call and attributes inclusive and exclusive time per OS function. The
  time stamps come from a VBlank counter plus the beam position (one unit
  is 1/256 raster line, about 0.25 us; the `calibration` line relates
  them to EClock ticks) because `ReadEClock` would cost more than most of
  the measured calls. All patches are removed before exit.
- **Dispatcher trace** (`trace` mode): the built-in class dispatchers are
  wrapped as in `perfprofile.c`, giving per-class method counts and
  inclusive/exclusive times.
- **PC sampling** (`loop-<phase>` mode): the probe repeats one phase for
  minutes while a GDB client (`sample_pcs.mjs`, symbols via `symbolize_samples.mjs`) pauses the emulator every
  15 ms, reads the PC and the top of the stack, and continues. Samples
  are mapped to Zune68 functions with `nm` of the unstripped build and
  to ROM library vectors (exec/graphics/... LVO names); ROM samples are
  also attributed to the nearest Zune68 return address on the stack.

Phases: `create` (build the tree), `dispose`, `open+close` (the second
open/close after one warm-up), `redraw10` (ten `MUI_Redraw`), `relayout5`
(five InitChange/ExitChange). The `plain` mode times them uninstrumented
(median of 15). Both toolkits use the same binary, fixture, pristine OS
disk and emulator; one emulator runs at a time.

Instrumentation cost is substantial and must be subtracted before quoting
absolute costs: about 12 us per wrapped call in `count` mode and about
49 us in `time` mode on this fixture (derived from the phase totals of
the plain, count and time runs). A function whose implementation calls
other wrapped functions (FindTagItem calls NextTagItem, RectFill calls
BltPattern) carries their wrapper cost in its exclusive time. Leaf
functions (Draw, WritePixel, SetAPen, GetAttr) are attributed directly.

`census_toolkits.py` runs the modes per toolkit profile;
`census_report.py A.txt B.txt --sort excl|calls` compares two transcripts.

## Uninstrumented phase times on this fixture

Median of 15, `perfcensus plain`, one boot each:

| Phase | MUI 3.8 ms | Zune68 ms | Zune68 / 3.8 |
| --- | ---: | ---: | ---: |
| create | 74.88 | 100.16 | 1.338 |
| dispose | 31.14 | 26.35 | 0.846 |
| open+close | 540.94 | 561.74 | 1.038 |
| redraw10 | 1563.75 | 1782.65 | 1.140 |
| relayout5 | 954.15 | 1116.35 | 1.170 |

Creation alone is 25 ms slower; disposal is 5 ms faster. The relayout
batch, which is a window-level relayout plus a full redraw, carries the
redraw difference (about 22 ms per redraw) plus 10 ms of its own.

MUI 3.9 (20.2362, the earlier parity reference) is slower than MUI 3.8
on the same fixture and benchmark (one boot, EClock ticks):

| Batch | MUI 3.8 | MUI 3.9 | Zune68 |
| --- | ---: | ---: | ---: |
| create+dispose | 75,622 | 101,856 | 89,985 |
| open+close | 383,633 | 426,388 | 412,073 |
| redraw x10 | 1,109,756 | 1,393,669 | 1,264,388 |
| relayout x5 | 681,325 | 817,722 | 803,157 |
| list insert200 | 1,076,568 | 1,442,214 | 725,500 |
| list active x50 | 791,061 | 957,019 | 803,103 |
| notify x100 | 495,703 | 519,071 | 499,718 |
| text set x100 | 128,204 | 130,697 | 126,677 |

So the earlier "at parity with MUI 3.9" results were real, but MUI 3.9
is 20-35% slower than 3.8 in exactly these batches. Beating 3.8 needs
further cuts, not a fix of a regression.

## OS-call counts per phase

Calls per phase (count mode, identical across boots), Zune68 / MUI 3.8,
for the functions whose counts differ most. Nested ROM-internal calls are
included (RectFill calls BltPattern, WaitBlit and TypeOfMem through the
jump table), so the rows also show which ROM paths each toolkit uses.

| Phase | Function | Zune68 | MUI 3.8 | Note |
| --- | --- | ---: | ---: | --- |
| create | NewObjectA | 77 | 58 | Zune builds a Family object per group and more internal objects |
| create | AllocMem + AllocVec | 535 | 43 | MUI 3.8 uses 110 AllocPooled instead |
| create | NextTagItem | 1413 | 704 | |
| create | ParseIX | 24 | 1 | Configdata parses every keyboard binding per application |
| create | Strnicmp | 499 | 0 | inside ParseIX |
| create | MapANSI | 1 | 0 | inside ParseIX (commodities inverts the keymap) |
| create | dos Open | 4 | 1 | ENV:MUI, ENV:Zune, ENVARC:MUI, ENVARC:Zune preference files |
| create | StartNotify | 2 | 0 | application preference file and the public screen file |
| create | GetAttr | 118 | 5 | |
| create | UDivMod32 | 93 | 0 | 32-bit division through utility.library (68000 build) |
| dispose | FreeMem + FreeVec | 519 | 67 | MUI 3.8: 81 FreePooled |
| open+close | GetAttr | 276 | 26 | 149 of them are Family list lookups by Group |
| open+close | StrToLong | 95 | 0 | image, pen and font specs parsed again at every window open |
| open+close | FilePart | 21 | 0 | font names parsed again |
| open+close | AllocVec + AllocMem | 319 | 125 | image spec structures per object per open |
| open+close | UDivMod32 + SMult32 + SDivMod32 | 555 | 61 | utility.library arithmetic |
| open+close | Draw | 114 | 49 | vertical frame lines |
| open+close | WritePixel | 55 | 4 | frame corner pixels |
| open+close | TextLength | 61 | 3 | |
| redraw10 | Draw | 630 | 0 | 63 vertical frame lines per redraw |
| redraw10 | WritePixel | 530 | 0 | 53 corner pixels per redraw |
| redraw10 | Move | 880 | 250 | |
| redraw10 | GetAttr | 360 | 0 | Family list lookups |
| redraw10 | InstallClipRegion | 80 | 40 | Zune's are multi-rectangle regions (NewRegion/OrRectRegion/XorRectRegion) |
| redraw10 | RectFill | 2540 | 2870 | MUI 3.8 fills more rectangles, including every frame line |
| redraw10 | TypeOfMem | 2680 | 1650 | ROM-internal |
| relayout5 | Draw / WritePixel | 315 / 265 | 0 / 0 | |
| relayout5 | GetAttr | 665 | 0 | 123 Family list lookups per relayout |
| relayout5 | NextTagItem | 5015 | 3880 | |
| relayout5 | UDivMod32 + SMult32 + SDivMod32 | 1225 | 15 | |
| relayout5 | SizeWindow | 5 | 0 | WindowResize is called with dx = dy = 0 |
| relayout5 | WindowLimits | 10 | 5 | |
| relayout5 | AllocVec + AllocMem | 45 | 0 | |

MUI 3.8 draws every frame line with RectFill (blitter) and never calls
Draw or WritePixel in these phases; it pays for that with more
RectFill/BltPattern calls. It also spends calls Zune does not need:
AskSoftStyle/SetSoftStyle (780 per ten redraws), SetDrMd (510), TextExtent
(210), TextFit (250), BltMaskBitMapRastPort (70) and AreaDraw/AreaEnd
polygons at window open.

## Time attribution

`time` mode, instrumented milliseconds (see the overhead note above),
Zune68 / MUI 3.8, exclusive time of the functions that differ most.

Redraw x10 (instrumented totals 3123 / 2815 ms):

| Function | Zune68 ms | MUI 3.8 ms | Real estimate per redraw |
| --- | ---: | ---: | --- |
| graphics/Draw | 263 | 0 | 23 ms (630 calls, 0.42 ms each) |
| graphics/WritePixel | 93 | 0 | 7 ms (0.18 ms per pixel) |
| layers/InstallClipRegion | 32 | 3.5 | 3 ms (0.4 ms per multi-rectangle region) |
| intuition/GetAttr | 33 | 0 | 1.6 ms |
| graphics/Move | 15 | 4 | under 1 ms |
| graphics/BltPattern (inside RectFill) | 1155 | 1357 | MUI 3.8 blits about 20 ms more per ten redraws |
| graphics/SetSoftStyle + AskSoftStyle + SetDrMd + TextFit + TextExtent | 8 | 100 | mostly wrapper cost of 2300 extra MUI calls |
| toolkit code outside OS calls | 646 | 450 | Zune's own drawing code and dispatch chains |

Relayout x5 (2145 / 1849 ms): Draw 134 / 0, WritePixel 37 / 0, GetAttr
32 / 0, InstallClipRegion 17 / 1.7, NextTagItem 92 / 65, FindTagItem
151 / 133, SetAPen 50 / 29; toolkit code outside OS calls 564 / 414.

Open+close (1204 / 1149 ms): Draw 44 / 9, GetAttr 13 / 1, NewObjectA
13 / 5, WritePixel 8 / 0.5, AllocVec+FreeVec 17 / 3, UDivMod32 5 / 0;
MUI 3.8 pays AreaEnd 31 and Move 18. The toolkit code inside the
Window_Open SetAttrs call is 194 / 138 ms.

Create (281 / 177 ms): ParseIX 25 / 0 plus Strnicmp 18 / 0 plus MapANSI
3.7 / 0 (all one feature), NextTagItem 42 / 12, AllocMem+AllocVec 25 / 3,
dos Open 9.1 / 1.8, StartNotify 8.8 / 0, GetAttr 9 / 0.3, SetAttrsA 6 / 0.3;
NewObjectA exclusive (the constructors themselves) 115 / 76. MUI 3.8 pays
FindTagItem 24 and ObtainSemaphore 7 (its Dataspace lookups).

Dispose (64 / 68 ms): FreeMem+FreeVec 21 / 3; MUI 3.8 writes its
preference file at disposal (dos Open/Write/Seek/NameFromFH/SetProtection,
17 ms), which Zune does not.

## Dispatcher trace

Dispatches per redraw: Zune68 about 470 (Area Draw 49, Area
DrawBackground 129, Group DrawBackground 107, Window DrawBackground 30,
DrawParentBackground 26, Family OM_GET 34, Text Draw 18, Group Draw 17,
...); MUI 3.8 about 90 (Group Draw 18, Text Draw 18, Image Draw 7, Area
Draw 5, ...). MUI 3.8's built-in classes draw frames and backgrounds in
internal functions; Zune dispatches MUIM_DrawBackground from every object
up through its groups to the window (`Area__MUIM_DrawParentBackground`),
three to five BOOPSI dispatches per background fill.

Per relayout: Zune asks MinMax twice per object (Group AskMinMax 36,
Area 110, Text 40 per relayout; MUI 3.8: 18, 6, 18). `Window__MUIM_
RecalcDisplay` asks the originator chain and then `WindowMinMax` asks the
whole tree again. It then hides the subtree, calls `WindowResize`
(WindowLimits, SizeWindow with a zero delta, WindowLimits) and shows it
again. The zero-delta SizeWindow makes Intuition refresh the window frame
and title (PrintIText, RemoveGList/AddGList, RefreshGList, a new system
gadget image) once per relayout. MUI 3.8 does not resize or re-add
gadgets when the size is unchanged (it does hide/show its prop gadgets).

At creation, Zune dispatches OM_ADDMEMBER to Group and Family per child
(54 + 55), ConnectParent to every object (111 dispatches), OM_GET 77 times
on groups and 79 on areas; MUI 3.8's Group OM_NEW adds children without
per-child dispatches. Zune's Configdata OM_NEW takes 25 ms instrumented
(ParseIX, four preference file opens and two notifications) against
0.14 ms for MUI 3.8, whose Application constructor reads its 150 settings
through 300 Dataspace finds (22 + 17 ms instrumented) instead.

<!-- SAMPLING -->
## PC samples

About 3,000 samples per phase and toolkit (`loop-<phase>` with the GDB
sampler, Zune68 `head` build with symbols, MUI 3.8 without). Samples
whose stack holds a timer.device reply frame were set aside: they are
interrupt deliveries caught by the pause and over-represented (18% of
Zune's redraw samples against 2% for MUI 3.8; Zune windows ask for
IDCMP_INTUITICKS, so a 10 Hz timer runs while a Zune window is active).
The time census shows no matching inflation of leaf-call costs, so this
is a sampling artefact, not CPU time.

Kickstart 3.2.3's graphics.library fills rectangles with the CPU here:
the hottest address range in every drawing phase is a word-store loop
(`clr.w (a1); jmp (a3)` / `and.w d4,(a1)`) inside the RectFill/BltPattern
path, not a blitter wait. Filled area is therefore CPU time for both
toolkits, and overdraw is the first-order cost of drawing.

Share of the remaining samples:

| Phase | Where | MUI 3.8 | Zune68 |
| --- | --- | ---: | ---: |
| redraw | graphics CPU fill loop (RectFill) | 42% | 29% |
| redraw | graphics Draw internals | - | 7% |
| redraw | graphics BltPattern setup | 6% | 6% |
| redraw | layer locking (LockLayerRom internals) | 4% | 5% |
| redraw | text rendering (TextFit region) | 4% | 3% |
| redraw | toolkit's own code | about 20% | 21% |
| relayout | graphics CPU fill loop | - | 24% |
| relayout | toolkit's own code | - | 32% |
| open+close | graphics CPU fill loop | 37% | 36% |
| open+close | utility FindTagItem/NextTagItem | 3% | 5% |
| open+close | toolkit's own code | about 25% | 20% |
| create | toolkit's own code | about 50% | 46% |
| create | exec allocation (AllocMem/Allocate internals) | 8% | 11% |
| create | commodities ParseIX + keymap (InvertKeyMap/MapANSI) | - | 7% |
| create | other tasks (RAM handler, ramlib, disk) | 13% | 5% |

MUI 3.8 fills more per redraw (its frame lines are RectFills and it
fills the window background first); Zune's extra drawing time is the Draw
line primitive, layer locking, and its own code. MUI 3.8's creation
spends 13% in the RAM handler and ramlib (23 OpenLibrary calls); Zune
spends 7% parsing hotkeys and 11% allocating.

Zune68 functions by samples (statics inlined into the dispatchers under
-Os appear as the dispatcher):

- redraw: Area dispatcher (Draw/DrawBackground bodies) 4.8%, Group Draw
  2.6%, DrawParentBackground 1.4%, imspec_draw 1.2%, Group dispatcher
  1.1%, imspec_draw_at 0.7%, MUI_Redraw 0.6%, text draw 0.6%,
  get_intframe 0.6%, penspec_drawdirect 0.6%, frame line helpers and
  button_draw 1.0% together.
- relayout: Area dispatcher 5.7%, Group AskMinMax 2.7%, ZuneNextObject
  (child iteration) 2.2%, Group Draw 2.1%, Layout1D_mui_redistrib 1.0%,
  DrawParentBackground 1.0%, get_intframe 1.0%, area_update_msizes 0.7%,
  __area_finish_minmax 0.6%, Family OM_GET 0.4%.
- create: Area dispatcher 5.8%, Group dispatcher 2.4%, builtinDispatcher
  trampoline 2.4%, Notify dispatcher 2.3%, vsnprintf 1.9% (preference
  file and port names), Area OM_NEW 1.7%, ZUNE_GetBuiltinClass 1.2%,
  Text OM_NEW 1.1%, Notify OM_NEW 1.0%, Configdata OM_NEW 1.0%, Group
  OM_GET 0.9%, DisconnectParent 0.8%, cfg_find 0.6%; DoSuperMethodA
  itself 3.3% and DoMethodA 1.2%.
<!-- /SAMPLING -->

<!-- PRIMS -->
## Graphics primitive costs on this guest

`gfxprims.c` (400 calls each, simple-refresh window), microseconds per
call. The first columns are on the benchmark's 640x480 depth-2 screen
(`gfxprims screen`), the last on the 16-colour Workbench screen.

| Primitive | depth 2 | Workbench |
| --- | ---: | ---: |
| horizontal line, 20 px, RectFill | 206 | - |
| horizontal line, 50 px, RectFill | 219 | 270 |
| horizontal line, 50 px, Move/Draw | 311 | 616 |
| vertical line, 20 px, RectFill | 314 | - |
| vertical line, 20 px, Move/Draw | 305 | - |
| vertical line, 50 px, RectFill | 491 | 963 |
| vertical line, 50 px, Move/Draw | 303 | 545 |
| vertical line, 20/50 px, BltBitMapRastPort of a 1-pixel bitmap | 777 / 896 | - |
| horizontal line, 50 px, BltBitMapRastPort | 536 | - |
| single pixel, WritePixel | 75 | 79 |
| single pixel, RectFill | 199 | 355 |
| SetAPen with the same pen | 13 | 12 |
| 101x61 RectFill | 2500 | 3200 |

A line costs 200-300 us whatever its length: the ROM's per-call work
(layer lock, clipping, CPU fill setup) dominates, and the per-pixel part
is small. Zune's Move/Draw for vertical frame lines is therefore not
slower than MUI 3.8's one-pixel-wide RectFill, and a prepared-bitmap
blit is worse. What matters is the number of primitive calls per frame:
Zune issues six per bevel frame (two RectFill, two Draw, two WritePixel),
MUI 3.8 four. The 53 corner WritePixels per redraw are the only
avoidable frame cost (about 4-5 ms per redraw).
<!-- /PRIMS -->

<!-- CALLERS -->
## Call sites of the hot OS functions in Zune68

From the return addresses recorded in count mode (static functions
resolved through the link map; counts per phase):

- Creation: NextTagItem 383 in `Notify__OM_NEW`, 353 in `Area__OM_NEW`,
  150 in `Text__OM_NEW`, 106 in `Group__OM_NEW`; AllocVec 76 in
  `Notify__OM_NEW` (one per object), 52 in `StrDup`, 46 in
  `Notify__MUIM_Notify` (three per notification), 20 in Text's
  `store_contents_len`; the 315 AllocMem are 220 inside those AllocVec
  calls, 77 object instances inside NewObjectA and 4 ports. GetAttr: 54
  in `Group__MUIM_AddObject`, 18 in `Group__MUIM_ConnectParent`, 18 in
  `Group__OM_NEW`. ObtainSemaphore: 77 in `ZUNE_GetBuiltinClass` and 77
  in `MUI_FreeClass` (one pair per object). All 24 ParseIX are
  `Configdata__SetKey`; the four Open calls are `Configdata__MUIM_Load`;
  the two StartNotify are `Application__OM_NEW` and
  `Configdata__OM_NEW`. UDivMod32: 46 in vsnprintf, 46 in libgcc's
  `__umodsi3`.
- Open/close: AllocVec 51 in `get_config_imspec` and 40 in
  `get_pattern_imspec` (image specs built per object per open), 20 in
  `zune_text_new`, 16 in `load_custom_frame` (also the 16 FilePart: a
  custom frame file lookup per framed object per open); StrToLong 83 in
  `zune_imspec_setup` and 11 in `zune_pen_spec_to_intern`; TextLength 30
  in Slider's `knob_text_width`; GetAttr 84 in the Area dispatcher, the
  rest Family list lookups in Group Setup/Cleanup/AskMinMax/Layout/Show.
- Redraw (per ten): Draw 520 in `draw_vertical_line`, 110 in
  `cycle_draw`; WritePixel 440 in `button_draw`, 90 in `mx_draw` (radio
  images); RectFill 1230 in `zune_penspec_drawdirect` (backgrounds), 520
  in `draw_horizontal_line`, 230 in `mx_draw`, 200 in `arrow_span`, 90
  in `glyph_draw`; GetAttr 340 in `Group__MUIM_Draw` (two Family list
  lookups per group); InstallClipRegion 60 in Zune's clip stack (one
  install per Add and per Remove) and 20 inside intuition.
- Relayout (per five): GetAttr 180 in `Group__MUIM_AskMinMax`, 85 each
  in Group Layout, Show, Hide and twice in Draw; UDivMod32/SMult32 in
  `Group__MUIM_Layout` (about 300), `Layout1D_mui_redistrib` (160),
  `numeric_scale` and libgcc `__umodsi3` (300); AllocVec 20 in
  `Group__MUIM_AskMinMax`.
<!-- /CALLERS -->

## Findings and proposed fixes

<!-- FIXES -->
Status after the implementation pass of 15 October 2026 (uncommitted on
mui5-dev). Times are the median of three fresh boots per toolkit, rotated
order, 10 s settle, perfbench-equal on the 68020 fixture; every run was
within 0.2% of the others.

| Batch | MUI 3.8 ms | before ms | now ms | now vs MUI 3.8 |
| --- | ---: | ---: | ---: | ---: |
| create+dispose | 106.44 | 127.27 | 88.37 | -17.0% |
| open+close | 540.70 | 561.75 | 540.83 | +0.0% |
| redraw x10 | 1563.81 | 1782.13 | 1664.23 | +6.4% |
| relayout x5 | 954.03 | 1116.75 | 987.48 | +3.5% |
| list insert200 | 1516.68 | 1020.30 | 1015.13 | -33.1% |
| list active x50 | 1114.88 | 1134.73 | 1132.56 | +1.6% |
| notify x100 | 698.90 | 685.96 | 683.23 | -2.2% |
| text set x100 | 179.51 | 178.02 | 178.32 | -0.7% |

### Implemented

Creation and disposal:

1. **Keyboard bindings parsed once.** `Configdata__SetKey` keeps the parsed
   expression of each default binding string in a small library-wide
   cache; saved bindings are still parsed per object. ParseIX per tree:
   24 to 0.
2. **No preference files without a preference drawer.** One Lock of
   ENV:zune replaces up to four failing Opens per application.
3. **File notifications start at the first input.** The application's
   prefs file and the Configdata's public screen file notifications start
   in the first MUIM_Application_NewInput/InputBuffered
   (`Application_StartNotifications`, `MUIM_Configdata_SetupNotify`). A
   file written between construction and that point is caught by comparing
   its date with the construction time (`zune_file_written_since`), so
   config_init_probe's reload case passes.
4. **Notify's attribute block on demand.** Allocated when HelpLine,
   HelpNode or AppMessage is first set.
5. **Lock-free class cache hits.** `ZUNE_GetBuiltinClass` and
   `MUI_FreeClass` adjust a cached built-in class's count inside Forbid
   instead of the library semaphore; list insertion is under Forbid too.

Open and close:

6. **Image specs.** A configured image is resolved to its preference
   string directly instead of allocating and freeing a config spec per
   object; pattern specs are shared constants (they hold no screen
   resources); numbers are parsed inline.
7. **Custom frame slots.** Empty slots return before allocating anything
   (16 per window open).

Redraw and relayout:

8. **Background fills without the dispatch chain.** `zune_draw_background`
   and `zune_window_draw_background` call Area's and Window's
   implementation directly when every class of the object down to Area is
   built-in and it is not a virtual group (only Area and Group handle the
   method among built-in classes); anything else still dispatches.
9. **Family list pointer cached in the group.** Fetched once at OM_NEW,
   with a lazy fallback for objects that never had it set.
10. **Radio, arrow and popup glyphs as blitter templates.** One
    BltTemplate per pen instead of a dozen RectFill/WritePixel; templates
    are built on first use in chip memory and freed at expunge.
11. **Bevel corners only when needed.** Area reports which of the two
    corner pixels its background fill has just painted with
    MPEN_BACKGROUND; `button_draw` writes only the others. Redundant
    SetAPen calls before the line helpers are gone.
12. **One MinMax pass per relayout and no zero-delta resize.**
    `Window__MUIM_RecalcDisplay` runs `WindowMinMax` once and only asks
    objects inside hidden subtrees directly; `WindowResize` calls
    SizeWindow only for a real size change and WindowLimits only when the
    limits changed.

### Rejected

- **Dropping the bevel corner pixels.** MUI 3.9 paints the top-right and
  bottom-left corners with MPEN_BACKGROUND even on a white parent and
  under a pressed button's fill (tests/native/cornerbg.c,
  tests/native/pixdump.c "states"). Removing them saved 3.7 ms per redraw
  but changed pixels against MUI 3.9; item 11 keeps the pixels.
- **Replacing frame lines.** On this Kickstart a one-pixel line costs
  200-300 us per call whatever the primitive (see the primitive table), so
  RectFill or blitted lines are not cheaper than Move/Draw.

### Verification

- `make check-core` passes (linked 68000 checks). check_class_lifetime
  now traps Forbid/Permit, which the class cache uses.
- tests/native/pixdump.c: every pixel of a window with frames, glyphs,
  pressed, disabled and hidden/shown objects, in seven states, is
  identical to the build before this pass.
- tests/native/cornerbg.c matches MUI 3.9 in all objects and states.
- list_repaint_probe: 189/189 comparisons pass. config_init_probe passes,
  including filesystem notification reload.
- radiopix, activeframe and ghostdump, rebuilt with `-fno-inline` (see
  below), are identical to the previous build. whitebg exits with rc=20 on
  this fixture for both builds and gave no evidence.

Probes built with the MUI 5 SDK's inline varargs `MUI_NewObject` and
`-Os` take the address of the first variadic argument after inlining and
read leftover stack words as tags. radiopix built that way crashed (a
"child" pointing into its own code) once the library's stack use changed.
Build such probes with `-fno-inline` or with perfbench.c's `va_start`
wrapper.

### Remaining

- **Redraw (+6.4%, about 10 ms per redraw).** Zune installs eight
  multi-rectangle clip regions per redraw against four plain ones; the top
  bevel corner is still written where the parent's fill did not run
  (DRAWALL redraws); the rest is spread over Zune's drawing code with no
  single hotspot in the PC samples.
- **Relayout (+3.5%)** carries the redraw difference.
- **List active (+1.6%)** is outside this pass.
<!-- /FIXES -->

## Reproduction

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os -std=gnu17 \
  -Wall -Wno-pointer-sign -I/path/to/SDK/MUI/C/include \
  -Wl,-u,___stkinit tests/native/perfcensus.c -o /tmp/perfcensus -lamiga

python3 tests/native/census_toolkits.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --binary /tmp/perfcensus --output /tmp/census \
  --profiles candidate mui38 --modes plain count time trace
python3 tests/native/census_report.py /tmp/census/0-candidate-time.txt \
  /tmp/census/0-mui38-time.txt --sort excl
```

`perfcensus_lvos.h` is generated from the NDK inline headers of
m68k-amigaos-gcc 13.4 (function name and LVO per library). The probe
patches the running OS: use it only in a disposable guest, never beside
other MUI clients, and never on real hardware.

For PC samples, boot the fixture with its gdbserver enabled, run
`Bench:perfcensus loop-redraw` (or loop-relayout, loop-open, loop-create)
and attach the sampler to port 2355 while the loop runs.
