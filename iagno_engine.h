#ifndef KINDLE_IAGNO_ENGINE_H
#define KINDLE_IAGNO_ENGINE_H

#include <glib.h>

#define IAGNO_SIZE 8
#define IAGNO_MAX_MOVES 64

typedef enum {
    IAGNO_EMPTY = 0,
    IAGNO_BLACK = 1,
    IAGNO_WHITE = 2
} IagnoPiece;

typedef enum {
    IAGNO_RUNNING = 0,
    IAGNO_PASS,
    IAGNO_GAME_OVER
} IagnoMoveState;

typedef struct {
    IagnoPiece board[IAGNO_SIZE][IAGNO_SIZE];
    IagnoPiece turn;
    int black_count;
    int white_count;
    int move_count;
    char moves[IAGNO_MAX_MOVES][8];
    IagnoPiece history[IAGNO_MAX_MOVES + 1][IAGNO_SIZE][IAGNO_SIZE];
    IagnoPiece turn_history[IAGNO_MAX_MOVES + 1];
} IagnoGame;

void iagno_game_init(IagnoGame *game);
gboolean iagno_is_valid_move(const IagnoGame *game, int x, int y, IagnoPiece player);
int iagno_collect_valid_moves(const IagnoGame *game, IagnoPiece player, int moves[IAGNO_MAX_MOVES][2]);
IagnoMoveState iagno_apply_move(IagnoGame *game, int x, int y);
gboolean iagno_undo(IagnoGame *game);
gboolean iagno_has_any_move(const IagnoGame *game, IagnoPiece player);
IagnoPiece iagno_other_player(IagnoPiece player);
IagnoPiece iagno_winner(const IagnoGame *game);
void iagno_ai_pick_move(const IagnoGame *game, IagnoPiece player, int level, int *out_x, int *out_y);

#endif
