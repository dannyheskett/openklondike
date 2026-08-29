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
No ads, no tracking, no accounts. Just Klondike solitaire, free and open source.
```

## Keywords (≤100 chars, comma-separated, no spaces after commas)

Do not repeat the app name — it is already indexed.

```
solitaire,klondike,patience,cards,card game,classic,offline,draw three,free cell,deck
```

## Description (≤4000 chars)

```
The Klondike solitaire you already know, done properly — and with none of the junk that has crept into this genre.

No ads. No tracking. No accounts. No in-app purchases. openklondike never touches the network. It's just the game. You can play it in airplane mode.

THE GAME YOU REMEMBER
• Build the four foundations from Ace to King to win
• Draw One or Draw Three, your choice, from the Options screen
• Standard scoring, with the clock and move count along the bottom
• The bouncing-cards cascade when you win

CONTROLS MADE FOR A TOUCHSCREEN
• Tap the deck to deal, tap it again when it's empty to redeal
• Tap any card to send it home — no hunting for a tiny foundation
• Drag a card, or a whole run, and it lifts clear of your finger so you can see it
• The pile you're about to drop on lights up before you let go
• Two-finger tap for the menu

BUILT RIGHT
• Cards sized to your screen, from the smallest iPhone up
• Crisp, minimal visuals that stay out of your way
• Tiny download, easy on your battery

FREE AND OPEN SOURCE
openklondike is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/openklondike

No dark patterns, no "coins", no paywalled decks. Just Klondike.
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
