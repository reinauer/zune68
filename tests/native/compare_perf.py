#!/usr/bin/env python3
"""Compare two library builds in an exclusively owned disposable UAE guest.

The fixture must boot by executing Bench:run-tests, with Bench: mounted
from fixture/guest. Each build directory must contain both library names.
ROMs, the OS disk, preferences and emulator configuration are user supplied.
"""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture", "emulator", "binary", "baseline", "candidate",
                 "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()
    if args.trials < 1 or args.timeout < 1:
        parser.error("trials and timeout must be positive")
    root = args.fixture.resolve()
    guest = root / "guest"
    libraries = guest / "package/Libs"
    config = root / "run.uae"
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    names = ("muimaster.library", "zunemaster.library")
    builds = {"baseline": args.baseline, "candidate": args.candidate}
    metadata = {
        "binary_sha256": digest(args.binary),
        "config_sha256": digest(config),
        "emulator_sha256": digest(args.emulator),
        "libraries": {label: {name: digest(build / name) for name in names}
                      for label, build in builds.items()},
        "runs": [],
    }
    # All runners sharing a fixture must honor this lock. Never attach this
    # script to an installation already running in another emulator.
    with (root / "runner.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        saved = {path: path.read_bytes() if path.exists() else None for path in
                 [*(libraries / name for name in names), guest / "run-tests",
                  guest / "perfbench"]}
        try:
            shutil.copy2(args.binary, guest / "perfbench")
            for trial in range(args.trials):
                order = ["baseline", "candidate"]
                if trial % 2:
                    order.reverse()
                for label in order:
                    for name in names:
                        source = builds[label] / name
                        if digest(source) != metadata["libraries"][label][name]:
                            raise RuntimeError("library build changed during run")
                        shutil.copy2(source, libraries / name)
                    result_path = guest / "perf-result.txt"
                    result_path.write_text("")
                    (guest / "run-tests").write_text(
                        "FailAt 21\nStack 131072\nAssign MUI: Bench:package\n"
                        "Assign LIBS: Bench:package/Libs ADD\n"
                        "Bench:perfbench screen >Bench:perf-result.txt\n"
                        "Echo ALLDONE >>Bench:perf-result.txt\n")
                    stem = f"{trial}-{label}"
                    with (output / (stem + ".log")).open("w") as log:
                        process = subprocess.Popen(
                            [str(args.emulator), "-f", str(config), "-s",
                             "use_gui=no", "-s", "debugging_features=none"],
                            stdout=log, stderr=log)
                        try:
                            deadline = time.monotonic() + args.timeout
                            while time.monotonic() < deadline:
                                result = result_path.read_text(errors="replace")
                                if "ALLDONE" in result:
                                    break
                                if process.poll() is not None:
                                    raise RuntimeError("emulator exited")
                                time.sleep(1)
                            else:
                                raise RuntimeError("guest timed out: " + result)
                        finally:
                            process.terminate()
                            try:
                                process.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait()
                    (output / (stem + ".txt")).write_text(result)
                    ticks = {name.strip(): int(value) for name, value in
                             re.findall(r"^(.+?)\s+median=\s*(\d+) ticks",
                                        result, re.M)}
                    if "rc=0" not in result or len(ticks) != 8:
                        raise RuntimeError("incomplete benchmark: " + result)
                    row = {"trial": trial, "profile": label, "ticks": ticks}
                    metadata["runs"].append(row)
                    (output / "results.json").write_text(
                        json.dumps(metadata, indent=2) + "\n")
                    print(json.dumps(row), flush=True)
        finally:
            for path, contents in saved.items():
                if contents is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(contents)


if __name__ == "__main__":
    main()
