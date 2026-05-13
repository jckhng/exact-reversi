# Exact Reversi PWA

An installable browser Reversi/Othello game in grayscale e-ink style.

## Features

- Play as Black or White against the built-in AI (Easy/Medium/Hard).
- 2-player local and AI demo mode.
- Legal move hints highlighted on the board.
- Undo moves.
- Save/Load a manual restore point.
- Works offline after first load.
- Installable via Chrome "Add to Home Screen."

## Building

```bash
npm install
npm run typecheck
npm run build
```

## Rules

Click a highlighted square to place your piece. The move is only legal if it
flips at least one of your opponent's pieces. Black plays first.

## Attribution

Rules engine ported from the iagno Reversi implementation in GNOME Games.
Part of the Exact Games / GnomeGames4Kindle project.

## License

GPL-3.0-or-later. See THIRD_PARTY.md for dependency details.
