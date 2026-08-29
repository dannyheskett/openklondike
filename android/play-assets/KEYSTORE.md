# Upload keystore + release secrets (Google Play)

The release workflow builds an uploadable `.aab` **only when the
`PLAY_UPLOAD_KEYSTORE` secret is present**. This is a one-time setup you run
locally (the private key must never live in the repo or in CI logs).

With **Play App Signing** (recommended, and the default for new apps), Google
holds the real _app signing key_; you sign uploads with this _upload key_. If
you ever lose the upload key, Google can reset it — but still back it up.

## 1. Generate the upload key (run locally)

```sh
keytool -genkeypair -v \
  -keystore openklondike-upload.jks \
  -alias openklondike-upload \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -dname "CN=Danny Heskett, O=openklondike, C=US"
```

It prompts for a **keystore password** and a **key password** — pick strong
ones and save them in your password manager. Keep `openklondike-upload.jks`
somewhere safe and backed up. **Do not commit it** (it's outside the repo by
design; `.gitignore` already ignores `*.jks`/`*.keystore` — verify before
adding anything).

## 2. Set the four repo secrets

Using the GitHub CLI (replace the passwords with what you chose above):

```sh
base64 -w0 openklondike-upload.jks > upload.jks.b64
gh secret set PLAY_UPLOAD_KEYSTORE   < upload.jks.b64
gh secret set PLAY_KEY_ALIAS         -b "openklondike-upload"
gh secret set PLAY_KEYSTORE_PASSWORD -b "<the keystore password>"
gh secret set PLAY_KEY_PASSWORD      -b "<the key password>"
rm -f upload.jks.b64        # don't leave the base64 lying around
```

(Or add them in the GitHub UI: **Settings → Secrets and variables → Actions →
New repository secret**.)

## 3. That's it

The next release (any push to `main`, or a manual `release` run) will detect the
secret and produce `openklondike-<version>-android.aab` alongside the sideload
APK. Download it from the release assets (or the `android` CI artifact) and
upload it in the Play Console.

## Note on Play App Signing enrollment

The first time you upload an AAB, Play offers to manage signing. Accept **Play
App Signing** and let Google generate/hold the app signing key; your uploads
continue to use the upload key created above.
