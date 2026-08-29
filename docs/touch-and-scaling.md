# Touch and scaling

Why the mobile and web builds differ from the desktop one. Each of these
contradicts something the desktop game assumed, and none of it is obvious from
the code alone.

## Cards scale where the window cannot be constrained

The desktop board uses fixed 80x112 cards and a 704x704 minimum window
(`SetWindowMinSize`), which is exactly wide enough for seven 80px columns plus
gaps and margins. Resizing moves the margins and nothing else.

A phone cannot honour that. Seven fixed columns need 704 physical pixels; on a
1080-wide handset that fills the screen edge to edge, and on anything narrower it
does not fit. A Klondike board must show seven columns at once.

So the rule is scoped, not dropped:

- Where a minimum size can be enforced, cards never scale. Desktop keeps the
  fixed board and the 704x704 minimum.
- Where it cannot, the board scales to fit. The touch board fits seven columns to
  the live screen. The web build's *fixed* board also shrinks below the minimum,
  because a browser window is whatever the user drags it to.

The scaled board derives every metric from the resulting card width using the
ratios the fixed board hardcodes (gap 16/80, fans 28/80 and 12/80, and so on).
`tests/test_layout.c` asserts the desktop numbers are unchanged and that the
touch board fits each device shape inside its margins -- a board that fits by
running edge to edge has eaten them.

## Fans compress instead of overflowing

The deepest possible column is six face-down cards under a thirteen-card run.
At the natural fan spacing that is 536px against 468px of available height on
the desktop board, so deep columns ran off the bottom even at the fixed size.

`tab_card_ys()` compresses a column's fan uniformly until it fits, with a floor
that keeps a face-down card visible as a sliver. Drawing, hit testing and drop
targeting all call it, so they cannot disagree about where a card is. Hit testing
measures a card's exposed strip as the distance to the next card's top rather
than recomputing the step, so it stays correct under compression.

## Touch grammar

There is no right-click on a touchscreen, and double-tap is a poor gesture.
Both were shortcuts for the same action, so they collapse into a single tap:

| | Desktop | Touch |
| --- | --- | --- |
| Deal / recycle | click the stock | tap the stock |
| Send a card home | double-click or right-click | tap the card |
| Move a card or run | drag | drag |
| Menu | Escape | two-finger tap |
| Menu navigation | arrows + Enter | swipe, or tap the row |

Two details in the drag:

- The run lifts clear of the fingertip once the gesture passes the tap slop, and
  not before, so a tap does not twitch the card. The threshold is the same slop
  `input.c` uses to classify the gesture.
- The pile the run would legally land on is highlighted while it is held. A
  finger covers the card being dropped.

Tap versus drag is decided on release, not touch-down. That lets both start the
same way: the drag begins immediately, and if the finger never moved the release
turns it into an auto-move.

## Both orientations

Phone builds are not locked to portrait (`screenOrientation="fullUser"`, plus
the landscape entries in `UISupportedInterfaceOrientations`). A seven-column
card game reads sideways, and mobile solitaire generally offers both.

Rotation is free: the board is computed from `GetScreenWidth/Height` every
frame, the Android Activity declares the `configChanges` that keep it alive
across a rotation, and the iOS Metal view recomputes its drawable and safe-area
insets in `layoutSubviews` / `safeAreaInsetsDidChange`.

## Chrome follows the screen, not the card

The wordmark bar's height was derived from the card width (`card_w * 22 / 80`).
That is circular -- a taller bar shrinks the card, which shrinks the bar -- and
on an iPhone 12 held sideways it produced a 96px bar where the screen-derived
formula gives 36, on the scarcest axis.

All chrome now follows the screen: the wordmark bar the height, the stats line
the short dimension. The board is fitted to the width, so a height-derived font
is oversized next to the cards on a tall phone.

The touch board has no bottom bar. iOS hands the game a viewport that already
excludes the home indicator, so a bar pinned to the bottom sits flush against
it. The stats are one line under the wordmark instead.

## Landscape: bigger cards, same arrangement

Both orientations use the classic arrangement -- stock, waste, a gap, then the
four foundations across the top of the seven-column grid the tableau uses. World
of Solitaire and Green Felt both lay a landscape board out this way.

Landscape reserves less tableau depth instead: 2.2 card heights against the 4 it
asks for upright. That lets the cards grow and leaves deep columns to the fan
compression. Leftover width goes into the column gaps, capped at a third of a
card. On an iPhone 12 (2250x1107 of safe area) the cards are 188x263 with the
board at 75% of the width.

An earlier version split the piles into side rails -- stock and waste left,
foundations 2x2 right -- to give the tableau the full height. It measured better
(96% of the width, 3.5 card heights) and was rejected: no established Klondike
does it, and the top row is how the board is read.

A seven-column board does not fill a 2:1 screen. The references leave the same
gap.

## The clock counts simulation steps, not frames

`Game.timer_frames` incremented once per rendered frame and drove both the
displayed clock and the -2-points-per-10-seconds penalty. Correct under
`SetTargetFPS(60)`; double speed on a 120 Hz phone; arbitrary in a browser tab.

`src/tick.c` is a fixed-timestep accumulator. The loop converts the real frame
delta into whole 1/60 s steps, runs `game_tick()` that many times, banks the
remainder, and drops the backlog after five steps so a backgrounded tab skips
ahead rather than spiralling. `tests/test_game.c` asserts 60 steps per second at
30, 60 and 120 Hz. The same accumulator drives the win cascade, so the trail
density is refresh-rate independent.

## Not shared with openblocks

openblocks validates its fixed timestep on hardware with a `SIMSTATS=1` build
that boots into autoplay and logs frames-vs-steps to logcat. A falling-block
game plays itself under gravity; solitaire cannot without a solver. There is no
equivalent build here, the `devicefarm` workflow omits the `simstats` input, and
the unit tests over `src/tick.c` cover the same property.
