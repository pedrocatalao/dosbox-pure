#!/usr/bin/env python3
# Turns the files beside this script into dosbox_pure_dxm_edit.h, which the
# core serves on Z: when DOS ex Machina is the frontend.  Run from the root
# of the fork.
import os

HERE = os.path.dirname(os.path.abspath(__file__))
FILES = [("EDIT.EXE", "dxm_edit_exe"), ("EDIT.HLP", "dxm_edit_hlp")]

out = ["// Generated from dxm/edit/ - FreeDOS EDIT 0.9b (GPL-2.0), served on Z: when",
       "// DOS ex Machina is the frontend.  Regenerate after replacing those files:",
       "//   python3 dxm/edit/mkheader.py",
       "#ifndef DOSBOX_PURE_DXM_EDIT_H", "#define DOSBOX_PURE_DXM_EDIT_H", ""]
for fn, sym in FILES:
    d = open(os.path.join(HERE, fn), "rb").read()
    out.append(f"static const unsigned char {sym}[{len(d)}] = {{")
    for i in range(0, len(d), 20):
        out.append("\t" + ",".join(f"0x{b:02x}" for b in d[i:i+20]) + ",")
    out.append("};")
    out.append("")
out.append("#endif")
open(os.path.join(HERE, "..", "..", "dosbox_pure_dxm_edit.h"), "w").write("\n".join(out) + "\n")
