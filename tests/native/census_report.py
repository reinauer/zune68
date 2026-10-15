#!/usr/bin/env python3
"""Compare two perfcensus transcripts (same mode) phase by phase.

usage: census_report.py A.txt B.txt [--phase NAME] [--top N] [--sort calls|excl|top]

For count/time transcripts it prints, per phase, the total EClock ticks,
the time inside OS calls (sum of top-level inclusive) and the remainder
spent in the toolkit's own code, then the per-function rows sorted by the
largest difference. For trace transcripts it compares class methods.
"""
import argparse
import re
from collections import OrderedDict


def parse(path):
    phases = OrderedDict()
    calib = None
    current = None
    for line in open(path, errors="replace"):
        line = line.rstrip("\n")
        if line.startswith("calibration "):
            m = dict(re.findall(r"(\w+)=(\d+)", line))
            calib = int(m["eclock"]) / int(m["vticks"])
        elif line.startswith("phase "):
            name = line.split()[1]
            m = dict(re.findall(r"(\w+)=([\d/]+)", line))
            current = phases[name] = {"eclock": int(m["eclock"]),
                                      "vticks": int(m["vticks"]),
                                      "fn": OrderedDict(), "stat": OrderedDict()}
        elif line.startswith("fn ") and current is not None:
            _, lib, fn, rest = line.split(" ", 3)
            m = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", rest)}
            current["fn"][lib.split(".")[0] + "/" + fn] = m
        elif line.startswith("stat ") and current is not None:
            _, cls, rest = line.split(" ", 2)
            m = dict(re.findall(r"(\w+)=([0-9a-f]+)", rest))
            key = cls + " " + m["method"]
            current["stat"][key] = {"calls": int(m["calls"]),
                                    "incl": int(m["inclusive"], 10),
                                    "excl": int(m["exclusive"], 10)}
        elif line.startswith("timing "):
            name = line.split()[1]
            m = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}
            phases[name] = {"timing": m}
    return calib, phases


def ms(ticks, freq=709379):
    return ticks * 1000.0 / freq


# OS functions that run toolkit code (class dispatchers, hooks) before they
# return: their inclusive and exclusive times contain toolkit work.
CALLBACK = {"intuition/NewObjectA", "intuition/DisposeObject", "intuition/SetAttrsA",
            "intuition/GetAttr", "intuition/SetGadgetAttrsA", "intuition/DoGadgetMethodA",
            "intuition/RefreshGList", "intuition/RefreshGadgets", "intuition/DrawImage",
            "intuition/DrawImageState", "intuition/EraseImage", "utility/CallHookPkt",
            "layers/DoHookClipRects", "exec/RawDoFmt", "intuition/OpenWindowTagList",
            "intuition/OpenWindow", "intuition/CloseWindow", "intuition/ChangeWindowBox",
            "intuition/AddGList", "intuition/RemoveGList", "intuition/SetWindowTitles",
            "intuition/ActivateWindow", "intuition/BeginRefresh", "intuition/EndRefresh"}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--phase")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--sort", default="excl", choices=("calls", "excl", "top", "incl"))
    args = ap.parse_args()
    ca, pa = parse(args.a)
    cb, pb = parse(args.b)
    for name in pa:
        if args.phase and name != args.phase:
            continue
        A, B = pa[name], pb.get(name)
        if B is None:
            continue
        if "timing" in A:
            print(f"{name:12s} A median {ms(A['timing']['median']):9.2f} ms   "
                  f"B median {ms(B['timing']['median']):9.2f} ms   "
                  f"A/B {A['timing']['median'] / B['timing']['median']:.3f}")
            continue
        print(f"== {name}: A {ms(A['eclock']):.2f} ms, B {ms(B['eclock']):.2f} ms "
              f"(A/B {A['eclock'] / B['eclock']:.3f})")
        keys = set(A["fn"]) | set(B["fn"])
        if keys:
            topA = sum(v["excl"] for k, v in A["fn"].items() if k not in CALLBACK) * (ca or 0)
            topB = sum(v["excl"] for k, v in B["fn"].items() if k not in CALLBACK) * (cb or 0)
            cbA = sum(v["excl"] for k, v in A["fn"].items() if k in CALLBACK) * (ca or 0)
            cbB = sum(v["excl"] for k, v in B["fn"].items() if k in CALLBACK) * (cb or 0)
            callsA = sum(v["calls"] for v in A["fn"].values())
            callsB = sum(v["calls"] for v in B["fn"].values())
            print(f"   OS calls: A {callsA}  B {callsB}")
            if ca and cb and topA:
                print(f"   OS exclusive (no callbacks): A {ms(topA):.2f} ms ({100 * topA / A['eclock']:.0f}%)  "
                      f"B {ms(topB):.2f} ms ({100 * topB / B['eclock']:.0f}%)")
                print(f"   callback-function exclusive (toolkit code inside): A {ms(cbA):.2f} ms  B {ms(cbB):.2f} ms")
                print(f"   remainder (toolkit code at top level): A {ms(A['eclock'] - topA - cbA):.2f} ms  "
                      f"B {ms(B['eclock'] - topB - cbB):.2f} ms")
            def val(d, k, f):
                return d["fn"].get(k, {}).get(f, 0)
            def score(k):
                if args.sort == "calls":
                    return abs(val(A, k, "calls") - val(B, k, "calls"))
                return abs(val(A, k, args.sort) * (ca or 1) - val(B, k, args.sort) * (cb or 1))
            rows = sorted(keys, key=score, reverse=True)[:args.top]
            print(f"   {'function':34s} {'calls A':>8s} {'calls B':>8s} {'excl A ms':>10s} {'excl B ms':>10s} {'top A ms':>9s} {'top B ms':>9s}")
            for k in rows:
                print(f"   {k:34s} {val(A, k, 'calls'):8d} {val(B, k, 'calls'):8d} "
                      f"{ms(val(A, k, 'excl') * (ca or 0)):10.2f} {ms(val(B, k, 'excl') * (cb or 0)):10.2f} "
                      f"{ms(val(A, k, 'top') * (ca or 0)):9.2f} {ms(val(B, k, 'top') * (cb or 0)):9.2f}")
        skeys = set(A["stat"]) | set(B["stat"])
        if skeys:
            def sval(d, k, f):
                return d["stat"].get(k, {}).get(f, 0)
            def sscore(k):
                if args.sort == "calls":
                    return abs(sval(A, k, "calls") - sval(B, k, "calls"))
                return abs(sval(A, k, "excl") * (ca or 1) - sval(B, k, "excl") * (cb or 1))
            rows = sorted(skeys, key=sscore, reverse=True)[:args.top]
            print(f"   {'class method':34s} {'calls A':>8s} {'calls B':>8s} {'excl A ms':>10s} {'excl B ms':>10s} {'incl A ms':>9s} {'incl B ms':>9s}")
            for k in rows:
                print(f"   {k:34s} {sval(A, k, 'calls'):8d} {sval(B, k, 'calls'):8d} "
                      f"{ms(sval(A, k, 'excl') * (ca or 0)):10.2f} {ms(sval(B, k, 'excl') * (cb or 0)):10.2f} "
                      f"{ms(sval(A, k, 'incl') * (ca or 0)):9.2f} {ms(sval(B, k, 'incl') * (cb or 0)):9.2f}")


if __name__ == "__main__":
    main()
