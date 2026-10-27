# Getting openklondike into TestFlight

The release workflow already knows how to sign an `.ipa` and upload it to App
Store Connect. It does neither today because the secrets are absent — every
Apple step gates on them and reports "skipping". This is the one-time setup that
turns it on.

**You do not need a Mac.** The certificate is created with `openssl`, the
provisioning profile is downloaded from Apple's website, and the build, signing
and upload all happen on GitHub's macOS runners. That is the whole reason this
is automated.

**You do need a paid Apple Developer Program membership** ($99/year). A free
Apple ID can sideload to your own device but cannot use TestFlight.

Throughout: bundle ID is `com.danheskett.openklondike`, and it must match
`ios/Info.plist` exactly.

---

## 1. Register the App ID

[developer.apple.com](https://developer.apple.com/account) → **Certificates,
Identifiers & Profiles** → **Identifiers** → **+**

- Type: **App IDs** → **App**
- Description: `openklondike`
- Bundle ID: **Explicit** → `com.danheskett.openklondike`
- Capabilities: leave everything off. The app uses none.

While you are here, note your **Team ID** (Membership Details, a 10-character
string like `A1B2C3D4E5`). That is `IOS_TEAM_ID`.

## 2. Create the distribution certificate (no Mac)

Generate a private key and a certificate signing request locally:

```sh
openssl genrsa -out ios_distribution.key 2048
openssl req -new -key ios_distribution.key -out ios_distribution.csr \
  -subj "/emailAddress=dan@danheskett.com/CN=Danny Heskett/C=US"
```

developer.apple.com → **Certificates** → **+** → **Apple Distribution**.

> ⚠️ It must be **Apple Distribution**, not the legacy "iOS Distribution
> (App Store and Ad Hoc)". The release workflow picks the signing identity with
> `awk '/Apple Distribution/'`, and the legacy certificate's common name starts
> with "iPhone Distribution" instead, so signing would fail to find an identity.

Upload `ios_distribution.csr`, download the resulting `distribution.cer`, then
bundle it with the private key into a `.p12`:

```sh
openssl x509 -in distribution.cer -inform DER -out distribution.pem -outform PEM
openssl pkcs12 -export \
  -inkey ios_distribution.key -in distribution.pem \
  -out distribution.p12 -name "Apple Distribution" \
  -passout pass:CHOOSE_A_PASSWORD
```

Keep `distribution.p12`, the password, and `ios_distribution.key` somewhere
safe and backed up. Generate them **outside the repo** — `~/apple-signing/` is
a good home. `.gitignore` covers `*.key`, `*.csr`, `*.cer`, `*.pem`, `*.p12` and
`*.p8` as a backstop, but the private key should never be inside the working
tree in the first place.

## 3. Create the App Store provisioning profile

developer.apple.com → **Profiles** → **+**

- Type: **App Store Connect** (under Distribution)
- App ID: `com.danheskett.openklondike`
- Certificate: the Apple Distribution certificate from step 2

Download it as `profile.mobileprovision`.

## 4. Create the app record in App Store Connect

[appstoreconnect.apple.com](https://appstoreconnect.apple.com) → **Apps** →
**+** → **New App**

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `openklondike` (see [LISTING.md](LISTING.md) for fallbacks if taken) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.openklondike` |
| SKU | `openklondike` |

This record must exist **before** the first upload: `altool` uploads *to* an
app, and `scripts/testflight_notes.py` looks the app up by bundle ID.

For TestFlight internal testing you do **not** need screenshots, a description,
or a privacy policy yet. Those are for submitting to the store — the copy is
ready in [LISTING.md](LISTING.md) when you get there.

## 5. Create an App Store Connect API key

App Store Connect → **Users and Access** → **Integrations** → **App Store
Connect API** → **+**

- Name: `openklondike CI`
- Access: **App Manager**

Download `AuthKey_XXXXXXXXXX.p8`. **Apple lets you download it exactly once.**
Note the **Key ID** (in the filename and the table) and the **Issuer ID** (shown
above the table, a UUID).

## 6. Set the seven repo secrets

Note the encodings — they are not all the same. The two binary files are
base64'd because a GitHub secret is text; the `.p8` is already text and is
stored raw, because the workflow writes it straight back out as a PEM file.

```sh
base64 -w0 distribution.p12        | gh secret set IOS_CERT_P12
gh secret set IOS_CERT_PASSWORD    -b 'CHOOSE_A_PASSWORD'        # from step 2
base64 -w0 profile.mobileprovision | gh secret set IOS_PROVISIONING_PROFILE
gh secret set IOS_TEAM_ID          -b 'A1B2C3D4E5'               # from step 1

gh secret set ASC_KEY_P8 < AuthKey_XXXXXXXXXX.p8                 # raw, NOT base64
gh secret set ASC_KEY_ID     -b 'XXXXXXXXXX'
gh secret set ASC_ISSUER_ID  -b '69a6de70-....-....-....-........'
```

(Or add them in the GitHub UI: **Settings → Secrets and variables → Actions →
New repository secret**.)

The two groups gate independently:

- `IOS_CERT_P12` + `IOS_CERT_PASSWORD` + `IOS_PROVISIONING_PROFILE` +
  `IOS_TEAM_ID` → `build-ios` (in `build.yml`) produces the **signed** `.ipa`.
  They are required: a missing one fails the build.
- `ASC_KEY_P8` + `ASC_KEY_ID` + `ASC_ISSUER_ID` → the `release` workflow
  validates and uploads it, attaches TestFlight notes, and submits it to App
  Review. A missing one fails the release.

## 7. Cut a release

Merge `staging` into `main`: `build.yml` signs the `.ipa`, verifies it is App
Store-shaped, tags `release-N` and attaches it to the GitHub Release. Then run
the **release** workflow for that tag:

1. `appstore-upload` downloads the `.ipa` from the GitHub Release, runs
   `altool --validate-app` (its errors are far better than the upload path's)
   and then `--upload-app` -- or skips the upload, saying so, if App Store
   Connect already has that build number.
2. Apple processes the build for 5–15 minutes.
3. `appstore-submit` polls until the build is `VALID`, writes "What to Test",
   checks the internal tester group gets it, withdraws any earlier version
   still pending App Review, submits version 1.0.N, and reads it back.

## 8. Add internal testers

App Store Connect → your app → **TestFlight** → **Internal Testing** → **+** on
Testers or Groups.

Internal testers must be Users on your App Store Connect account (up to 100).
**Internal testing needs no Beta App Review**, so builds are installable as soon
as processing finishes. External testers do need review — typically a day or
two, and only once per major version.

---

## What the repo already handles

These are the upload rejections that cost people the most time, and they are all
dealt with in `Makefile` and `scripts/gen_icons.py`:

| Problem | Handled by |
| --- | --- |
| ITMS-90713, missing top-level `CFBundleIconName` | `plutil -replace` after `actool` |
| App icon rejected for having an alpha channel | `gen_icons.py` flattens the iOS icon to RGB |
| "Built with a beta version of Xcode" | `DTXcode` / `DTXcodeBuild` / `DTSDK*` injected from the runner's toolchain |
| Export-compliance question on every upload | `ITSAppUsesNonExemptEncryption = false` in `Info.plist` |
| "Build must be built with the iOS 26 SDK or later" | `build-ios` fails early with a clear message if the runner's SDK is older |
| `CFBundleVersion` must increase per upload | tracks the `release-N` number automatically |
| Missing `CFBundleSupportedPlatforms` | injected via `PlistBuddy` |

## Troubleshooting

**"no Apple Distribution identity found in the keychain"** — the certificate is
the legacy iOS Distribution type, or the `.p12` did not include the private key.
Redo step 2, making sure `openssl pkcs12 -export` got both `-inkey` and `-in`.

**"no app found for bundle id ..."** from `appstore-submit` — the App Store
Connect app record (step 4) does not exist, or its bundle ID does not match
`ios/Info.plist`.

**Upload rejected: "The provided entity includes an attribute with an invalid
value"** — usually a duplicate `CFBundleVersion`. Each upload needs a higher
number; cut a new release rather than re-running the old one.

**The release fails at "Require the App Store Connect secrets"** — one of the
three `ASC_*` secrets is missing; the error names it. (A missing `IOS_*`
secret fails `build.yml` instead, so an unsigned `.ipa` never reaches a
release.)
