# Application dependency and shutdown checks

## Environment and scope

These checks use disposable WinUAE guests with real AmigaOS 3.2.3,
68020 emulation and a Workbench screen. The classic library is beta1
(`7b847fba`, version 19.81). The development library is built from
`58a17519` (version 21.227); later commits preceding this report change
only tests and documentation. Application and class version strings and
SHA-256 hashes are in `results/mui5-applications-20261014-versions.json`.
No application binaries, keys or third-party library binaries are included.

This does not reproduce an AmigaOS 3.9 BB4/AfA OS/68060 installation.
Network activity is disabled for the Lumi shutdown checks. LumiZip has
no archive open; LumiWeather has no configured location. Archive commands,
weather updates, background network work and saved user preferences are
outside these checks.

## IBrowse 3.0a and Newstring

The tested IBrowse executable identifies itself as 30.8 (22.12.2023).
Both the MUI class drawer and the browser's local `mui` drawer receive
the same Newstring version for each run. No library version patching is
used. A separate small client opens muimaster.library and calls
MUI_GetClass("Newstring.mcc") before launching the browser.

| Library | Newstring | Result |
| --- | --- | --- |
| beta1 19.81 | 21.26 from MUI 5 | Class unavailable; IBrowse displays `Couldn't create CL_URLString` |
| beta1 19.81 | classic 16.18 | Class available; local form page opens and ARexx reports its URL and active browser/window |
| development 21.227 | 21.26 from MUI 5 | String-derived class available; same page and ARexx queries succeed |

The modern MCC calls OpenLibrary("muimaster.library", 20). It cannot
initialize under beta1's version 19.81. This reproduces the error without
AfA OS and explains how a previously installed MUI 5 class can prevent
IBrowse from starting. It does not establish which MCC a remote user has
installed. Retain backups and test a compatible classic MCC in an isolated
classic installation; do not change beta1's version field to bypass the
class dependency.

The raw dependency and browser-query output is in
`results/mui5-applications-20261014-ibrowse-{classic,modern,development}.txt`.
In the failing case the browser remains in its error requester and the
ARexx query does not return before the 150-second host timeout. In the
passing case the browser loads the local page and responds to all three
queries.

## Lumi shutdown

Public LumiZip 1.0.1 (12.10.2026) and LumiWeather 1.4.524 (11.10.2026)
were started from a shell with a 128 KiB stack. A separate client locates
the application window and injects a real close-button click through
input.device. LumiWeather's confirmation is answered with its Q shortcut.
The launching shell writes `EXIT:0` after the application returns; an
independent shell writes `ALIVE` after the close attempt.

Both applications return successfully under beta1 and the development
build in this fixture.
This does not resolve the reported shutdown freeze on OS 3.9/AfA OS.
A follow-up needs a matching system, dependency versions, preferences and
quit path, then a task/stack snapshot at the actual failure.

An earlier LumiZip ARexx QUIT attempt left the window open while an
independent shell continued. That is a separate command-path observation,
not proof of a disposal deadlock or Workbench freeze. The window-close
checks above do exercise actual application shutdown.

## Restoring the original MUI installation

Zune68's muimaster.library and zunemaster.library are independent complete
libraries. Removing only zunemaster.library does not restore a replaced
muimaster.library. Restore the desired original muimaster.library from a
verified `.zune68-old` backup, or reinstall that MUI version; restore
replaced external classes as appropriate, then reboot. The installer
preserves an existing backup, so its contents must be checked rather than
assuming it is the most recent pre-install version.
