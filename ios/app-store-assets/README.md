# App Store assets

Artwork and listing copy for the iOS App Store, mirroring `android/play-assets/`
for Google Play. Nothing here is compiled into the app — the in-bundle app icon
lives in `ios/Assets.xcassets`, not in this folder.

Nothing here is wired up yet: there is no App Store Connect record and none of
the `IOS_*` / `ASC_*` secrets are set, so the release workflow produces an
unsigned `.ipa` and skips the TestFlight upload. **[TESTFLIGHT.md](TESTFLIGHT.md)
is the step-by-step for turning it on** — no Mac required.

## Required

| Asset | Size | Notes |
| --- | --- | --- |
| `icon-1024.png` | 1024×1024 | **No alpha channel, no transparency, no rounded corners.** Apple masks the corners itself; a submitted icon with an alpha channel is rejected outright. `scripts/gen_icons.py` flattens it to RGB for exactly this reason. |
| `screenshots/iphone-6.9/` | 1290×2796 | Required. 1–10 images, portrait. Covers every current iPhone; Apple scales this set down for older devices, so no other iPhone size is needed. |

openklondike ships **iPhone only** (`UIDeviceFamily = [1]` in `ios/Info.plist`),
so no iPad screenshots are needed. iPads can still install and run it scaled. If
iPad is ever declared, add `2` to `UIDeviceFamily`, set `UIRequiresFullScreen`
to opt out of Split View, and add a `screenshots/ipad-13/` set at 2064×2752 —
Apple requires that set whenever iPad is supported.

Screenshots must be PNG or JPEG, sRGB, with no alpha channel.
`scripts/gen_store_screenshots.mjs` rewrites the browser's RGBA output as plain
RGB before saving, so the committed files already satisfy this. They are ordered
in the listing by filename, hence the `01-`/`02-` prefixes.

## Generating the assets

```sh
python3 scripts/gen_icons.py                   # icon-1024.png
./scripts/build_raylib_web.sh && make web      # the bundle the shots come from
node scripts/gen_store_screenshots.mjs         # screenshots/iphone-6.9/
```

The screenshots come from the web build because it compiles the same
`src/render*.c` the iOS build does and picks the touch board at runtime from
`matchMedia('(pointer: coarse)')`, which a headless browser can simply be told
is true. So they are real frames from the real renderer, capturable without a
Mac. The traps that cost time getting there are documented at the top of the
script.
