#!/usr/bin/env python3
"""Collect completed profile_toolkits runs and fill the profiling report."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from statistics import median


def sha(data):
    return hashlib.sha256(data).hexdigest()


def collect(directories):
    runs, binaries, inputs = [], set(), {}
    for directory in directories:
        data = json.loads((directory / "results.json").read_text())
        binaries.add(data["binary_sha256"])
        for key in ("config_sha256", "emulator_sha256", "pristine_disk_sha256"):
            if key in inputs:
                assert inputs[key] == data[key], key
            inputs[key] = data[key]
        for run in data["runs"]:
            raw = (directory / run["transcript"]).read_bytes()
            assert sha(raw) == run["sha256"]
            text = raw.decode()
            assert "rc=0" in text and "ALLDONE" in text
            assert not re.search(r"(?:overflow|underflow)=[1-9]", text)
            timing = {name: {"median": int(mid), "min": int(low),
                            "max": int(high), "repetitions": int(n)}
                      for name, mid, low, high, n in re.findall(
                          r"timing (\S+) median=(\d+) min=(\d+) max=(\d+) n=(\d+)", text)}
            assert len(timing) == 5
            trace = []
            for block in text.split("trace phase=")[1:]:
                trace.append({"phase": block.split()[0], "methods": [
                    {"class": cls, "method": method, "calls": int(calls),
                     "inclusive": int(inc), "exclusive": int(exc)}
                    for cls, method, calls, inc, exc in re.findall(
                        r"stat (\S+) method=(\w+) calls=(\d+) inclusive=(\d+) exclusive=(\d+)", block)]})
            files = data["profiles"][run["profile"]]
            runs.append({**run, "timing": timing, "trace": trace,
                "toolkit_manifest_sha256": sha(json.dumps(files, sort_keys=True).encode()),
                "library_sha256": files["Libs/muimaster.library"],
                "text": text})
    assert len(binaries) == 1
    return {**inputs, "binary_sha256": binaries.pop(), "runs": runs}


def values(data, profile, mode, phase):
    return [r["timing"][phase]["median"] for r in data["runs"]
            if r["profile"] == profile and r["mode"] == mode]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", nargs="+", type=Path, required=True)
    parser.add_argument("--focus", type=Path, required=True)
    parser.add_argument("--experiment", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    base = collect(args.baseline)
    focus = collect([args.focus])
    experiment = collect([args.experiment])
    for group in (focus, experiment):
        for key in ("config_sha256", "emulator_sha256", "pristine_disk_sha256"):
            assert base[key] == group[key], key
    manifests = {}
    for group in (base, focus, experiment):
        for row in group["runs"]:
            old = manifests.setdefault(row["profile"], row["toolkit_manifest_sha256"])
            assert old == row["toolkit_manifest_sha256"]
    for profile in ("mui38", "zune68"):
        for mode in ("plain", "trace"):
            selected = [r for r in base["runs"] if r["profile"] == profile and r["mode"] == mode]
            assert len(selected) == 3 and len({r["trial"] for r in selected}) == 3
    expected = {
        "mui38": {"active-scroll50": (90, 41, 41),
                  "active-visible50": (49, 0, 0), "active-closed50": (0, 0, 0)},
        "zune68": {"active-scroll50": (57, 1607, 346),
                   "active-visible50": (98, 500, 100), "active-closed50": (0, 500, 100)},
    }
    for row in base["runs"]:
        for trace in row["trace"]:
            if trace["phase"] not in expected[row["profile"]]:
                continue
            counts = {(m["class"], m["method"]): m["calls"] for m in trace["methods"]}
            actual = tuple(counts.get(key, 0) for key in (("List.mui", "80426f3f"),
                           ("Area.mui", "00000103"), ("Prop.mui", "00000103")))
            assert actual == expected[row["profile"]][trace["phase"]]
            if row["profile"] == "zune68":
                displays = {"active-scroll50": 1246, "active-visible50": 98, "active-closed50": 0}
                assert counts.get(("List.mui", "80425377"), 0) == displays[trace["phase"]]
    freq = 709.379  # EClock ticks per millisecond, verified in every transcript.
    table = ["Median of three boot medians; milliseconds per batch. The range",
             "columns show the minimum and maximum boot medians.", "",
             "| Phase | MUI 3.8 ms (range) | Zune68 ms (range) | Zune time difference |",
             "| --- | ---: | ---: | ---: |"]
    for name in ("create", "dispose", "active-scroll50", "active-visible50", "active-closed50"):
        a, b = [values(base, p, "plain", name) for p in ("mui38", "zune68")]
        def cell(v):
            return f"{median(v)/freq:.2f} ({min(v)/freq:.2f}–{max(v)/freq:.2f})"
        table.append(f"| {name} | {cell(a)} | {cell(b)} | {(median(b)/median(a)-1)*100:+.2f}% |")
    focus_rows = ["One fresh boot per toolkit, 15 repetitions, with only Application",
                  "and Configdata new/dispose wrapped. Times below are instrumented.", "",
                  "| Creation scope | MUI 3.8 ms | Zune68 ms |", "| --- | ---: | ---: |"]
    for cls in ("whole tree", "Application.mui", "Configdata.mui"):
        row = []
        for profile in ("mui38", "zune68"):
            if cls == "whole tree":
                v = values(focus, profile, "trace", "create")
            else:
                v = [m["inclusive"] for r in focus["runs"] if r["profile"] == profile
                     for t in r["trace"] if t["phase"] == "create"
                     for m in t["methods"] if m["class"] == cls and m["method"] == "00000101"]
                assert len(v) == 15
            row.append(median(v)/freq)
        focus_rows.append(f"| {cls} | {row[0]:.2f} | {row[1]:.2f} |")
    focus_rows += ["", "Application includes Configdata; do not add these rows. MUI 3.8",
        "initializes preferences later in its Application path, so its small",
        "Configdata constructor is not an equivalent measure of preference work.", "",
        "The Application constructor explains about 29 ms of the roughly 48 ms",
        "whole-tree difference in this focused run. Zune's Configdata constructor",
        "takes about 65 ms inside that path. The remaining difference includes",
        "window/widget construction and library dispatch outside Application. The full",
        "trace also identifies Notify, Area and Group construction as contributors.", "",
        "Even this narrower instrumentation raises whole-tree creation time.",
        "These figures localize work; the plain table remains the speed result."]
    exp_rows = ["A separate test binary sets `MUIA_Group_Forward=FALSE` alongside each",
        "timed `MUIA_List_Active` tag, using the unchanged Zune library. It verifies",
        "the final active entry after each workload. Three fresh boots each take",
        "15 repetitions. This is a diagnostic API experiment, not a production fix.", "",
        "| Batch | Ordinary Zune ms | Forwarding disabled ms | Change |",
        "| --- | ---: | ---: | ---: |"]
    for name in ("active-scroll50", "active-visible50", "active-closed50"):
        a = median(values(base, "zune68", "plain", name))
        v = values(experiment, "zune68", "plain", name)
        assert len(v) == 3
        b = median(v)
        exp_rows.append(f"| {name} | {a/freq:.2f} | {b/freq:.2f} | {(b/a-1)*100:+.2f}% |")
    exp_rows += ["", "This only suppresses forwarding for the caller's active-entry tag;",
        "internally generated updates and rendering still run. It does not establish",
        "safe filtering for arbitrary mixed tag lists or application subclasses.", "",
        "The large scrolling gap is mainly rendering work: suppressing this tag's",
        "forwarding saves only about 3% of the scrolling batch. Reusing rows and",
        "reducing repeated text preparation have higher potential for that workload."]
    document = args.report.read_text()
    def fill(document, marker, replacement):
        start, end = f"<!-- {marker} -->", f"<!-- /{marker} -->"
        pattern = re.escape(start) + r".*?" + re.escape(end)
        assert len(re.findall(pattern, document, re.S)) == 1
        return re.sub(pattern, lambda _: start + "\n" + replacement + "\n" + end,
                      document, flags=re.S)
    for marker, replacement in (("TIMINGS", table), ("CREATION", focus_rows), ("FORWARDING", exp_rows)):
        document = fill(document, marker, "\n".join(replacement))
    stem = "toolkit-profile-20261014"
    evidence = (f"Raw transcripts: `results/{stem}-transcripts.txt`.\n"
        f"Parsed timings and input hashes: `results/{stem}.json`.\n"
        "The library hashes match the original toolkit comparison. The default\n"
        "probe rebuilt from this source was byte-identical to the measured binary.\n")
    document = fill(document, "EVIDENCE", evidence.rstrip())
    args.report.write_text(document.rstrip() + "\n")
    transcripts = []
    for label, data in (("baseline", base), ("focus", focus), ("noforward", experiment)):
        for run in data["runs"]:
            text = run.pop("text")
            run.pop("trace")  # Full method traces remain in the raw transcript.
            assert "eclock=709379" in text
            transcripts.append(f"=== {label}/{run['transcript']} ===\n{text.rstrip()}\n")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / (stem + "-transcripts.txt")).write_text("\n".join(transcripts))
    result = {"baseline": base, "focus": focus, "noforward": experiment,
        "probe_source_sha256": sha(Path(__file__).with_name("perfprofile.c").read_bytes()),
        "tree_source_sha256": sha(Path(__file__).with_name("perfbench.c").read_bytes())}
    (args.output / (stem + ".json")).write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
