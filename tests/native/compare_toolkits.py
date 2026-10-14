#!/usr/bin/env python3
"""Time complete toolkit installations in a disposable, exclusively owned guest.

The fixture boots Bench:run-tests and mounts fixture/guest as Bench:.
Its run.uae must use fixture/system.hdf. A pristine.hdf is restored before
each boot. Install each toolkit below guest/<profile>, including Libs/.
Never use this runner with an existing installation or a running emulator.
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


def manifest(root):
    return {str(p.relative_to(root)): digest(p)
            for p in sorted(root.rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture", "emulator", "binary", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--profiles", nargs="+", required=True)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()
    if args.trials < 1 or args.timeout < 1:
        parser.error("trials and timeout must be positive")
    if len(set(args.profiles)) != len(args.profiles) or any(
            not re.fullmatch(r"[a-z0-9_-]+", p) for p in args.profiles):
        parser.error("profiles must be unique lowercase directory names")
    root = args.fixture.resolve()
    guest = root / "guest"
    config = root / "run.uae"
    pristine = root / "pristine.hdf"
    disk = root / "system.hdf"
    if str(disk) not in config.read_text():
        parser.error("run.uae must use the fixture's system.hdf")
    for profile in args.profiles:
        if not (guest / profile / "Libs/muimaster.library").is_file():
            parser.error("profile has no muimaster.library: " + profile)
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    metadata = {
        "binary_sha256": digest(args.binary),
        "config_sha256": digest(config),
        "emulator_sha256": digest(args.emulator),
        "pristine_disk_sha256": digest(pristine),
        "profiles": {p: manifest(guest / p) for p in args.profiles},
        "runs": [],
    }
    with (root / "runner.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        saved = {p: p.read_bytes() if p.exists() else None for p in
                 (guest / "run-tests", guest / "perfbench")}
        try:
            shutil.copy2(args.binary, guest / "perfbench")
            for trial in range(args.trials):
                # Three profiles each occupy every boot-order position.
                offset = trial % len(args.profiles)
                order = args.profiles[offset:] + args.profiles[:offset]
                for profile in order:
                    if manifest(guest / profile) != metadata["profiles"][profile]:
                        raise RuntimeError("toolkit files changed: " + profile)
                    shutil.copy2(pristine, disk)
                    result_path = guest / "perf-result.txt"
                    result_path.write_text("")
                    (guest / "run-tests").write_text(
                        "FailAt 21\nStack 131072\n"
                        f"Assign MUI: Bench:{profile}\n"
                        f"Assign LIBS: Bench:{profile}/Libs\n"
                        "Assign LIBS: SYS:Libs ADD\n"
                        "Assign LIBS: SYS:Classes ADD\n"
                        "Bench:perfbench screen-fixed >Bench:perf-result.txt\n"
                        "Echo ALLDONE >>Bench:perf-result.txt\n")
                    stem = f"{trial}-{profile}"
                    print("starting " + stem, flush=True)
                    started = time.monotonic()
                    with (output / (stem + ".log")).open("w") as log:
                        process = subprocess.Popen(
                            [str(args.emulator), "-f", str(config), "-s",
                             "use_gui=no", "-s", "debugging_features=none"],
                            stdout=log, stderr=log)
                        try:
                            deadline = started + args.timeout
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
                    version = re.search(r"^library (\S+) eclock=(\d+)", result, re.M)
                    geometry = re.search(r"^geometry .+", result, re.M)
                    if not version or not geometry or "list-entries=200" not in result:
                        raise RuntimeError("missing benchmark metadata: " + result)
                    row = {"trial": trial, "profile": profile,
                           "version": version[1], "eclock": int(version[2]),
                           "geometry": geometry[0], "ticks": ticks,
                           "host_seconds": round(time.monotonic() - started, 2)}
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
