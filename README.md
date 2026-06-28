# openklondike

Klondike Solitaire written in C with Raylib, in the spirit of the classic
Windows 95 game: clean, familiar, and frill-free.

Unlike its sibling projects (openblocks, opensweeper), the window is freely
**resizable** — but the **cards are a fixed size and never scale**. The board
is centred in the window with a minimum size that is just big enough to play.

## Building

### Linux/WSL2

```bash
./scripts/build_raylib_linux.sh
make
make run
```

### Release Build

```bash
make release
make run-release
```

### Windows Cross-Compile

Requires mingw-w64:

```bash
./scripts/build_raylib_windows.sh
make windows
```

Produces `build/openklondike-x64.exe` and `build/openklondike-x86.exe`.

### macOS

```bash
./scripts/build_raylib_mac.sh
make mac  # -> build/openklondike-mac (universal arm64 + x86_64)
```

## Tests

```bash
make test
```

## Playing

- **Left-click the stock** to deal cards to the waste; click the empty stock to
  recycle the waste back.
- **Drag** a card (or a valid descending, alternating-colour run) onto a tableau
  column or a foundation.
- **Double-click** a card to send it straight to its foundation.
- **Right-click** a card as a shortcut to send it to a foundation.

Build the four foundations up from Ace to King, one per suit, to win — and enjoy
the bouncing-cards cascade.

### Draw mode

Toggle **Draw: One / Three** in the menu (default: Draw One). Standard Windows 95
scoring is shown along the bottom: score, elapsed time, and move count.

### Keys

- **Escape**: menu
- **Alt+Enter**: toggle fullscreen

## Recording

Toggle **Record: On/Off** from the menu to capture your session to an H.264 MP4
(`openklondike-YYYYMMDD-HHMMSS.mp4`). No external tools required. The game is
re-rendered at a fixed canvas size for capture, so recordings stay frame-exact
regardless of the live window size.

## License

MIT. See [LICENSE](LICENSE).
