#include "iagno_engine.h"

#include <stdio.h>
#include <string.h>

static const int dirs[8][2] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1}
};

static const int weights[IAGNO_SIZE][IAGNO_SIZE] = {
    {120, -20, 20, 5, 5, 20, -20, 120},
    {-20, -40, -5, -5, -5, -5, -40, -20},
    { 20,  -5, 15, 3, 3, 15,  -5,  20},
    {  5,  -5,  3, 3, 3,  3,  -5,   5},
    {  5,  -5,  3, 3, 3,  3,  -5,   5},
    { 20,  -5, 15, 3, 3, 15,  -5,  20},
    {-20, -40, -5, -5, -5, -5, -40, -20},
    {120, -20, 20, 5, 5, 20, -20, 120}
};

static gboolean in_bounds(int x, int y)
{
    return x >= 0 && x < IAGNO_SIZE && y >= 0 && y < IAGNO_SIZE;
}

IagnoPiece iagno_other_player(IagnoPiece player)
{
    return player == IAGNO_BLACK ? IAGNO_WHITE : IAGNO_BLACK;
}

static int count_flips_dir(const IagnoGame *game, int x, int y, IagnoPiece player, int dx, int dy)
{
    IagnoPiece other = iagno_other_player(player);
    int cx = x + dx;
    int cy = y + dy;
    int count = 0;

    while (in_bounds(cx, cy) && game->board[cy][cx] == other) {
        count++;
        cx += dx;
        cy += dy;
    }

    if (count == 0 || !in_bounds(cx, cy) || game->board[cy][cx] != player)
        return 0;

    return count;
}

static int count_flips(const IagnoGame *game, int x, int y, IagnoPiece player)
{
    int total = 0;
    int i;

    if (!in_bounds(x, y) || game->board[y][x] != IAGNO_EMPTY)
        return 0;

    for (i = 0; i < 8; i++)
        total += count_flips_dir(game, x, y, player, dirs[i][0], dirs[i][1]);

    return total;
}

static void update_counts(IagnoGame *game)
{
    int x;
    int y;

    game->black_count = 0;
    game->white_count = 0;

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            if (game->board[y][x] == IAGNO_BLACK)
                game->black_count++;
            else if (game->board[y][x] == IAGNO_WHITE)
                game->white_count++;
        }
    }
}

static void save_history(IagnoGame *game)
{
    if (game->move_count > IAGNO_MAX_MOVES)
        return;

    memcpy(game->history[game->move_count], game->board, sizeof(game->board));
    game->turn_history[game->move_count] = game->turn;
}

void iagno_game_init(IagnoGame *game)
{
    memset(game, 0, sizeof(*game));

    game->board[3][3] = IAGNO_WHITE;
    game->board[3][4] = IAGNO_BLACK;
    game->board[4][3] = IAGNO_BLACK;
    game->board[4][4] = IAGNO_WHITE;
    game->turn = IAGNO_BLACK;
    update_counts(game);
    save_history(game);
}

gboolean iagno_is_valid_move(const IagnoGame *game, int x, int y, IagnoPiece player)
{
    return count_flips(game, x, y, player) > 0;
}

int iagno_collect_valid_moves(const IagnoGame *game, IagnoPiece player, int moves[IAGNO_MAX_MOVES][2])
{
    int count = 0;
    int x;
    int y;

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            if (iagno_is_valid_move(game, x, y, player)) {
                moves[count][0] = x;
                moves[count][1] = y;
                count++;
            }
        }
    }

    return count;
}

gboolean iagno_has_any_move(const IagnoGame *game, IagnoPiece player)
{
    int moves[IAGNO_MAX_MOVES][2];
    return iagno_collect_valid_moves(game, player, moves) > 0;
}

IagnoMoveState iagno_apply_move(IagnoGame *game, int x, int y)
{
    IagnoPiece player = game->turn;
    IagnoPiece other = iagno_other_player(player);
    int i;
    int d;

    if (!iagno_is_valid_move(game, x, y, player))
        return IAGNO_RUNNING;

    for (d = 0; d < 8; d++) {
        int dx = dirs[d][0];
        int dy = dirs[d][1];
        int flips = count_flips_dir(game, x, y, player, dx, dy);
        int cx = x + dx;
        int cy = y + dy;

        for (i = 0; i < flips; i++) {
            game->board[cy][cx] = player;
            cx += dx;
            cy += dy;
        }
    }

    game->board[y][x] = player;
    if (game->move_count < IAGNO_MAX_MOVES) {
        snprintf(game->moves[game->move_count], sizeof(game->moves[game->move_count]),
                 "%c%c%c", player == IAGNO_BLACK ? 'B' : 'W', 'a' + x, '1' + y);
    }
    game->move_count++;

    update_counts(game);

    if (iagno_has_any_move(game, other)) {
        game->turn = other;
        save_history(game);
        return IAGNO_RUNNING;
    }

    if (iagno_has_any_move(game, player)) {
        game->turn = player;
        save_history(game);
        return IAGNO_PASS;
    }

    save_history(game);
    return IAGNO_GAME_OVER;
}

gboolean iagno_undo(IagnoGame *game)
{
    if (game->move_count <= 0)
        return FALSE;

    game->move_count--;
    memcpy(game->board, game->history[game->move_count], sizeof(game->board));
    game->turn = game->turn_history[game->move_count];
    memset(game->moves[game->move_count], 0, sizeof(game->moves[game->move_count]));
    update_counts(game);
    return TRUE;
}

IagnoPiece iagno_winner(const IagnoGame *game)
{
    if (game->black_count > game->white_count)
        return IAGNO_BLACK;
    if (game->white_count > game->black_count)
        return IAGNO_WHITE;
    return IAGNO_EMPTY;
}

static int evaluate_position(const IagnoGame *game, IagnoPiece player)
{
    IagnoPiece other = iagno_other_player(player);
    int score = 0;
    int player_moves[IAGNO_MAX_MOVES][2];
    int other_moves[IAGNO_MAX_MOVES][2];
    int x;
    int y;

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            if (game->board[y][x] == player)
                score += weights[y][x];
            else if (game->board[y][x] == other)
                score -= weights[y][x];
        }
    }

    score += 4 * (iagno_collect_valid_moves(game, player, player_moves) -
                  iagno_collect_valid_moves(game, other, other_moves));
    score += 2 * ((player == IAGNO_BLACK ? game->black_count : game->white_count) -
                  (player == IAGNO_BLACK ? game->white_count : game->black_count));

    return score;
}

static int score_move(const IagnoGame *game, IagnoPiece player, int x, int y, int level)
{
    IagnoGame copy = *game;
    int flips = count_flips(game, x, y, player);
    int score;

    copy.turn = player;
    iagno_apply_move(&copy, x, y);

    score = weights[y][x] + flips * 10;
    if (level >= 2)
        score += evaluate_position(&copy, player);
    if (level >= 3) {
        int reply[IAGNO_MAX_MOVES][2];
        int replies = iagno_collect_valid_moves(&copy, iagno_other_player(player), reply);
        score -= replies * 12;
    }

    return score;
}

void iagno_ai_pick_move(const IagnoGame *game, IagnoPiece player, int level, int *out_x, int *out_y)
{
    int moves[IAGNO_MAX_MOVES][2];
    int count = iagno_collect_valid_moves(game, player, moves);
    int best_score = -G_MAXINT;
    int best_index = 0;
    int i;

    *out_x = -1;
    *out_y = -1;

    if (count <= 0)
        return;

    if (level <= 0) {
        best_index = g_random_int_range(0, count);
    } else {
        for (i = 0; i < count; i++) {
            int score = score_move(game, player, moves[i][0], moves[i][1], level);
            if (score > best_score) {
                best_score = score;
                best_index = i;
            }
        }
    }

    *out_x = moves[best_index][0];
    *out_y = moves[best_index][1];
}
