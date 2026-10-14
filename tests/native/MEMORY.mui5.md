# MUI5 development memory analysis

Compare published classic Zune68 `7b847fba` with development commit `6abcf70c` (version 21.227).
Distinguish file size, loadable code/data/BSS, live object allocations,
and storage allocated while a window is open. This is not an application
peak-memory measurement.

## Resident library size

Final measured build (`6abcf70c`):

| Measurement | Classic | MUI5 development | Change |
| --- | ---: | ---: | ---: |
| Stripped muimaster.library | 334,840 | 404,056 | +69,216 |
| CODE + DATA + BSS | 314,736 | 379,444 | +64,708 |

The second row excludes loader/relocation bookkeeping and the allocated
library base. Stripping symbols reduces the file, not those loadable
segments. Both library names are complete libraries; these are per-library
figures, not a claim that two simultaneously loaded instances share code.

The largest module increases before the final memory pass are Fontpanel
(+9,268), List (+6,736), the text engine (+5,212), Title (+4,680), Filepanel
(+4,492), String (+3,532), Keyadjust (+3,516) and requester glue (+3,400).
Object-file sizes are diagnostic attribution, not an exact decomposition
of the final linked file.

## Implemented reductions

- One-pass Text construction removes repeated tag scans and the allocated
  empty preparse string.
- Plain text uses an arena sized for its single line/chunk/content and
  skips the full escape parser. A nine-character caption requests 112
  bytes rather than 220, retaining one allocation. Linked 68000 parsing
  took 4,062 emulated cycles versus 6,534 for the classic parser. These
  figures exclude font measurement/drawing and are not native timings.
- Background color and alpha-image data share storage in mutually
  exclusive chunk variants, preserving the original 44-byte chunk size.
- Fontpanel uses a shared Fontdisplay class and an actual owned TextFont;
  it no longer stores a 256-byte font specification per panel.
- Fallback encoded-text buffers use an encoding-specific upper bound and
  explicit terminators, instead of four bytes per input byte. A 1,024-byte
  UTF-8 ASCII source converted to Latin-1 needs 1,025 requested bytes,
  rather than 4,100, excluding the conversion-cache node and Exec rounding.

## Runtime measurements

The same 68000/no-FPU probe binary ran in separate fresh boots of the
same AmigaOS 3.2.3/68020 WinUAE fixture. Both library opens reported
`already-loaded=0`. Cold opening consumed 479,672 bytes with the classic
library and 548,200 with the candidate (+68,528). This includes dependent
libraries and loader bookkeeping; it is not just the sum of hunks.

Class measurements use 128 objects, or four for each panel, with one warm
round followed by three measured rounds. The table divides the median
held allocation by the number of objects. It includes Exec rounding and
owned child objects. Process objects were not launched, so their task
stack and running-process resources are excluded.

| Closed object | Classic bytes/object | MUI5 bytes/object | Change |
| --- | ---: | ---: | ---: |
| Notify | 56 | 56 | 0 |
| Rectangle | 304 | 312 | +8 |
| Text | 368 | 384 | +16 |
| String | 640 | 656 | +16 |
| Group | 512 | 520 | +8 |
| List | 7,648 | 8,304 | +656 |
| Title | unavailable | 608 | — |
| Process, not launched | unavailable | 136 | — |
| Keyadjust | unavailable | 2,712 | — |
| Filepanel | unavailable | 34,624 | — |
| Fontpanel | unavailable | 33,184 | — |

A page-mode window with 128 Text objects, all set up but only one visible,
used **34,784 -> 21,472 bytes** of additional open-window storage: a
**13,312-byte (38.27%) reduction**. All three rounds agreed and released
that storage on close. Including construction of those Text objects,
the measured subtotal fell from 81,888 to 70,624 bytes (13.75%). This
subtotal excludes the larger resident library and the parent objects.

The repeated object-disposal deltas were normally zero. Small unrelated
OS changes were visible (up to 512 bytes released and 264 bytes retained
in individual rounds), with no accumulating growth across these rounds.
This short probe cannot prove that every application or feature is leak
free. A subsequent 68000 batch produced the same median object sizes
and window delta; its already-resident library opening is excluded from
the cold-open comparison.

List's larger footprint includes the MUI5 default capacity of 64 display
columns. Two pointer arrays account for approximately 504 additional
requested bytes compared with reserving one column. Subclasses and
custom display hooks may use that capacity even when fewer columns are
visible, so it was retained. Applications that know their maximum can
set `MUIA_List_MaxColumns` explicitly when constructing the list.

Raw observations: [classic](results/mui5-memory-20261014-classic.txt),
[current](results/mui5-memory-20261014-current.txt),
[size snapshots](results/mui5-memory-20261014-size.json), and
[measurement metadata](results/mui5-memory-20261014-metadata.json).

## Reproduce

Build `mui5-memory-probe` through the native makefile and use the same
binary with both libraries. Run it first in each fresh boot using
`run_mui5_probes.py ... memory`; an earlier probe can leave the library
resident and invalidate the cold-open measurement. Record library and
probe hashes. The runner restores the fixture's libraries on exit.
For file size, strip a copy of the library. For resident segment size,
read CODE, DATA and BSS lengths from its Amiga Hunk image; do not sum
symbol-table or relocation-record file sizes into that figure.

## Remaining opportunities and tradeoffs

The new features still increase resident code substantially. Moving the
rarely used requester/preferences classes to demand-loaded modules could
reduce the always-loaded footprint, but requires a separate ABI/lifetime
change and testing. It has not been done in this pass.

Class caching and optional marking/editing state already avoid repeated
construction or unused per-object allocations. Do not remove the cache
or inline layout paths merely to reduce a few hundred bytes: measure the
creation/layout cost first. Function alignment has a small documented
size cost; no unverified compiler or packing change was applied.

The stripped library is 20.67% larger than the classic baseline, and
loadable segments are 20.56% larger. The text/conversion savings improve
object-heavy workloads but do not erase that fixed resident increase.
The final shared-layout/plain-draw optimization costs 796 additional
CODE bytes versus the first completed feature build; timing results are
reported separately in [PERFORMANCE.mui5.md](PERFORMANCE.mui5.md).
