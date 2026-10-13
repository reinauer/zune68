# MUI 5 menu and String probes

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
requires a 68020. Zune's menu, image and String input probes accept a
`screen` argument to open a private planar screen for a 68000 fixture.
Input probes inject real input.device events; keep other input idle.
Read the final PASS/FAIL line and exit status, not just a runner marker.

| Probe | Checks |
| --- | --- |
| `mui5_menu_probe` | Copied titles/shortcuts, borrowed AISS names, owned/borrowed BOOPSI image disposal, encoded shortcuts and batch methods. |
| `mui5_menu_input_probe` | Command-key case, nested changes, Escape, selection, tall image drawing; Zune also closes the parent during a popup. |
| `mui5_image_probe` | Missing-picture fallback and copied spec render alike; inactive hints do not replace contents. Pixel hashes are compared within each runtime. |
| `mui5_string_probe` | Hint aliases, 64-bit SET/GET boundaries and default text pointer. The stable reference getter's 32-bit clamp is explicitly labeled. |
| `mui5_string_input_probe` | Hook editing/veto/delete/Return, forwarded Newstring selection and clipboard round trip; Zune editing-menu actions. |

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
