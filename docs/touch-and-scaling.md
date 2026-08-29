# Touch and scaling: the decisions behind the mobile build

Notes on the four choices that shaped the Android / iOS / web port, kept here
because each of them contradicts something the desktop game had assumed, and the
reasoning is not obvious from the code alone.

## 1. The cards had to start scaling

The README used to state, as a design rule, that the cards are a fixed 80×112
and **never** scale — the window is resizable but only the margins move. That
rule works because the desktop window enforces a 704×704 minimum
(`SetWindowMinSize`), which is exactly wide enough for seven 80px columns plus
gaps and margins.

A phone cannot honour that. Seven fixed columns need 704 physical pixels, which
on a 1080-wide handset would fill the screen edge to edge and leave the cards
absurdly small relative to the display; on anything narrower it would not fit at
all. There is no arrangement of a Klondike board that avoids showing seven
columns at once.

So the rule is now scoped rather than dropped:

- **Where a minimum size can be enforced, the cards never scale.** Desktop keeps
  the fixed 80×112 board and the 704×704 window minimum.
- **Where it cannot, the board scales to fit.** The touch layout fits the seven
  columns to the live screen width. The web build's *fixed* layout also shrinks
  below the minimum, because a browser window is whatever the user drags it to
  and the alternative is a board running off the edge.

The scaled layout derives every metric from the resulting card width using the
same ratios the fixed layout hardcodes (gap 16/80, fans 28/80 and 12/80,
wordmark 22/80, and so on), so the two boards are one design at two sizes.
`tests/test_layout.c` pins both: that the desktop numbers are still exactly the
original ones, and that the touch board fits every device shape *inside its
margins* — a board that merely "fits" by running edge to edge has silently eaten
them, which is precisely the bug the margin assertion was written after finding.

## 2. Fans compress rather than overflow

The deepest column Klondike can produce is six face-down cards under a full
thirteen-card run. At the natural fan spacing that is 536px tall on the desktop
board, against 468px of space between the tableau and the status bar — so the
bottom of a deep column ran off the board even at the fixed size. The touch
board, with its wider fan, would be worse.

`tab_card_ys()` now compresses a column's fan uniformly until it fits, with a
floor that keeps a face-down card visible as a distinct sliver. Every caller —
drawing, hit testing, drop targeting — goes through that one function, so they
cannot disagree about where a card is, and hit testing measures each card's
exposed strip as the distance to the next card's top rather than recomputing the
step, so it stays correct under compression.

## 3. The touch grammar is the one card players already know

Klondike's desktop controls do not survive contact with a touchscreen: there is
no right-click, and double-tap is a poor gesture on a phone. Both of those were
shortcuts for the same action — send this card to its foundation — so on touch
they collapse into a single tap, which is what every mobile solitaire does:

| | Desktop | Touch |
| --- | --- | --- |
| Deal / recycle | click the stock | tap the stock |
| Send a card home | double-click **or** right-click | **tap the card** |
| Move a card or run | drag | drag |
| Menu | Escape | two-finger tap |
| Menu navigation | arrows + Enter | swipe, or tap the row |

Two details make the drag feel right rather than merely work:

- The run **lifts clear of the fingertip** once the gesture has travelled past
  the tap slop — but not before, so a tap never twitches the card upward on its
  way home. The threshold is the same slop `input.c` uses to classify the
  gesture, so the card lifts exactly when the gesture stops being a tap.
- The pile the run would **legally** land on is highlighted while it is held.
  Under a finger you cannot see the card you are dropping, so the board has to
  tell you the drop will take before you commit.

Deciding tap-versus-drag on *release* rather than on touch-down is what allows
both to start the same way: the drag begins immediately and feels instant, and
if the finger never moved, the release turns it into an auto-move instead of a
drop back where it started.

## 4. Both orientations, because this is a card game

The sibling repos lock their phone builds to portrait, and for a falling-block
game that is obviously right. openklondike inherited the lock by copying, which
was wrong: a **seven-column** card game reads at least as well sideways, and
essentially every mobile solitaire offers both.

Unlocking it (`screenOrientation="fullUser"`, and the two landscape entries in
`UISupportedInterfaceOrientations`) was necessary but not sufficient. The layout
came out badly on a sideways phone: it caps the card size on whichever axis
binds, which in landscape is the short one, and then centred seven now-narrow
columns in a very wide screen. The result used under half the width, and the
cards came out *smaller* than the same phone gives in portrait. Two changes fix
it, both in `layout_scaled()`:

- **Ask for three card-heights of playable space instead of four** when the
  screen is wider than it is tall. Four buys a full tableau column with no fan
  compression at all, which a sideways phone simply cannot afford; three lets
  the cards stay a sensible size and hands the difference to the draw-time fan
  compression, which exists precisely for this.
- **Spend leftover width on the column gaps**, capped at half a card. In
  portrait the width pass has already consumed everything, so this does nothing
  there; in landscape it spreads the board across the screen.

`tests/test_layout.c` asserts both outcomes — that the board uses at least 60%
of the usable width, and that rotating a phone never shrinks the cards.

Rotation is otherwise free: the board is computed from `GetScreenWidth/Height`
every frame, the Android Activity declares the relevant `configChanges` so it is
not restarted mid-game, and the iOS Metal view already recomputed its drawable
and safe-area insets in `layoutSubviews` / `safeAreaInsetsDidChange`.

## 5. Chrome is sized from the screen, and the bottom bar is gone

Two things real-device testing exposed that rendered frames had not.

The status bar pinned along the bottom looked wrong on iPhone: iOS hands the
game a viewport that already excludes the home indicator, so the bar sat flush
against it, cramped and in the way. Both sibling games solve this the same way —
wordmark bar pinned top, a stats band directly beneath it, nothing at the bottom
— so openklondike now does too. The band is openblocks' HUD: label over value,
label muted, value picked out. The wordmark bar itself is untouched.

The second was a genuine bug. The wordmark bar's height was derived from the
**card width** (`card_w * 22 / 80`). That is circular — a taller bar shrinks the
card, which shrinks the bar — and on an iPhone 12 held sideways it inflated the
bar to 96px where the siblings' screen-derived formula gives 36. It was quietly
eating 60px of the scarcest axis. All chrome now follows the screen: the
wordmark bar the height, the stats band the *short* dimension (the board is
fitted to the width, so a height-derived font came out enormous next to the
cards on a tall phone).

## 6. Sideways, the board rearranges

A top row of stock, waste and foundations costs a whole card height. Upright
that is cheap; sideways it is the scarcest thing there is. So past 1.6:1 the
board rearranges: stock and waste into a rail down the left, the four
foundations into a 2x2 block on the right, and the seven tableau columns take
the entire height between the bars — ten card columns across instead of seven.

On an iPhone 12 (2250x1107 of safe area) that is:

| | card | chrome | board width | tableau |
| --- | --- | --- | --- | --- |
| before | 176x246 | 96 + 60 | 77% | 2.5 card heights |
| after | 184x257 | 36 + 67 | 96% | 3.5 card heights |

The 1.6:1 gate matters. Rails cost three extra columns of width, which only pays
for itself when height is genuinely scarce. A tablet turned sideways is about
1.33:1 and has height to spare; giving it rails made its cards *smaller* than
the same tablet upright (164px against 174px). Keeping the row there gives it
205px instead. `tests/test_layout.c` pins that, along with the rule that no two
piles may overlap — an invariant the old single row of seven could not break by
construction, and ten hand-placed piles very much can.

## 7. The clock had to stop counting frames

`Game.timer_frames` incremented once per rendered frame and drove both the
displayed clock and the −2-points-per-10-seconds penalty. With
`SetTargetFPS(60)` on a desktop that is correct. On a 120 Hz phone it runs at
double speed, and in a browser it follows `requestAnimationFrame`, which is
whatever the tab feels like.

`src/tick.c` is the standard fixed-timestep accumulator: the loop converts the
real frame delta into whole 1/60 s steps and runs `game_tick()` that many times,
banking the remainder and dropping the backlog after five steps so a
backgrounded tab skips ahead instead of spiralling. `tests/test_game.c` asserts
60 steps per second at 30, 60, and 120 Hz.

The same accumulator drives the win cascade, so the bouncing cards leave the
same trail density at any refresh rate.

## What is deliberately not shared with openblocks

openblocks validates its fixed timestep on real hardware with a `SIMSTATS=1`
build that boots into autoplay and logs frames-vs-steps to logcat. A falling
block game plays itself under gravity; solitaire cannot without a solver, so
there is no equivalent build here and the `devicefarm` workflow omits the
`simstats` input. The unit tests over `src/tick.c` cover the same property.
