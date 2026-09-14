# FreeDOS EDIT

DOS ex Machina serves FreeDOS EDIT on Z: (see `dosbox_pure_dxm.h`), built
into the core as `dosbox_pure_dxm_edit.h` by `mkheader.py`.

| File | What it is |
|---|---|
| `EDIT.EXE`, `EDIT.HLP` | FreeDOS EDIT 0.9b, unmodified, from `BIN/` in `edit.zip` |
| `edit.zip` | the FreeDOS 1.3 `base/edit` package as released: binaries, docs, `COPYING`, and EDIT's source in `SOURCE/EDIT/SOURCES.ZIP` |
| `dfp100s.zip` | FreeDOS D-Flat+ 1.00 source as released: the TUI library and the DTOOL libraries EDIT is built against |

Both are GPL-2.0 (D-Flat+ also offers an LGPL-2.1 option). Together they are
the complete source of the binaries above; build with Borland C/C++ 3.1 as
their makefiles describe.

Where they came from:

- https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/base/edit.zip
  sha256 `582729981a967493f59f38b7d4464c1b1a6caa25f71c0a2fbf6dd9e3b782037b`
- https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/devel/libs/dflat/dfp100s.zip
  sha256 `7616d1fe54a163f33cf27aa9ef46632040e24701053e7c9b30a3bbd388614911`

`EDIT.EXE` (sha256 `dc38028f…6ae5`) and `EDIT.HLP` (`9c90eac6…2265`) are
byte-identical to the copies in `edit.zip`. To replace them, put the new
files here and run `python3 dxm/edit/mkheader.py` from the root of the fork.
