# MUI 5 compatibility probes

For creation/List performance investigation, see
[PROFILING.toolkits.md](PROFILING.toolkits.md). Its test-only dispatcher
wrappers require a disposable guest and are separate from compatibility
and uninstrumented performance measurements.

`make -C workbench/libs/muimaster check-core` builds the probes against
Zune's generated headers. Compilation is not a guest runtime test.
The existing linked-code String tests also exercise full 64-bit getters
and Newstring's selection anchor on a 68000 CPU model.

For an independent ABI comparison, compile each `mui5_*_probe.c` with the
2025 OS3 SDK headers and run the same executable with both Zune68 and the
2021 stable MUI 5 runtime (21.227). For example:

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -std=gnu17 -O2 \
  -Wall -Wno-pointer-sign -Wno-int-conversion \
  -I/path/to/SDK/MUI/C/include -Wl,-u,___stkinit \
  -o menu-probe mui5_menu_probe.c
```

Use separate disposable guest installations; do not run two emulators
against one writable disk. The runtime needs the corresponding MUI
libraries/classes and a working AmigaOS installation. Reference MUI 5
requires a 68020. Zune's menu, image, Title, Text, List and String input
probes accept a `screen` argument to open a private planar screen for a 68000 fixture.
Input probes inject real input.device events; keep other input idle.
Read the final PASS/FAIL line and exit status, not just a runner marker.

| Probe | Checks |
| --- | --- |
| `mui5_menu_probe` | Copied titles/shortcuts, borrowed AISS names, owned/borrowed BOOPSI image disposal, encoded shortcuts and batch methods. |
| `mui5_menu_input_probe` | Command-key case, nested changes, Escape, selection, tall image drawing; Zune also closes the parent during a popup. |
| `mui5_image_probe` | Missing-picture fallback and copied spec render alike; inactive hints do not replace contents. Pixel hashes are compared within each runtime. |
| `mui5_string_probe` | Hint aliases, 64-bit SET/GET boundaries, cursor preservation when setting DisplayPos and default text pointer. The stable reference getter's 32-bit clamp is explicitly labeled. |
| `mui5_string_input_probe` | Hook editing/veto/delete/Return, ordinary editing before a lonely hook, forwarded Newstring selection and clipboard round trip; Zune editing-menu actions. |
| `mui5_title_probe` | Built-in class creation, mouse page selection, FindPage and default paired removal; Zune also exercises bottom tabs, subclass new/close buttons, paired drag reordering, last-close notification, empty groups, keyboard navigation, overflow selection and reopening. |
| `mui5_text_probe` | Borrowed/copied contents, all six shortening modes and getters, whole-text hiding, control-character behavior and pixel comparisons against explicitly shortened strings. Zune also checks that elision retains styled chunks. |
| `mui5_list_probe` | Sort-column callback ABI, external-array sorting, inline editor creation, commit veto, Return, Escape, hidden-column editing and disposal; Zune also checks title-click notification. |

Title and List use original test subclasses to exercise the documented
ownership contract. The application creates an inline editor and applies
accepted changes in `EditDone`; List attaches, lays out and disposes the
editor. The abort callback receives a NULL entry. The tests use the same
OS3-SDK-built executable in each runtime.

The comparison has explicit limits. MUI 21.227 did not emit `TitleClick`
in this fixture, and its formatted elision differed from equivalent
explicit text. Those cases are Zune regression assertions, not matching
reference claims. Plain elision matches explicit text in both runtimes;
pixel hashes are compared within a runtime with its configured font.
The reference keeps Text_Copy enabled with HiChar, and Text_ControlChar
does not set Area_ControlChar. Its DisplayPos getter follows BufferPos in
the tested unshown object; Zune keeps the documented separate view offset.
Left/right Title placement remains reserved, as in the SDK documentation.

The 13 October 2026 run passed these probes on Zune's emulated OS3/68020
and 68000/no-FPU fixtures. MUI 21.227 passed the shared assertions, with
the differences above reported separately. This is emulator evidence;
physical hardware and complete MUI 5 compatibility are not established.

The String input probe passes the compatibility-mode tag used by the
reference Newstring wrapper so the forwarded selection anchor can be
compared without redistributing or requiring that wrapper for this probe.
The menu tests do not establish full AISS-theme coverage or identical
MUI popup styling. Native Intuition pulldowns do not gain extra images.

## IBrowse fixture

Validated with IBrowse 30.8 (22 December 2023) on emulated AmigaOS 3.2.3.

`mui5_version_gate.c` is a **test-only** helper for a disposable guest.
It pins muimaster, temporarily reports 21.227, and records the IBrowse
Application object through an Intuition NewObjectA hook. It publishes
that object in a named test port for `mui5_ibrowse_probe.c`. Its synchronous
script must quit IBrowse before returning; the helper then restores the
vector, port and library identity. An optional second argument (`keep`)
retains the real version for classic-MUI regression tests. Do not install
this helper in a normal system. Restart the disposable guest after an
interrupted test.

Mount a private directory as `Bench:` containing these two HTML files:

- `mui5-form.html`: title `MUI5 fixture`, a text input with value `seed`,
  a password input and a link to `mui5-second.html`.
- `mui5-second.html`: title `MUI5 second`, any simple local content.

The synchronous test script launches IBrowse with
`URL=file:///Bench:mui5-form.html`, waits for startup, uses its ARexx port
to SHOW and ACTIVATE the window, and runs `mui5_ibrowse_probe modern`.
It then queries the active URL using IBrowse ARexx and quits with
`QUIT FORCE`. Expected URL: `file:///Bench:mui5-second.html`.
The probe verifies the class ancestry and uses real input events for form
editing, autocomplete replacement, URL typing and acknowledgement.

The modern fixture must use the reference installation's Newstring MCC,
which subclasses String. The original Newstring MCC subclasses Area:
a temporary version change alone does not select built-in String.
For the classic comparison, restore the original MCC, use `keep` and
run the probe with `classic` instead of `modern`.

No MUI runtime, proprietary SDK/demo source, browser, ROM, disk image,
or reference MCC is included here. The integration test establishes
these specific workflows, not every version-gated browser feature.

## Version 21 development profile

Build `make check release MUI5_TEST=1 BUILD=/absolute/path/to/build`.
Both library names and built-in class getters report 21.227; external MCC
versions stay independent. The package identifies itself as **MUI5 development**.
Use a separate build directory for the default 19.81 profile. Switching
MUI5_TEST in one directory also regenerates the configuration and rebuilds
the affected objects.

CI builds both profiles on `mui5-dev`. Download `zune68-mui5-dev` to test
actual version gates. The ordinary `zune68-release` artifact from that
branch still contains the experimental source changes with the default
identity. Main remains the classic baseline until the development branch
has had an application-testing period and is explicitly merged.

The data probe requires version 21 directly and checks class versions;
it does not patch the installed library. The IBrowse helper remains useful
for locating the application object. With the version 21 package, its
recorded real identity is already 21.227. IBrowse 30.8's String-derived
Newstring path passed form editing, URL entry and navigation with this
profile on the emulated 68020 system.

| Additional probe | Checks |
| --- | --- |
| `mui5_data_probe` | Datamap/Objectmap ownership, sorted iteration with removal, borrowed keys, external pools, Textdata conversion and Text ownership, Dataspace Get/Count/reuse, Group reordering, List StringArray/TitleArray/ColumnOrder, directory totals and rereading. |
| `mui5_selection_probe` | Selectgroup label clicks and radio images, active-object keyboard routing, Textdata rendering, OS3 opacity storage, TextColor and Relayout. |

Both probes are built with the original SDK and run against original
MUI 21.227 and Zune68. They also run on Zune's 68000/no-FPU fixture; use
`screen` to open a private planar screen. The data probe creates and removes
`T:mui5-dir-probe`; run it only in a disposable guest without that directory.
Textdata was exercised with and without codesets.library on the 68000.

Observed reference differences are explicit in the probes:

- MUI 21.227 stores the Selectgroup Next/Prev sentinel values in this
  fixture; Zune implements documented wrapping. These assertions and
  keyboard routing are Zune-specific.
- Detaching Text_Data clears ordinary contents in MUI 21.227. Zune retains
  the separately supplied ordinary contents as a fallback.
- MUI 21.227 includes directory overhead in the byte count (1033 for the
  two files totaling 9 bytes in this RAM: fixture). Zune sums file bytes.
  The shared assertions check consistent 32/64-bit values and rereading.
- Family reorder inserts each item after a fixed anchor, reversing the
  supplied sequence, except when appending. Zune matches this behavior.
- TextColor and Relayout are V22 APIs, tested as Zune behavior without a
  matching V22 reference runtime. Opacity storage is compared on OS3.

## Selector, marking and menu/List extension probes

The following probes exercise the development implementation added on
14 October. Compile them with the same SDK and flags as the probes above.

| Probe | Coverage |
| --- | --- |
| `mui5_panel_probe` | Filepanel filtering, ASL aliases, pattern changes, Fontpanel inventory and mutable font/style/pen/draw-mode settings, accept/cancel/close-request and repeated opening. |
| `mui5_marking_probe` | Forward/reverse mouse marking, visible selection, formatted chunks, elided suffixes, multiline clipboard text and reopening. |
| `mui5_list_format_probe` | Pixel column widths, reordering and logical hiding, borrowed object cells, balanced setup/cleanup and caller-owned disposal. |
| `mui5_menu_probe` | Menuitem copying, independent owned strings and shared BOOPSI image lifetime in addition to the previous menu checks. |
| `mui5_menu_input_probe` | Actual input.device events: case-sensitive shortcuts, popup selection/images, four nested branches, cancellation, parent closure and native WillOpen closing its window. |

Filepanel and Fontpanel are real selector groups. Direct Panel_Run calls
run the supplied application's modal loop in the caller's task and leave
its window open. MUI_AllocAslRequest/MUI_AslRequest now create supported
file/font requesters in a separate process, exporting owned strings,
multi-file WBArgs and directory locks before disposing the panel. Filter
callbacks receive the original public requester address. Unsupported ASL
options and other requester types continue through asl.library.

Fontpanel supports installed fonts, styles, pens, draw modes and outline/
shadow previews. User font families are saved to ENV:Zune/fontfamilies
and ENVARC:Zune/fontfamilies with replacement/rollback handling. Native
probes use temporary RAM assigns and restore the original assignments.
Fontdisplay is a shared built-in preview class. Installed font engines
remain responsible for font availability and rasterization; no private
engine-specific API is claimed.

Text marking copies parsed text, excluding formatting codes, embedded
objects and synthetic ellipses. Its optional selection state is allocated
only for objects created with MUIA_Text_Marking. The marking probe sends
IntuiMessage events directly to the event handler; the menu input probe
uses input.device events.

Menu copies retain destination identity and children. Arbitrary BOOPSI
images have no generic clone method: copies share a reference-counted
lifetime and release the image once. The original 21.227 reference
accepted borrowed-string copying but hung in the owned-string copy probe;
those ownership checks are therefore Zune-specific assertions. WillOpen
is a V22 method, tested without a matching V22 reference runtime.

List columns now retain physical width constraints when their contents
are reordered, matching 21.227. Constrained pixel columns include its
six-pixel internal allowance; legacy FIXWIDTH includes the inter-column
advance. TestPos rejects blank space beyond the last column. Explicit
DELTA=0 follows the SDK; 21.227 substitutes its default gap. Object cells
have balanced setup/cleanup and remain caller-owned; the 21.227 reference
did not draw the probe's object cell. Frame padding and effect artwork
are not pixel-identical to the reference.

String edit hooks now honor SGA_END, SGA_REUSE, SGA_NEXTACTIVE and
SGA_PREVACTIVE. The action matrix was compared with 21.227, including an
inactive string whose Window_ActiveObject pointer remains unchanged.
Textdata_SourceEncoding has no documented function and was ignored by
21.227 in the tested cases; Encoding controls conversion in both runtimes.

Text parsing covers tabs, outline/shadow/ghost toggles, JAM2/inverse,
foreground/background pens, selected/hidden inline images, explicit
object widths, RGB/ARGB colors and raw ARGB images. Classic CyberGraphX
V41/Picasso96 pixel arrays supply software alpha blending on RTG screens;
no newer MorphOS vectors are called. Plain text retains direct drawing.
The RTG probe checks opaque, half-alpha and transparent pixels, preserving
the current color when only alpha changes, and an inline alpha image.
Planar effect probes check state resets and rendering, not identical
MUI artwork. Complex clipping, themed images and alpha combined with
all soft-style effects still need broader application coverage.

Process.mui has separate-task launch, signaling, cooperative kill and
joined disposal. Keyadjust.mui supports copied/validated shortcuts,
keyboard capture, mouse capture options and a menu for editing multiple
shortcuts. Its native input probe validates keyboard capture and real popup selection
for removing a shortcut and appending another. Mouse-expression capture
and double/triple-click suppression have direct event-handler checks;
all physical mouse/qualifier combinations remain broader coverage.
Two simultaneous file requesters also open and cancel independently.

### Remaining class and theme scope

The SDK calls Scrmodelist private; Gadget is an Intuition-gadget base
with no complete subclass contract. Argstring's attributes are explicitly
undocumented. The SDK provides no class autodocs for Menubar,
Frimagedisplay, Popfrimage, Menudisplay, Backgroundadjust, Pubscreenadjust,
Pubscreenpanel, Pubscreenlist, Applist, Cclist, Screenspace, Imagespace,
Rootgrp, Audiocontrols, Audiomixer, Popmenu, Imagebrowser, Colorring,
Pubscreen, Aboutpage or Winborder. These classes have not been replaced
with empty objects. They require a concrete caller and behavioral probes.
Screenmodepanel and Slave remain separate public class gaps; screen-mode
ASL requests still work through native ASL. Full MUI preferences/theme
file compatibility and composited window opacity are not implemented.
These results do not establish complete MUI 5 compatibility.

### Native batch reproduction

`run_mui5_probes.py` locks a disposable guest fixture and restores replaced
libraries and startup files. It requires a fixture that boots
`Bench:run-tests` with `fixture/guest` mapped as Bench:. For example:

```sh
python3 tests/native/run_mui5_probes.py \
  --fixture /path/to/disposable-fixture --emulator /path/to/winuae \
  --build /path/to/build --libraries /path/to/build \
  --output /path/to/results.txt \
  panel requesters process keyadjust text_escape rtg list_format \
  data string_input text list
```

Omit `--libraries` to retain the fixture's reference MUI. Omit `rtg` for
a planar-only installation. Functional runs use unrestricted emulation
speed and must never be used as timing measurements. The complete new
probe batch passed on 68000 and 68020 AmigaOS 3.2.3; the RTG pixel probe
passed on the 68020 Picasso96 fixture. Physical hardware and AfA OS are
not covered by these results.

## Classic-workload performance

See [PERFORMANCE.mui5.md](PERFORMANCE.mui5.md) for the comparison against
published baseline `7b847fba`, the raw run medians, library-size changes
and reproduction commands. `compare_perf.py` requires a disposable guest
fixture and records hashes of the exact emulator, benchmark and libraries.

[PERFORMANCE.toolkits.md](PERFORMANCE.toolkits.md) compares the development
library with original MUI 3.8 and MUI 5. `compare_toolkits.py` keeps complete
toolkit installations separate and restores a pristine OS disk before
each boot. It uses the benchmark's `screen-fixed` mode, which requests a
larger screen and bounded geometry and checks that the root fits inside
the window. Actual toolkit-constrained dimensions are recorded.

## Application dependency and shutdown checks

See [APPLICATIONS.mui5.md](APPLICATIONS.mui5.md) for the IBrowse 3.0a
Newstring dependency comparison, Lumi shutdown checks and their OS limits.
