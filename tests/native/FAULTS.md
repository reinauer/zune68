# Failure paths and heap checks

Guest tools for the paths a program takes when memory or another
resource runs out, and for damage to the heap. Each source file's header
has its usage and build line.

| Tool | What it does |
|---|---|
| `failn` | Fails the Nth resource call that library code makes for a program; `failn sweep` runs the program once for each N |
| `failprog` | The program swept: an application with most built-in classes, opened, closed and disposed |
| `failsweep.py` | Runs a sweep on a WinUAE fixture and boots again after each crash |
| `failsym.py` | Turns the addresses in failn's lines into muimaster functions, with the build's link map |
| `memcheck` | Checks every free list around each AllocMem and FreeMem; reports the first damage and the call that caused it |
| `mccspy` | Traces the methods a program's own classes get, to diff a run on MUI 3.8 against one on Zune68 |
| `memtally`, `memreport.py` | Which muimaster functions hold a running program's memory |
| `memstat`, `memlibs` | Free memory after an expunge; the libraries in memory |
| `fixpaths` | Paths fixed after fault injection, one line each |

## A sweep

```
failsweep.py --uae FIX/run.uae --work FIX/work --failn failn \
    --prog failprog --lib build/muimaster/muimaster.library --out sweep.txt
failsym.py --map build/muimaster/muimaster.library.map sweep.txt
```

Leave out `--lib` to sweep the fixture's own library, such as MUI 3.8.
Each line gives the run's exit code and the memory it kept (`lost`). A run
that crashes is recorded as `N=<n> crash`.

The call numbers depend on the boot: a crash found in a full sweep comes
again only in a sweep that starts from 1. With `--keep` the sweep stops at
the first crash and leaves the emulator running. Before the crash, set
breakpoints on exec's `Alert` and `ColdReboot` (their jump targets in the
ROM), or the machine resets and the evidence is gone. failn's record of
the failed call is at the address it prints as `rec=`.
