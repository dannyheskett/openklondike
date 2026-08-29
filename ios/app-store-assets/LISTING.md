# openklondike — App Store listing

Copy/paste into App Store Connect. Mirrors `android/play-assets/LISTING.md`,
with the differences Apple requires (subtitle, keywords, promotional text).

One rule that differs from Play: **never mention Android, Google Play, or
another platform** in the description. Apple rejects listings that reference
competing stores.

## New App form (My Apps → + → New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `openklondike` (must be unique App Store-wide; see fallbacks below) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.openklondike` |
| SKU | `openklondike` |
| User Access | Full Access |

If `openklondike` is taken, in order of preference: `Openklondike Solitaire`,
`Openklondike Patience`, `Openklondike Cards`. The name is public, capped at 30
characters, and can be changed with any later version — the SKU and bundle ID
cannot.

## Subtitle (≤30 chars)

```
Classic Klondike solitaire
```

## Promotional text (≤170 chars)

Editable anytime without submitting a new build — use it for release notes or
seasonal copy.

```
Klondike solitaire. Free and open source. No ads, no tracking, no accounts.
```

## Keywords (≤100 chars, comma-separated, no spaces after commas)

Do not repeat the app name — it is already indexed.

```
solitaire,klondike,patience,cards,card game,classic,offline,draw three,free cell,deck
```

## Description (≤4000 chars)

```
The Klondike solitaire you already know. Seven tableau columns, four foundations, draw one or draw three, standard scoring with a clock and a move count. Build the foundations Ace to King to win.

No ads. No tracking. No accounts. No in-app purchases. openklondike never touches the network. It runs in airplane mode.

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

## App information

- **Category (primary):** Games → Card
- **Category (secondary):** Games → Puzzle
- **Content Rights:** does not contain third-party content
- **Age Rating:** answer "None" to every question → **4+**. The gambling
  questions are a genuine "no": there is no wagering, no simulated gambling,
  and no currency of any kind.
- **Copyright:** `2026 Daniel Heskett`
- **Support URL:** https://danheskett.com
- **Marketing URL:** https://danheskett.com/projects/openklondike/
- **Privacy Policy URL:** https://danheskett.com/app/privacy-policy/

## App Privacy (App Store Connect → App Privacy)

Answer **"No, we do not collect data from this app."** — accurate and
verifiable: no network code, no analytics SDK, no permissions requested. This
yields a "Data Not Collected" privacy label.

## Pricing

Free. No in-app purchases. Requires the **Free Applications agreement** to be
Active under Business / Agreements, Tax, and Banking.

## Export compliance

openklondike uses no encryption of any kind. `ios/Info.plist` already carries
`ITSAppUsesNonExemptEncryption = false`, so App Store Connect will not ask the
encryption question on each upload.
