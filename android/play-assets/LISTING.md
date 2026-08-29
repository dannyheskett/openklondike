# openklondike — Google Play store listing

Copy/paste these into the Play Console (**Grow → Store presence → Main store
listing**, plus **Store settings** for category). The images in this folder are
generated, not hand-made: `scripts/gen_icons.py` draws the icon and feature
graphic from the same palette `src/render.c` uses, and
`scripts/gen_store_screenshots.mjs` captures the screenshots from the real web
build. Rerun either to regenerate.

Nothing here is wired up yet — no Play Console app record exists and none of the
`PLAY_*` secrets are set, so the release workflow ships the sideload APK and
skips the bundle. See [KEYSTORE.md](KEYSTORE.md) for what turns it on.

## Assets (this folder)

| File | Play field | Spec |
|------|-----------|------|
| `icon-512.png` | App icon | 512×512 PNG (32-bit) |
| `feature-graphic-1024x500.png` | Feature graphic | 1024×500 PNG/JPG |
| `screenshots/phone/` | Phone screenshots | 4× 1080×1920 PNG (9:16, promo-eligible) |
| `screenshots/tablet/` | 7-inch and 10-inch tablet screenshots | 4× 2160×3840 PNG (9:16, same files fit both slots) |
| `screenshots/phone-landscape/` | Phone screenshots, sideways | 4× 1920×1080 PNG (16:9) |
| `screenshots/tablet-landscape/` | Tablet screenshots, sideways | 4× 3840×2160 PNG (16:9) |

Play accepts either orientation in a slot; pick one set per slot rather than
mixing. The game supports both, so either is honest.

## App name (≤30 chars)

```
openklondike
```

## Short description (≤80 chars)

```
Klondike solitaire. Free, open source, no ads, no tracking.
```

## Full description (≤4000 chars)

```
The Klondike solitaire you already know. Seven tableau columns, four foundations, draw one or draw three, standard scoring with a clock and a move count. Build the foundations Ace to King to win.

No ads. No tracking. No accounts. No in-app purchases. openklondike requests no permissions and never touches the network.

THE GAME
- Draw One or Draw Three, set in Options
- Standard scoring, with the clock and move count along the bottom
- The bouncing-cards cascade when you win

TOUCH CONTROLS
- Tap the deck to deal, tap it again when empty to redeal
- Tap a card to send it to a foundation, or to the best tableau build
- Drag a card or a run. It lifts clear of your finger, and the pile it would land on is highlighted
- Two-finger tap for the menu
- Portrait and landscape; the board re-fits when you rotate

BUILT
- Cards sized to your screen, from a small phone to a tablet
- Works offline
- Small download

OPEN SOURCE
openklondike is MIT licensed. Read the code or build it yourself: https://github.com/dannyheskett/openklondike
```

## Categorization (Store settings)

- **App or game:** Game
- **Category:** Card
- **Tags:** card, solitaire, patience, klondike, offline
- **Email:** dan@danheskett.com
- **Website:** https://danheskett.com
- **Content rating:** Everyone (no objectionable content; IARC questionnaire —
  answer "no" to all violence/adult items). Note the gambling question: the
  answer is **no**. There is no wagering, no simulated gambling, and no
  currency of any kind.

## Data safety (Policy → App content)

- Data collected: **None**
- Data shared: **None**
- App has no `INTERNET` permission (verify in `android/AndroidManifest.xml`) →
  "no data transmitted off the device" is truthful.
- Privacy policy URL: **https://danheskett.com/app/privacy-policy/**

## Screenshots

`screenshots/phone/` (4× 1080×1920) in the phone slot, `screenshots/tablet/`
(4× 2160×3840) in both the 7-inch and 10-inch tablet slots. Captured from the
web build's touch board, which compiles the same renderer Android does, so they
are real frames rather than mockups. Regenerate with:

```sh
./scripts/build_raylib_web.sh && make web
node scripts/gen_store_screenshots.mjs
```
