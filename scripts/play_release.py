#!/usr/bin/env python3
"""Push the store listing, and promote a build, on Google Play.

Everything between "the AAB is uploaded" and "the build is in front of users" is
API-writable, so none of it has to be done by hand in the Play Console:

    listing   title / short / full description + every screenshot
    release   put an already-uploaded versionCode on a track, at a rollout %
    status    what is currently on each track

The one thing this cannot do is the paperwork: the Data safety form, the IARC
content rating and the target-audience declaration have no API and are one-time
console work. See android/play-assets/LISTING.md.

Play's edits are transactional: everything below opens an edit, changes it, and
commits once. Committing IS the submission -- there is no separate submit call,
which is why --dry-run stops before the commit rather than before the changes.

Credentials, first one set wins:
  $PLAY_SERVICE_ACCOUNT_JSON   the service-account JSON *content* (CI)
  $PLAY_PUBLISHER_KEY          path to the service-account JSON
  ~/.config/openklondike/play-publisher.json

Usage:
  play_release.py status
  play_release.py listing [--dry-run]
  play_release.py release --build N --track internal|alpha|beta|production \\
                          [--rollout 0.2] [--notes TEXT] [--dry-run]

`--build N` is the release number: the Makefile sets versionCode from it
(ANDROID_VERSION_CODE), so release-N is versionCode N on Play.
"""
import argparse
import base64
import json
import mimetypes
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

sys.path.insert(0, str(Path(__file__).resolve().parent))
from store_listing import ListingError, play_listing  # noqa: E402

PACKAGE = "com.danheskett.openklondike"
SCOPE = "https://www.googleapis.com/auth/androidpublisher"
API = "https://androidpublisher.googleapis.com/androidpublisher/v3/applications"
UPLOAD = "https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications"
LANG = "en-US"

REPO = Path(__file__).resolve().parent.parent
ASSETS = REPO / "android/play-assets"

# Play image slots -> what fills them. The tablet sets are captured at a real
# tablet shape rather than the phone frames scaled up, because this game lays the
# board out to the window: a tablet genuinely shows a different grid.
# The landscape folders beside these hold the same frames turned sideways; swap
# them in here if you would rather the listing showed the game that way.
IMAGES = {
    "icon":                 [ASSETS / "icon-512.png"],
    "featureGraphic":       [ASSETS / "feature-graphic-1024x500.png"],
    "phoneScreenshots":     sorted((ASSETS / "screenshots/phone").glob("*.png")),
    "sevenInchScreenshots": sorted((ASSETS / "screenshots/tablet").glob("*.png")),
    "tenInchScreenshots":   sorted((ASSETS / "screenshots/tablet").glob("*.png")),
}


def load_credential():
    raw = os.environ.get("PLAY_SERVICE_ACCOUNT_JSON")
    if raw:
        return json.loads(raw)
    path = os.environ.get("PLAY_PUBLISHER_KEY") or os.path.expanduser(
        "~/.config/openklondike/play-publisher.json")
    if not os.path.exists(path):
        sys.exit(f"no Play credential: set $PLAY_SERVICE_ACCOUNT_JSON or put a key at {path}")
    with open(path) as f:
        return json.load(f)


def access_token(sa):
    b64 = lambda b: base64.urlsafe_b64encode(b).rstrip(b"=")
    now = int(time.time())
    signing_input = b64(json.dumps({"alg": "RS256", "typ": "JWT"}).encode()) + b"." + b64(
        json.dumps({"iss": sa["client_email"], "scope": SCOPE, "aud": sa["token_uri"],
                    "iat": now, "exp": now + 3600}).encode())
    key = serialization.load_pem_private_key(sa["private_key"].encode(), password=None)
    assertion = (signing_input + b"." + b64(
        key.sign(signing_input, padding.PKCS1v15(), hashes.SHA256()))).decode()
    body = urllib.parse.urlencode({
        "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer",
        "assertion": assertion}).encode()
    with urllib.request.urlopen(urllib.request.Request(sa["token_uri"], data=body)) as r:
        return json.load(r)["access_token"]


class Play:
    def __init__(self, dry_run=False):
        self.token = access_token(load_credential())
        self.dry_run = dry_run
        self.edit = None

    def call(self, method, url, data=None, content_type="application/json", mutating=True):
        if self.dry_run and mutating:
            shown = ""
            if isinstance(data, (bytes, bytearray)) and content_type == "application/json":
                shown = " " + data.decode()[:400]
            elif isinstance(data, (bytes, bytearray)):
                shown = f" <{len(data)} bytes>"
            print(f"  [dry-run] {method} {url.replace(API, '')}{shown}")
            return {}
        headers = {"Authorization": "Bearer " + self.token}
        if data is not None:
            headers["Content-Type"] = content_type
        req = urllib.request.Request(url, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req) as r:
                body = r.read()
            return json.loads(body) if body else {}
        except urllib.error.HTTPError as e:
            sys.exit(f"HTTP {e.code} on {method} {url}\n{e.read().decode()[:1500]}")

    # -- edit lifecycle -----------------------------------------------------
    def open_edit(self):
        # A dry run still opens a real edit: it is the only way to read current
        # state, and an edit that is never committed changes nothing.
        r = self.call("POST", f"{API}/{PACKAGE}/edits", data=b"", mutating=False)
        self.edit = r["id"]
        return self.edit

    def commit(self):
        if self.dry_run:
            print("  [dry-run] edit NOT committed; nothing changed on Play")
            return
        self.call("POST", f"{API}/{PACKAGE}/edits/{self.edit}:commit", data=b"")
        print(f"  edit {self.edit} committed")

    def abandon(self):
        if self.edit and not self.dry_run:
            self.call("DELETE", f"{API}/{PACKAGE}/edits/{self.edit}", mutating=False)

    def base(self):
        return f"{API}/{PACKAGE}/edits/{self.edit}"


def cmd_status(play, args):
    play.open_edit()
    try:
        tracks = play.call("GET", f"{play.base()}/tracks", mutating=False)
        for t in tracks.get("tracks", []):
            print(f"{t['track']}:")
            for rel in t.get("releases", []):
                codes = ",".join(rel.get("versionCodes", []) or [])
                frac = rel.get("userFraction")
                rollout = f" rollout={frac}" if frac is not None else ""
                print(f"  {rel.get('status'):<12} versionCodes=[{codes}] "
                      f"name={rel.get('name', '-')}{rollout}")
    finally:
        play.abandon()
    return 0


def cmd_listing(play, args):
    try:
        text = play_listing()
    except ListingError as e:
        sys.exit(f"error: {e}")

    missing = [str(p) for files in IMAGES.values() for p in files if not p.exists()]
    if missing:
        sys.exit("missing listing images:\n  " + "\n  ".join(missing))

    play.open_edit()
    print(f"edit {play.edit}")
    try:
        play.call("PUT", f"{play.base()}/listings/{LANG}",
                  data=json.dumps({"language": LANG, **text}).encode())
        print(f"  text: title={len(text['title'])}ch short={len(text['shortDescription'])}ch "
              f"full={len(text['fullDescription'])}ch")

        for slot, files in IMAGES.items():
            # deleteall first: uploading alone appends, so re-running would stack
            # duplicate screenshots up against Play's 8-per-slot ceiling.
            play.call("DELETE", f"{play.base()}/listings/{LANG}/{slot}")
            for f in files:
                ctype = mimetypes.guess_type(f.name)[0] or "image/png"
                play.call("POST",
                          f"{UPLOAD}/{PACKAGE}/edits/{play.edit}/listings/{LANG}/{slot}"
                          f"?uploadType=media",
                          data=f.read_bytes(), content_type=ctype)
            print(f"  {slot}: {len(files)} image(s)")
        play.commit()
    except BaseException:
        play.abandon()
        raise
    return 0


def cmd_release(play, args):
    if args.track == "production" and args.rollout is None:
        print("note: no --rollout given, so this goes to 100% of production users",
              file=sys.stderr)
    notes = args.notes or f"release-{args.build}"

    release = {
        "versionCodes": [str(args.build)],
        "name": f"release-{args.build}",
        "releaseNotes": [{"language": LANG, "text": notes}],
    }
    if args.rollout is not None:
        release["status"] = "inProgress"
        release["userFraction"] = args.rollout
    else:
        release["status"] = "completed"

    play.open_edit()
    print(f"edit {play.edit}")
    try:
        # Fail early and clearly if that versionCode was never uploaded, rather
        # than letting the commit fail with a less obvious message.
        if not play.dry_run:
            bundles = play.call("GET", f"{play.base()}/bundles", mutating=False)
            codes = {b["versionCode"] for b in bundles.get("bundles", [])}
            if args.build not in codes:
                sys.exit(f"versionCode {args.build} is not uploaded "
                         f"(uploaded: {sorted(codes)}); the release workflow uploads it "
                         f"to the internal track on every merge to main")

        play.call("PUT", f"{play.base()}/tracks/{args.track}",
                  data=json.dumps({"track": args.track, "releases": [release]}).encode())
        print(f"  track {args.track}: versionCode {args.build}, status {release['status']}"
              + (f", rollout {args.rollout}" if args.rollout is not None else ""))
        # Committing an edit is what submits it for review, so there is no
        # separate submit step to gate.
        play.commit()
    except BaseException:
        play.abandon()
        raise
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="verb", required=True)

    sub.add_parser("status", help="what is on each track")

    p_listing = sub.add_parser("listing", help="push listing text + images")
    p_listing.add_argument("--dry-run", action="store_true")

    p_rel = sub.add_parser("release", help="put an uploaded build on a track")
    p_rel.add_argument("--build", type=int, required=True, help="release number = versionCode")
    p_rel.add_argument("--track", required=True,
                       choices=["internal", "alpha", "beta", "production"])
    p_rel.add_argument("--rollout", type=float,
                       help="staged rollout fraction 0-1; omit for a full release")
    p_rel.add_argument("--notes", help="release notes; defaults to release-N")
    p_rel.add_argument("--dry-run", action="store_true")

    args = ap.parse_args()
    rollout = getattr(args, "rollout", None)
    if rollout is not None and not 0 < rollout <= 1:
        sys.exit("--rollout must be in (0, 1]")

    play = Play(dry_run=getattr(args, "dry_run", False))
    return {"status": cmd_status, "listing": cmd_listing, "release": cmd_release}[args.verb](
        play, args)


if __name__ == "__main__":
    sys.exit(main())
