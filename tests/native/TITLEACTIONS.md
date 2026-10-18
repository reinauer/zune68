# Native Snapshot and Popup titlebar buttons

The AmigaOS 3 titlebar now supports all four preferences switches:

- M opens application settings.
- S snapshots the window's position and inner size; double-clicking
  unsnapshots it. It is disabled for windows without a Window_ID.
- P opens a Window menu with Settings, Snapshot, Unsnapshot, Iconify and
  Send to back. Snapshot entries are disabled without a Window_ID.
- I iconifies the application.

Enable the switches in Zune Prefs, apply the preferences and reopen the
application's windows. The prefs switches now have explanatory bubble help.
Borderless, backdrop and toolbox windows keep their existing exclusions.

Explicit snapshots update only the selected ID in both ENV and ENVARC
application preference files. Appearance settings and other saved window
positions are preserved independently in each file. Unsnapshot removes the
entry from both files and the running application's position table. Normal
remember/save-on-exit settings remain separate: automatic close/dispose
capture updates memory, with the existing application exit code performing
its configured save. Those automatic settings can therefore remember a
window again after an explicit unsnapshot.

The click timestamp survives preference-driven window recreation, so the
first click's preference notification does not break double-click handling.
The popup uses the existing modal menu implementation and a window guard
so closing its parent during nested input cancels it safely. Gadgets,
images, allocated gadget IDs and image classes retain the existing cleanup
path. Additional gadget slots are allocated only when the enabled buttons
need them; the default M/I pair retains its two-slot allocation.

## Native validation

`titleactions.c` is an interactive guest probe driven by synthetic IDCMP
messages and input.device mouse/keyboard events. **Use a disposable OS
fixture only:** it writes `zune/ZuneTitleProbe.prefs` in ENV and ENVARC.
ENV and ENVARC must be separate directories for the preference preservation
checks (the test fixture assigns ENV to RAM:TitleENV).

The probe checks:

- all sixteen button masks and repeated creation/disposal;
- disabled Snapshot with no ID and rejection of a detached Window;
- single-click snapshot and double-click unsnapshot;
- persistence in ENV and ENVARC without copying session appearance settings;
- preservation of another window's saved position;
- popup opening, Escape cancellation and snapshot/unsnapshot selections;
- parent close during nested popup input;
- position restoration by a new application instance, and defaults after
  unsnapshot;
- preservation of a malformed position chunk without reading past it.

Build against the generated Zune68 SDK, for example:

```sh
m68k-amigaos-gcc -m68000 -msoft-float -noixemul -Os \
  -Wno-int-conversion -I/path/to/build/include \
  -Iworkbench/libs/muimaster/include -Wl,-u,___stkinit \
  tests/native/titleactions.c -o /path/to/titleactions -lamiga
```

Run with `tests/native/run_probe.py` in a prepared, exclusively owned
fixture, or execute `titleactions` from its guest startup script after
assigning MUI and LIBS to the candidate installation. The fixture used here
has AmigaOS 3.2.3 and a 640x480 depth-2 screen in HIRESLACE mode. Both 68000
and 68020 configurations are tested; this is emulator validation, not
physical hardware or a byte-for-byte original MUI menu reproduction.

Raw transcripts and binary/source hashes are in
`results/titleactions-20261017-*`.
