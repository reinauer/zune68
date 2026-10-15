#!/usr/bin/env python3
"""Run perfcensus in the disposable fixture used by compare_toolkits.py.

Each (profile, mode) pair boots a pristine OS disk, installs the toolkit's
Libs: and runs Bench:perfcensus <mode>. Transcripts land in the output
directory as <trial>-<profile>-<mode>.txt next to results.json.
"""
import argparse
import fcntl
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

from compare_toolkits import digest, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture", "emulator", "binary", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--profiles", nargs="+", required=True)
    parser.add_argument("--modes", nargs="+",
                        choices=("plain", "count", "time", "trace"),
                        default=["plain", "count", "time", "trace"])
    parser.add_argument("--trials", type=int, default=1)
    parser.add_argument("--start-trial", type=int, default=0)
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()
    if args.trials < 1 or args.start_trial < 0 or len(set(args.profiles)) != len(args.profiles) or any(
            not re.fullmatch(r"[a-z0-9_-]+", p) for p in args.profiles):
        parser.error("positive trials and unique lowercase profile names required")
    root = args.fixture.resolve()
    guest = root / "guest"
    config, pristine, disk = (root / n for n in
                             ("run.uae", "pristine.hdf", "system.hdf"))
    if str(disk) not in config.read_text():
        parser.error("config must reference the disposable system.hdf")
    for profile in args.profiles:
        if not (guest / profile / "Libs/muimaster.library").is_file():
            parser.error("missing muimaster.library: " + profile)
    args.output.mkdir(parents=True, exist_ok=True)
    metadata = {"binary_sha256": digest(args.binary),
                "config_sha256": digest(config),
                "emulator_sha256": digest(args.emulator),
                "pristine_disk_sha256": digest(pristine),
                "profiles": {p: manifest(guest / p) for p in args.profiles},
                "runs": []}
    with (root / "runner.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        saved = {p: p.read_bytes() if p.exists() else None for p in
                 (guest / "run-tests", guest / "perfcensus")}
        try:
            shutil.copy2(args.binary, guest / "perfcensus")
            for trial in range(args.start_trial, args.start_trial + args.trials):
                offset = trial % len(args.profiles)
                for profile in args.profiles[offset:] + args.profiles[:offset]:
                    for mode in args.modes:
                        if manifest(guest / profile) != metadata["profiles"][profile]:
                            raise RuntimeError("profile files changed")
                        shutil.copy2(pristine, disk)
                        result_path = guest / "census-result.txt"
                        result_path.write_text("")
                        (guest / "run-tests").write_text(
                            "FailAt 21\nStack 131072\n"
                            f"Assign MUI: Bench:{profile}\n"
                            f"Assign LIBS: Bench:{profile}/Libs\n"
                            "Assign LIBS: SYS:Libs ADD\nAssign LIBS: SYS:Classes ADD\n"
                            f"Bench:perfcensus {mode} >Bench:census-result.txt\n"
                            "Echo ALLDONE >>Bench:census-result.txt\n")
                        stem = f"{trial}-{profile}-{mode}"
                        print("starting " + stem, flush=True)
                        with (args.output / (stem + ".log")).open("w") as log:
                            proc = subprocess.Popen([str(args.emulator), "-f",
                                str(config), "-s", "use_gui=no", "-s",
                                "debugging_features=none"], stdout=log, stderr=log)
                            try:
                                deadline = time.monotonic() + args.timeout
                                while time.monotonic() < deadline:
                                    result = result_path.read_text(errors="replace")
                                    if "ALLDONE" in result:
                                        break
                                    if proc.poll() is not None:
                                        (args.output / (stem + ".partial.txt")).write_text(result)
                                        raise RuntimeError(f"emulator exited: {proc.returncode}")
                                    time.sleep(1)
                                else:
                                    raise RuntimeError("guest timed out: " + result[-1000:])
                            finally:
                                proc.terminate()
                                try:
                                    proc.wait(timeout=5)
                                except subprocess.TimeoutExpired:
                                    proc.kill()
                                    proc.wait()
                        (args.output / (stem + ".txt")).write_text(result)
                        key = "timing " if mode == "plain" else "phase "
                        if "rc=0" not in result or result.count(key) != 6:
                            raise RuntimeError("incomplete census: " + result[-1000:])
                        metadata["runs"].append({"trial": trial, "profile": profile,
                            "mode": mode, "transcript": stem + ".txt",
                            "sha256": digest(args.output / (stem + ".txt"))})
                        (args.output / "results.json").write_text(
                            json.dumps(metadata, indent=2) + "\n")
                        print(stem + ": " + "; ".join(line for line in result.splitlines()
                              if line.startswith(("timing ", "phase ", "geometry", "calibration"))),
                              flush=True)
        finally:
            for path, contents in saved.items():
                if contents is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(contents)


if __name__ == "__main__":
    main()
