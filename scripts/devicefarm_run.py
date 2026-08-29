#!/usr/bin/env python3
"""Upload the openklondike app (.apk or .ipa) to AWS Device Farm and run a Fuzz
test on a real device, reporting pass/fail. Used by the devicefarm CI workflow.

openklondike is a native game with no tappable UI widgets, so a scripted
Appium/Espresso/XCUITest test would give little; the built-in Fuzz test (random
taps) validates that the app launches and runs without crashing on a real
device. The platform (Android vs iOS) is inferred from the app file extension;
the unsigned iOS .ipa works because Device Farm re-signs apps on upload.

A second mode validates the fixed-timestep engine on real hardware: an app
built with SIMSTATS=1 (autoplay + per-second frames/steps logging) is run with
a gentle fuzz, then the device logcat is parsed and the run fails unless the
simulation held ~60 steps/s at whatever refresh rate the display delivered.
Pick a 120 Hz device (DEVICEFARM_DEVICE_MODEL) to exercise the high-refresh
path; the report states the render rate actually observed either way.

Config via env (no ARNs are hard-coded, so this file is safe to commit):
  DEVICEFARM_PROJECT_ARN     required  the Device Farm project ARN
  APP_PATH                   required  path to the .apk or .ipa to test
  DEVICEFARM_UPLOAD_ONLY     optional  "1" to stop after upload (free; no devices)
  DEVICEFARM_MAX_DEVICES     optional  device count for the run (default 1)
  DEVICEFARM_DEVICE_MODEL    optional  MODEL substring filter, e.g. "Galaxy S23"
  DEVICEFARM_CHECK_SIMSTATS  optional  "1" to assert SIMSTATS logcat lines
                                       (Android; requires a SIMSTATS=1 build)
  AWS_REGION                 optional  defaults to us-west-2
"""
import os
import re
import shutil
import sys
import time
import urllib.request

import boto3

REGION = os.environ.get("AWS_REGION", "us-west-2")
PROJECT = os.environ["DEVICEFARM_PROJECT_ARN"]
APP_PATH = os.environ["APP_PATH"]
UPLOAD_ONLY = os.environ.get("DEVICEFARM_UPLOAD_ONLY") == "1"
MAX_DEVICES = int(os.environ.get("DEVICEFARM_MAX_DEVICES", "1"))
DEVICE_MODEL = os.environ.get("DEVICEFARM_DEVICE_MODEL", "").strip()
CHECK_SIMSTATS = os.environ.get("DEVICEFARM_CHECK_SIMSTATS") == "1"

if APP_PATH.endswith(".apk"):
    PLATFORM, UPLOAD_TYPE = "ANDROID", "ANDROID_APP"
elif APP_PATH.endswith(".ipa"):
    PLATFORM, UPLOAD_TYPE = "IOS", "IOS_APP"
else:
    sys.exit(f"APP_PATH must be a .apk or .ipa, got: {APP_PATH}")

df = boto3.client("devicefarm", region_name=REGION)


def poll(fn, done, desc, timeout=1800, interval=10):
    start = time.time()
    while True:
        obj = fn()
        status = obj["status"]
        if done(obj):
            return obj
        if time.time() - start > timeout:
            sys.exit(f"timed out waiting for {desc} (last status {status})")
        print(f"  {desc}: {status} ...", flush=True)
        time.sleep(interval)


def paginate(fn, key, **kw):
    """Yield every item across all pages. Device Farm's list_* APIs page at 50.

    max_devices is user-settable and yields one job per device, so an unpaginated
    walk silently measured only the first page while the assertions below spoke as
    though they had seen everything -- and a DEVICE_LOG landing past a page
    boundary failed the run with "was the app built with SIMSTATS=1?", blaming the
    build for a pagination bug.
    """
    token = None
    while True:
        page = fn(**kw, nextToken=token) if token else fn(**kw)
        yield from page[key]
        token = page.get("nextToken")
        if not token:
            return


def download_media_artifacts(run_arn, out_dir):
    """Download the run's screenshots and video (skipping logs) so CI can upload
    them as a workflow artifact for visual inspection."""
    os.makedirs(out_dir, exist_ok=True)
    n = 0
    for job in paginate(df.list_jobs, "jobs", arn=run_arn):
        for suite in paginate(df.list_suites, "suites", arn=job["arn"]):
            sname = suite["name"].replace(" ", "_")
            for test in paginate(df.list_tests, "tests", arn=suite["arn"]):
                for atype in ("SCREENSHOT", "FILE"):
                    for a in paginate(df.list_artifacts, "artifacts", arn=test["arn"], type=atype):
                        ext = (a.get("extension") or "").lower()
                        if ext not in ("mp4", "png", "jpg", "jpeg"):
                            continue
                        name = a["name"].replace(" ", "_")
                        fn = os.path.join(out_dir, f"{sname}-{name}.{ext}")
                        # urlretrieve has no timeout parameter, so a stalled
                        # transfer hung until the job timeout.
                        with urllib.request.urlopen(a["url"], timeout=120) as r, \
                                open(fn, "wb") as out:
                            shutil.copyfileobj(r, out)
                        n += 1
    print(f"downloaded {n} media artifact(s) to {out_dir}", flush=True)


# One line per second of continuous play, from the SIMSTATS=1 app build
# (src/main.c simstats_count): total window span, rendered frames, sim steps.
SIMSTATS_RE = re.compile(r"SIMSTATS window=([0-9.]+) frames=(\d+) steps=(\d+)")


def check_simstats(run_arn):
    """Pull the device logs and assert the fixed-timestep engine held ~60 sim
    steps/s during continuous play, whatever the display's actual refresh rate.
    The rendered-frame rate is reported so the run states whether it really
    exercised a high-refresh (>60 Hz) display or should be retried on one."""
    windows = []  # (steps_per_sec, frames_per_sec) per logged play window
    for job in paginate(df.list_jobs, "jobs", arn=run_arn):
        for suite in paginate(df.list_suites, "suites", arn=job["arn"]):
            for test in paginate(df.list_tests, "tests", arn=suite["arn"]):
                for a in paginate(df.list_artifacts, "artifacts", arn=test["arn"], type="LOG"):
                    if a["type"] != "DEVICE_LOG":
                        continue
                    text = urllib.request.urlopen(a["url"], timeout=120).read().decode("utf-8", "replace")
                    for m in SIMSTATS_RE.finditer(text):
                        span = float(m.group(1))
                        windows.append((int(m.group(3)) / span, int(m.group(2)) / span))
    if len(windows) < 10:
        sys.exit(f"SIMSTATS: only {len(windows)} play window(s) found in the device "
                 "log (need >= 10) — was the app built with SIMSTATS=1?")

    sps = sorted(w[0] for w in windows)
    fps = sorted(w[1] for w in windows)
    median_sps, median_fps = sps[len(sps) // 2], fps[len(fps) // 2]
    # Per-window tolerance is loose (±10%): a window may straddle a dropped
    # backlog (tick.c's spiral clamp) or a scheduler hiccup. The median must be
    # tight around 60.
    within = sum(1 for s in sps if 54.0 <= s <= 66.0)
    print(f"SIMSTATS: {len(windows)} play windows; sim {median_sps:.1f} steps/s median "
          f"(min {sps[0]:.1f}, max {sps[-1]:.1f}); render {median_fps:.1f} fps median "
          f"(max {fps[-1]:.1f})", flush=True)
    if median_fps > 90.0:
        print("SIMSTATS: high-refresh display exercised (>90 fps rendered)", flush=True)
    else:
        print("SIMSTATS: NOTE — device rendered <= 90 fps, so this run did not "
              "exercise the high-refresh path; retry with DEVICEFARM_DEVICE_MODEL "
              "set to a 120 Hz phone (e.g. 'Galaxy S23' or 'Pixel 8')", flush=True)
    if not (57.0 <= median_sps <= 63.0) or within < 0.8 * len(windows):
        sys.exit(f"SIMSTATS FAIL: simulation rate drifted from 60 steps/s "
                 f"(median {median_sps:.2f}; {within}/{len(windows)} windows within ±10%)")
    print("SIMSTATS PASS: simulation held ~60 steps/s", flush=True)


def main():
    # 1) Register an upload slot and PUT the app to the presigned URL.
    up = df.create_upload(
        projectArn=PROJECT, name=os.path.basename(APP_PATH), type=UPLOAD_TYPE
    )["upload"]
    with open(APP_PATH, "rb") as f:
        data = f.read()
    req = urllib.request.Request(
        up["url"], data=data, method="PUT",
        headers={"Content-Type": "application/octet-stream"},
    )
    urllib.request.urlopen(req, timeout=300).read()
    print(f"uploaded {len(data)} bytes ({APP_PATH})", flush=True)

    # 2) Wait for Device Farm to validate the app.
    up = poll(
        lambda: df.get_upload(arn=up["arn"])["upload"],
        lambda o: o["status"] in ("SUCCEEDED", "FAILED"),
        "upload",
        timeout=300,
    )
    if up["status"] != "SUCCEEDED":
        sys.exit(f"upload failed: {up.get('metadata')}")
    print("upload validated: SUCCEEDED", flush=True)

    if UPLOAD_ONLY:
        print("DEVICEFARM_UPLOAD_ONLY set — stopping before scheduling a run.")
        return

    # 3) Schedule a Fuzz run on real device(s) of the matching platform.
    filters = [
        {"attribute": "PLATFORM", "operator": "EQUALS", "values": [PLATFORM]},
        {"attribute": "AVAILABILITY", "operator": "EQUALS", "values": ["HIGHLY_AVAILABLE"]},
    ]
    if DEVICE_MODEL:
        # e.g. "Galaxy S23" / "Pixel 8" — lets the simstats mode target a
        # 120 Hz panel instead of whatever device happens to be free.
        filters.append({"attribute": "MODEL", "operator": "CONTAINS", "values": [DEVICE_MODEL]})

    test_spec = {"type": "BUILTIN_FUZZ"}
    if CHECK_SIMSTATS:
        # Measurement runs want mostly-uninterrupted autoplay, not a tap storm:
        # one fuzz event per second for ~4 minutes of gameplay to sample.
        test_spec["parameters"] = {"event_count": "250", "throttle": "1000"}

    run = df.schedule_run(
        projectArn=PROJECT,
        appArn=up["arn"],
        name=f"openklondike CI fuzz ({PLATFORM})",
        test=test_spec,
        deviceSelectionConfiguration={
            "filters": filters,
            "maxDevices": MAX_DEVICES,
        },
    )["run"]
    print(f"scheduled run: {run['arn']}", flush=True)

    # 4) Wait for completion and report.
    run = poll(
        lambda: df.get_run(arn=run["arn"])["run"],
        lambda o: o["status"] == "COMPLETED",
        "run",
    )
    c = run.get("counters", {})
    print(f"run result: {run['result']}  counters={c}", flush=True)

    art_dir = os.environ.get("DEVICEFARM_ARTIFACT_DIR")
    if art_dir:
        download_media_artifacts(run["arn"], art_dir)

    if run["result"] != "PASSED":
        sys.exit(f"Device Farm run did not pass: {run['result']}")

    if CHECK_SIMSTATS:
        check_simstats(run["arn"])


if __name__ == "__main__":
    main()
