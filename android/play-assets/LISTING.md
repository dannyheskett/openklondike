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

## App name (≤30 chars)

```
openklondike
```

## Short description (≤80 chars)

```
Classic Klondike solitaire. Free, open source, no ads, no tracking.
```

## Full description (≤4000 chars)

```
The Klondike solitaire you already know, done properly — and with none of the junk that has crept into this genre.

No ads. No tracking. No accounts. No in-app purchases. openklondike requests zero permissions and never touches the network. It's just the game.

THE GAME YOU REMEMBER
• Build the four foundations from Ace to King to win
• Draw One or Draw Three, your choice, from the menu
• Standard scoring, with the clock and move count along the bottom
• The bouncing-cards cascade when you win

CONTROLS MADE FOR A TOUCHSCREEN
• Tap the deck to deal, tap it again when it's empty to redeal
• Tap any card to send it home — no hunting for a tiny foundation
• Drag a card, or a whole run, and it lifts clear of your finger so you can see it
• The pile you're about to drop on lights up before you let go
• Two-finger tap for the menu

BUILT RIGHT
• Cards sized to your screen, from a small phone to a tablet
• Crisp, minimal visuals that stay out of your way
• Fully offline — perfect for flights, commutes, anywhere
• Tiny download, easy on your battery

FREE AND OPEN SOURCE
openklondike is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/openklondike

No dark patterns, no "coins", no paywalled decks. Just Klondike.
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
