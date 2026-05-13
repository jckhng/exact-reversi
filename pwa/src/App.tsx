import { useEffect, useMemo, useState } from "react";

const SIZE = 8;
type Piece = 0 | 1 | 2; // 0=empty 1=black 2=white
type Board = Piece[][];
type MoveState = "running" | "pass" | "over";
type Difficulty = "easy" | "medium" | "hard";
type Mode = "black" | "white" | "two" | "demo";

const DIRS: [number, number][] = [[-1,-1],[0,-1],[1,-1],[-1,0],[1,0],[-1,1],[0,1],[1,1]];
const WEIGHTS: number[][] = [
  [120,-20,20,5,5,20,-20,120],
  [-20,-40,-5,-5,-5,-5,-40,-20],
  [20,-5,15,3,3,15,-5,20],
  [5,-5,3,3,3,3,-5,5],
  [5,-5,3,3,3,3,-5,5],
  [20,-5,15,3,3,15,-5,20],
  [-20,-40,-5,-5,-5,-5,-40,-20],
  [120,-20,20,5,5,20,-20,120]
];

interface GameState {
  board: Board;
  turn: 1 | 2;
  blackCount: number;
  whiteCount: number;
  moveCount: number;
  moves: string[];
  history: { board: Board; turn: 1 | 2 }[];
}

interface PersistedState {
  game: GameState;
  savedGame: GameState | null;
  difficulty: Difficulty;
  mode: Mode;
}

const STORAGE_KEY = "exact-reversi-pwa-v1";

function emptyBoard(): Board { return Array.from({ length: SIZE }, () => Array<Piece>(SIZE).fill(0)); }

function newGame(): GameState {
  const board = emptyBoard();
  board[3][3] = 2; board[3][4] = 1; board[4][3] = 1; board[4][4] = 2;
  return {
    board, turn: 1, blackCount: 2, whiteCount: 2,
    moveCount: 0, moves: [],
    history: [{ board: board.map(r => [...r] as Piece[]), turn: 1 }]
  };
}

function inBounds(x: number, y: number): boolean { return x >= 0 && x < SIZE && y >= 0 && y < SIZE; }
function other(p: 1 | 2): 1 | 2 { return p === 1 ? 2 : 1; }

function countFlipsDir(board: Board, x: number, y: number, player: Piece, dx: number, dy: number): number {
  const opp = player === 1 ? 2 : 1;
  let cx = x + dx, cy = y + dy, count = 0;
  while (inBounds(cx, cy) && board[cy][cx] === opp) { count++; cx += dx; cy += dy; }
  if (count === 0 || !inBounds(cx, cy) || board[cy][cx] !== player) return 0;
  return count;
}

function countFlips(board: Board, x: number, y: number, player: Piece): number {
  if (!inBounds(x, y) || board[y][x] !== 0) return 0;
  return DIRS.reduce((sum, [dx, dy]) => sum + countFlipsDir(board, x, y, player, dx, dy), 0);
}

function isValidMove(board: Board, x: number, y: number, player: Piece): boolean { return countFlips(board, x, y, player) > 0; }

function collectValidMoves(board: Board, player: Piece): [number, number][] {
  const moves: [number, number][] = [];
  for (let y = 0; y < SIZE; y++)
    for (let x = 0; x < SIZE; x++)
      if (isValidMove(board, x, y, player)) moves.push([x, y]);
  return moves;
}

function updateCounts(board: Board): { blackCount: number; whiteCount: number } {
  let blackCount = 0, whiteCount = 0;
  for (let y = 0; y < SIZE; y++)
    for (let x = 0; x < SIZE; x++) {
      if (board[y][x] === 1) blackCount++;
      else if (board[y][x] === 2) whiteCount++;
    }
  return { blackCount, whiteCount };
}

function applyMoveToBoard(board: Board, x: number, y: number, player: Piece): Board {
  const next = board.map(r => [...r] as Piece[]);
  for (const [dx, dy] of DIRS) {
    const flips = countFlipsDir(board, x, y, player, dx, dy);
    let cx = x + dx, cy = y + dy;
    for (let i = 0; i < flips; i++) { next[cy][cx] = player; cx += dx; cy += dy; }
  }
  next[y][x] = player;
  return next;
}

function applyMove(g: GameState, x: number, y: number): { next: GameState; state: MoveState } {
  if (!isValidMove(g.board, x, y, g.turn)) return { next: g, state: "running" };
  const board = applyMoveToBoard(g.board, x, y, g.turn);
  const { blackCount, whiteCount } = updateCounts(board);
  const opp = other(g.turn);
  const moveLabel = `${"BW"[g.turn - 1]}${"abcdefgh"[x]}${y + 1}`;
  let nextTurn: 1 | 2, state: MoveState;
  if (collectValidMoves(board, opp).length > 0) { nextTurn = opp; state = "running"; }
  else if (collectValidMoves(board, g.turn).length > 0) { nextTurn = g.turn; state = "pass"; }
  else { nextTurn = g.turn; state = "over"; }
  const next: GameState = {
    board, turn: nextTurn, blackCount, whiteCount, moveCount: g.moveCount + 1,
    moves: [...g.moves, moveLabel],
    history: [...g.history, { board: board.map(r => [...r] as Piece[]), turn: nextTurn }]
  };
  return { next, state };
}

function undoMove(g: GameState): GameState {
  if (g.history.length <= 1) return g;
  const history = g.history.slice(0, -1);
  const prev = history[history.length - 1];
  const { blackCount, whiteCount } = updateCounts(prev.board);
  return { ...g, board: prev.board.map(r => [...r] as Piece[]), turn: prev.turn, blackCount, whiteCount, moveCount: g.moveCount - 1, moves: g.moves.slice(0, -1), history };
}

function evaluatePosition(board: Board, player: Piece): number {
  const opp = player === 1 ? 2 : 1;
  let score = 0;
  for (let y = 0; y < SIZE; y++)
    for (let x = 0; x < SIZE; x++) {
      if (board[y][x] === player) score += WEIGHTS[y][x];
      else if (board[y][x] === opp) score -= WEIGHTS[y][x];
    }
  score += 4 * (collectValidMoves(board, player).length - collectValidMoves(board, opp).length);
  score += 2 * (board.flat().filter(c => c === player).length - board.flat().filter(c => c === opp).length);
  return score;
}

function aiPickMove(g: GameState, difficulty: Difficulty): [number, number] | null {
  const moves = collectValidMoves(g.board, g.turn);
  if (moves.length === 0) return null;
  const level = difficulty === "easy" ? 0 : difficulty === "medium" ? 2 : 3;
  if (level === 0) return moves[Math.floor(Math.random() * moves.length)];
  const player = g.turn;
  let best = -Infinity, bestMove = moves[0];
  for (const [x, y] of moves) {
    const board = applyMoveToBoard(g.board, x, y, player);
    let score = WEIGHTS[y][x] + countFlips(g.board, x, y, player) * 10;
    if (level >= 2) score += evaluatePosition(board, player);
    if (level >= 3) score -= collectValidMoves(board, other(player)).length * 12;
    if (score > best) { best = score; bestMove = [x, y]; }
  }
  return bestMove;
}

const fallback: PersistedState = { game: newGame(), savedGame: null, difficulty: "medium", mode: "black" };
function loadState(): PersistedState {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? { ...fallback, ...JSON.parse(raw) } : fallback;
  } catch { return fallback; }
}

function playerName(p: 1 | 2): string { return p === 1 ? "Black" : "White"; }
function winner(g: GameState): 1 | 2 | 0 {
  if (g.blackCount > g.whiteCount) return 1;
  if (g.whiteCount > g.blackCount) return 2;
  return 0;
}

function gameStatus(g: GameState, state: MoveState, mode: Mode, thinking: boolean): string {
  if (state === "over") { const w = winner(g); return w === 0 ? "Draw!" : `${playerName(w)} wins! (${g.blackCount}–${g.whiteCount})`; }
  if (state === "pass") return `${playerName(other(g.turn as 1 | 2))} has no moves. ${playerName(g.turn)} plays again.`;
  if (thinking) return "Opponent is thinking…";
  if (mode === "demo") return "AI demo running.";
  return `${playerName(g.turn)} to move. B:${g.blackCount} W:${g.whiteCount}`;
}

export default function App() {
  const initial = useMemo(loadState, []);
  const [game, setGame] = useState<GameState>(initial.game);
  const [savedGame, setSavedGame] = useState<GameState | null>(initial.savedGame);
  const [difficulty, setDifficulty] = useState<Difficulty>(initial.difficulty);
  const [mode, setMode] = useState<Mode>(initial.mode);
  const [thinking, setThinking] = useState(false);
  const [message, setMessage] = useState("");
  const [page, setPage] = useState<"game" | "about">("game");
  const [showHistory, setShowHistory] = useState(true);
  const [gameOver, setGameOver] = useState<MoveState>("running");
  const [reviewIdx, setReviewIdx] = useState<number | null>(null);

  const totalMoves = game.history.length - 1;
  const isReviewing = reviewIdx !== null;
  const displayIdx = reviewIdx ?? totalMoves;

  const displaySnap = isReviewing
    ? game.history[Math.min(reviewIdx!, game.history.length - 1)]
    : { board: game.board, turn: game.turn };
  const displayBoard = displaySnap.board;

  const hints = useMemo(() => {
    if (gameOver === "over" || isReviewing) return new Set<string>();
    return new Set(collectValidMoves(game.board, game.turn).map(([x, y]) => `${x},${y}`));
  }, [game, gameOver, isReviewing]);

  const isHumanTurn = gameOver !== "over" && !isReviewing && (
    mode === "two" ||
    (mode === "black" && game.turn === 1) ||
    (mode === "white" && game.turn === 2)
  );

  function goReview(idx: number | null) { setReviewIdx(idx); }

  useEffect(() => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ game, savedGame, difficulty, mode }));
  }, [game, savedGame, difficulty, mode]);

  useEffect(() => {
    if (gameOver === "over") { setThinking(false); return; }
    const shouldAI = mode === "demo" ||
      (mode === "black" && game.turn === 2) ||
      (mode === "white" && game.turn === 1);
    if (!shouldAI) return;
    setThinking(true);
    const id = window.setTimeout(() => {
      const move = aiPickMove(game, difficulty);
      if (!move) { setThinking(false); return; }
      const { next, state } = applyMove(game, move[0], move[1]);
      setGame(next);
      setGameOver(state);
      setMessage(gameStatus(next, state, mode, false));
      setThinking(false);
    }, mode === "demo" ? 450 : 550);
    return () => clearTimeout(id);
  }, [game, mode, difficulty, gameOver]);

  function tapCell(x: number, y: number) {
    if (!isHumanTurn || thinking || !hints.has(`${x},${y}`)) return;
    const { next, state } = applyMove(game, x, y);
    setGame(next);
    setGameOver(state);
    setMessage(gameStatus(next, state, mode, false));
    setReviewIdx(null);
  }

  function handleNew() { setGame(newGame()); setGameOver("running"); setMessage("New game."); setThinking(false); setReviewIdx(null); }
  function handleUndo() {
    let g = undoMove(game);
    if ((mode === "black" || mode === "white") && g.history.length > 1) {
      const humanPiece: Piece = mode === "black" ? 1 : 2;
      if (g.turn !== humanPiece) g = undoMove(g);
    }
    setGame(g); setGameOver("running"); setMessage("Undone."); setThinking(false); setReviewIdx(null);
  }
  function handleSave() { setSavedGame(game); setMessage("Saved."); }
  function handleLoad() {
    if (!savedGame) { setMessage("No saved game."); return; }
    setGame(savedGame); setGameOver("running"); setThinking(false); setMessage("Loaded."); setReviewIdx(null);
  }

  return (
    <main className="app">
      <header className="hero">
        <h1>Exact Reversi</h1>
        <p>{message || gameStatus(game, gameOver, mode, thinking)}</p>
      </header>

      <section className="toolbar" aria-label="Game controls">
        <button onClick={handleNew}>New</button>
        <button onClick={handleUndo} disabled={game.history.length <= 1 || thinking}>Undo</button>
        <button onClick={handleSave}>Save</button>
        <button onClick={handleLoad}>Load</button>
        <button onClick={() => setShowHistory(v => !v)} aria-pressed={showHistory}>
          {showHistory ? "Hide Moves" : "Show Moves"}
        </button>
        <button onClick={() => setPage(p => p === "game" ? "about" : "game")}>{page === "game" ? "About" : "Game"}</button>
      </section>

      <section className="settings" aria-label="Settings">
        <label>Mode
          <select value={mode} onChange={e => { setMode(e.target.value as Mode); setThinking(false); }}>
            <option value="black">Play Black</option>
            <option value="white">Play White</option>
            <option value="two">2 Player</option>
            <option value="demo">AI Demo</option>
          </select>
        </label>
        <label>Level
          <select value={difficulty} onChange={e => setDifficulty(e.target.value as Difficulty)}>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </label>
      </section>

      {page === "about" ? (
        <section className="about-page">
          <h2>About Exact Reversi</h2>
          <p>An installable browser port of Exact Reversi. Rules engine ported from the native iagno/GNOME Games implementation. Black plays first. Click a highlighted cell to place a piece and flip your opponent's pieces.</p>
          <p>Attribution: GNOME Games / iagno authors. License: GPL-3.0-or-later.</p>
          <button onClick={() => { localStorage.removeItem(STORAGE_KEY); handleNew(); }}>Clear Browser Save</button>
        </section>
      ) : (
        <section className={["play-area", showHistory ? "" : "history-hidden"].join(" ")}>
          <div>
            <div className="score-bar">
              <div className="score-item"><span className="score-dot black" /> Black: {game.blackCount}</div>
              <div className="score-item"><span className="score-dot white" /> White: {game.whiteCount}</div>
            </div>
            <div className="board" aria-label="Reversi board">
              {Array.from({ length: SIZE }, (_, y) =>
                Array.from({ length: SIZE }, (_, x) => {
                  const piece = displayBoard[y][x];
                  const isHint = hints.has(`${x},${y}`) && isHumanTurn && !thinking;
                  return (
                    <div
                      key={`${y}-${x}`}
                      className={["cell", !isHint ? "no-hover" : "", isHint ? "hint" : ""].join(" ")}
                      onClick={() => tapCell(x, y)}
                      aria-label={`${String.fromCharCode(97 + x)}${y + 1}`}
                    >
                      {piece !== 0 && <div className={`piece ${piece === 1 ? "black" : "white"}`} />}
                    </div>
                  );
                })
              )}
            </div>
          </div>

          {showHistory && (
          <aside className="history">
            <h2>Moves</h2>
            <div className="review-nav">
              <button onClick={() => goReview(0)} disabled={displayIdx === 0} title="Start">◀◀</button>
              <button onClick={() => goReview(Math.max(0, displayIdx - 1))} disabled={displayIdx === 0} title="Previous">◀</button>
              <span className="review-label">{isReviewing ? `${displayIdx}/${totalMoves}` : "Live"}</span>
              <button onClick={() => displayIdx < totalMoves ? goReview(displayIdx + 1) : goReview(null)} disabled={displayIdx >= totalMoves} title="Next">▶</button>
              <button onClick={() => goReview(null)} disabled={!isReviewing} title="Live">▶▶</button>
            </div>
            <ol>
              {game.moves.map((m, i) => (
                <li
                  key={i}
                  className={displayIdx === i + 1 ? "active" : ""}
                  onClick={() => goReview(i + 1)}
                >{m}</li>
              ))}
            </ol>
          </aside>
          )}
        </section>
      )}

      <footer className="notes">
        <p>Highlighted cells are legal moves. State auto-saved. Use Save/Load for manual restore.</p>
      </footer>
    </main>
  );
}
