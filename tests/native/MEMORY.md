# Memory

## Resident size

Bytes at `bfce5ee1`, GCC 16.2, `-Os`, `TRACE=0`:

| Build | Stripped file | CODE | DATA | BSS |
|---|---:|---:|---:|---:|
| Default (21.227) | 413,252 | 386,816 | 676 | 3,160 |
| `MUI5_TEST=0` (19.81) | 413,208 | 386,772 | 676 | 3,160 |

`muimaster.library` and `zunemaster.library` are complete libraries, so
loading both names does not share code. Strip a copy for the file size and
read the segment lengths from the Hunk image.

## Object and window cost

Measured at `c07393d5`. Median held allocation per closed object, including
Exec rounding and owned children:

| Object | Bytes |
|---|---:|
| Notify | 40 |
| Process, not launched | 136 |
| Rectangle | 296 |
| Text | 368 |
| Group | 488 |
| Title | 576 |
| String | 640 |
| Keyadjust | 2,648 |
| List | 8,120 |
| Filepanel | 32,144 |
| Fontpanel | 32,184 |

Other figures from the same run:

- Opening the library from cold takes 497,120 bytes, including dependent
  libraries and loader bookkeeping.
- A page-mode window with 128 Text objects, one visible, adds 21,472 bytes
  while open. All of it is released on close.

Smart refresh, the default, also holds the covered parts of each window.
With half the window covered that is 30 KB for IBrowse, 40 KB for Voyager
and 53 KB for YAM.

## Tradeoffs

- List reserves 64 display columns, about 504 bytes of pointer arrays
  beyond one column, because subclasses and display hooks may use them.
  Applications that know their maximum can set `MUIA_List_MaxColumns`.
- Plain text uses one arena for its line, chunk and contents. A nine
  character caption requests 112 bytes.
- Text marking allocates its selection state only when
  `MUIA_Text_Marking` is set.
- Rarely used requester and preferences classes are built in. Moving them
  to demand-loaded modules would shrink the library, but needs an ABI and
  lifetime change.
- Do not remove the class cache or the inline layout paths to save a few
  hundred bytes without measuring creation and layout first.

## Running the probe

`mui5-memory-probe` is built by `check-core`. Run it first in a fresh boot
with `run_mui5_probes.py ... memory`, because an earlier probe can leave the
library resident and invalidate the cold-open figure. Class figures use 128
objects, or four for each panel, and take the median of three rounds after
one warm round. Process objects are not launched, so their task stack is
not included.

[Raw output](results/mui5-memory-20261101.txt) and
[hashes](results/mui5-memory-20261101-metadata.json).
