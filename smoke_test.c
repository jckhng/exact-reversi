#include "iagno_engine.h"

#include <stdio.h>

static int fail(const char *message)
{
    fprintf(stderr, "smoke-test: %s\n", message);
    return 1;
}

int main(void)
{
    IagnoGame game;
    int moves[IAGNO_MAX_MOVES][2];
    int x;
    int y;

    iagno_game_init(&game);

    if (game.black_count != 2 || game.white_count != 2)
        return fail("initial counts are wrong");

    if (iagno_collect_valid_moves(&game, IAGNO_BLACK, moves) != 4)
        return fail("black should have four opening moves");

    if (!iagno_is_valid_move(&game, 3, 2, IAGNO_BLACK))
        return fail("d3 should be a valid black opening move");

    if (iagno_apply_move(&game, 3, 2) != IAGNO_RUNNING)
        return fail("failed to apply d3");

    if (game.black_count != 4 || game.white_count != 1)
        return fail("counts after d3 are wrong");

    iagno_ai_pick_move(&game, IAGNO_WHITE, 2, &x, &y);
    if (x < 0 || y < 0 || !iagno_is_valid_move(&game, x, y, IAGNO_WHITE))
        return fail("AI did not choose a valid move");

    if (!iagno_undo(&game))
        return fail("undo failed");

    if (game.black_count != 2 || game.white_count != 2 || game.turn != IAGNO_BLACK)
        return fail("undo did not restore opening state");

    puts("smoke-test: ok");
    return 0;
}
