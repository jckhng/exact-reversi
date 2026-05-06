#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>

#include "iagno_engine.h"

#define APP_TITLE "Kindle Iagno"
#define KINDLE_WINDOW_TITLE "L:A_N:application_ID:kindleiagno_PC:N_O:URL"
#define KINDLE_WINDOW_TITLE_TOPBAR "L:A_N:application_PC:T_ID:kindleiagno_O:URL"
#define SAVE_PATH "/mnt/us/extensions/kindle-iagno/kindle-iagno.save"
#define LEGACY_SAVE_PATH "/mnt/us/documents/kindle-iagno.txt"
#define LOG_PATH "/mnt/us/kindle-iagno.log"
#define KINDLE_APP_WIDTH 1072
#define KINDLE_APP_HEIGHT 1448

typedef enum {
    MODE_PLAY_BLACK = 0,
    MODE_PLAY_WHITE = 1,
    MODE_TWO_PLAYER = 2,
    MODE_AI_VS_AI = 3
} AppMode;

static const char *kindle_window_title(void)
{
    const char *value = g_getenv("KINDLE_SHOW_TOPBAR");
    return (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0) ? KINDLE_WINDOW_TITLE_TOPBAR
                                                                          : KINDLE_WINDOW_TITLE;
}

typedef struct {
    GtkWidget *window;
    GtkWidget *board;
    GtkWidget *status;
    GtkWidget *black_score;
    GtkWidget *white_score;
    GtkWidget *mode_combo;
    GtkWidget *level_combo;
    GtkWidget *moves_label;
    GtkWidget *history_sidebar;
    GtkWidget *history_toggle_button;
    GtkWidget *history_first_button;
    GtkWidget *history_prev_button;
    GtkWidget *history_next_button;
    GtkWidget *history_latest_button;
    IagnoGame game;
    AppMode mode;
    int level;
    int view_ply;
    gboolean game_over;
    guint ai_source;
    char message[160];
    gboolean history_visible;
} AppState;

static AppState app;

static gboolean is_ai_turn(void);
static void update_ui(void);
static void apply_move_and_update(int x, int y);
static void new_game(void);
static void undo_cb(GtkWidget *widget, gpointer data);
static void save_cb(GtkWidget *widget, gpointer data);
static void load_cb(GtkWidget *widget, gpointer data);
static gboolean canvas_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data);
static gboolean canvas_button(GtkWidget *widget, GdkEventButton *event, gpointer data);

static void toggle_history_cb(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    app.history_visible = !app.history_visible;
    if (app.history_visible) {
        gtk_widget_show(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Hide Moves");
    } else {
        gtk_widget_hide(app.history_sidebar);
        gtk_button_set_label(GTK_BUTTON(app.history_toggle_button), "Show Moves");
    }
    gtk_widget_queue_resize(app.board);
    gtk_widget_queue_draw(app.board);
}

static void app_log(const char *message)
{
    FILE *f = fopen(LOG_PATH, "a");
    if (!f)
        return;
    fprintf(f, "[app] %s\n", message);
    fclose(f);
}

static void app_apply_high_contrast(GtkWidget *widget)
{
    GdkColor black = { 0, 0x0000, 0x0000, 0x0000 };
    GdkColor white = { 0, 0xffff, 0xffff, 0xffff };

    gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, &black);
    gtk_widget_modify_fg(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &black);
    gtk_widget_modify_text(widget, GTK_STATE_SELECTED, &white);
    gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_base(widget, GTK_STATE_SELECTED, &black);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_bg(widget, GTK_STATE_SELECTED, &black);
}

static void app_install_kindle_style(void)
{
    gtk_rc_parse_string(
        "style \"kindle_high_contrast\" {\n"
        "  fg[NORMAL] = \"#000000\"\n"
        "  fg[ACTIVE] = \"#000000\"\n"
        "  fg[PRELIGHT] = \"#ffffff\"\n"
        "  fg[SELECTED] = \"#ffffff\"\n"
        "  text[NORMAL] = \"#000000\"\n"
        "  text[ACTIVE] = \"#000000\"\n"
        "  text[SELECTED] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\"\n"
        "  base[ACTIVE] = \"#ffffff\"\n"
        "  base[SELECTED] = \"#000000\"\n"
        "  bg[NORMAL] = \"#ffffff\"\n"
        "  bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[PRELIGHT] = \"#000000\"\n"
        "  bg[SELECTED] = \"#000000\"\n"
        "}\n"
        "gtk-button-images = 0\n"
        "gtk-menu-images = 0\n"
        "class \"GtkComboBox\" style \"kindle_high_contrast\"\n"
        "class \"GtkCellView\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenu\" style \"kindle_high_contrast\"\n"
        "class \"GtkMenuItem\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkComboBox*\" style \"kindle_high_contrast\"\n"
        "widget_class \"*GtkMenu*\" style \"kindle_high_contrast\"\n"
    );
}

static const char *piece_name(IagnoPiece piece)
{
    if (piece == IAGNO_BLACK)
        return "Black";
    if (piece == IAGNO_WHITE)
        return "White";
    return "Draw";
}

static void set_message(const char *message)
{
    g_strlcpy(app.message, message, sizeof(app.message));
}

static int current_view_ply(void)
{
    if (app.view_ply < 0 || app.view_ply > app.game.move_count)
        return app.game.move_count;
    return app.view_ply;
}

static gboolean is_viewing_latest(void)
{
    return current_view_ply() == app.game.move_count;
}

static IagnoPiece viewed_piece_at(int x, int y)
{
    return app.game.history[current_view_ply()][y][x];
}

static void count_viewed_pieces(int *black, int *white)
{
    int x;
    int y;

    *black = 0;
    *white = 0;

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            IagnoPiece piece = viewed_piece_at(x, y);
            if (piece == IAGNO_BLACK)
                (*black)++;
            else if (piece == IAGNO_WHITE)
                (*white)++;
        }
    }
}

static gboolean is_ai_turn(void)
{
    if (app.game_over)
        return FALSE;
    if (!is_viewing_latest())
        return FALSE;

    switch (app.mode) {
    case MODE_PLAY_BLACK:
        return app.game.turn == IAGNO_WHITE;
    case MODE_PLAY_WHITE:
        return app.game.turn == IAGNO_BLACK;
    case MODE_TWO_PLAYER:
        return FALSE;
    case MODE_AI_VS_AI:
        return TRUE;
    }

    return FALSE;
}

static const char *mode_name(AppMode mode)
{
    switch (mode) {
    case MODE_PLAY_BLACK:
        return "Play Black";
    case MODE_PLAY_WHITE:
        return "Play White";
    case MODE_TWO_PLAYER:
        return "2 Player";
    case MODE_AI_VS_AI:
        return "AI Demo";
    }

    return "Play Black";
}

static void draw_disc(cairo_t *cr, double cx, double cy, double r, IagnoPiece piece)
{
    cairo_save(cr);
    cairo_arc(cr, cx + 2, cy + 2, r, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
    if (piece == IAGNO_BLACK) {
        cairo_pattern_t *pat = cairo_pattern_create_radial(cx - r * 0.25, cy - r * 0.25, r * 0.1, cx, cy, r);
        cairo_pattern_add_color_stop_rgb(pat, 0, 0.30, 0.30, 0.30);
        cairo_pattern_add_color_stop_rgb(pat, 1, 0.02, 0.02, 0.02);
        cairo_set_source(cr, pat);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(pat);
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    } else {
        cairo_pattern_t *pat = cairo_pattern_create_radial(cx - r * 0.25, cy - r * 0.25, r * 0.1, cx, cy, r);
        cairo_pattern_add_color_stop_rgb(pat, 0, 1.0, 1.0, 1.0);
        cairo_pattern_add_color_stop_rgb(pat, 1, 0.62, 0.62, 0.62);
        cairo_set_source(cr, pat);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(pat);
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    }
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static gboolean board_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    cairo_t *cr = gdk_cairo_create(widget->window);
    GtkAllocation allocation;
    int valid[IAGNO_MAX_MOVES][2];
    int valid_count;
    int side;
    int offset_x;
    int offset_y;
    int cell;
    int x;
    int y;
    int i;

    (void) event;
    (void) data;

    gtk_widget_get_allocation(widget, &allocation);
    side = allocation.width < allocation.height ? allocation.width : allocation.height;
    side -= 8;
    cell = side / IAGNO_SIZE;
    side = cell * IAGNO_SIZE;
    offset_x = (allocation.width - side) / 2;
    offset_y = (allocation.height - side) / 2;

    cairo_set_source_rgb(cr, 0.86, 0.84, 0.76);
    cairo_paint(cr);

    cairo_rectangle(cr, offset_x, offset_y, side, side);
    cairo_set_source_rgb(cr, 0.70, 0.78, 0.63);
    cairo_fill(cr);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 0.12, 0.16, 0.12);
    for (i = 0; i <= IAGNO_SIZE; i++) {
        cairo_move_to(cr, offset_x + i * cell + 0.5, offset_y);
        cairo_line_to(cr, offset_x + i * cell + 0.5, offset_y + side);
        cairo_move_to(cr, offset_x, offset_y + i * cell + 0.5);
        cairo_line_to(cr, offset_x + side, offset_y + i * cell + 0.5);
    }
    cairo_stroke(cr);

    valid_count = (is_viewing_latest() && !is_ai_turn()) ?
        iagno_collect_valid_moves(&app.game, app.game.turn, valid) : 0;
    for (i = 0; i < valid_count; i++) {
        double cx = offset_x + valid[i][0] * cell + cell / 2.0;
        double cy = offset_y + valid[i][1] * cell + cell / 2.0;
        cairo_arc(cr, cx, cy, cell * 0.12, 0, 2 * G_PI);
        cairo_set_source_rgb(cr, 0.05, 0.20, 0.05);
        cairo_fill(cr);
    }

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            IagnoPiece piece = viewed_piece_at(x, y);
            if (piece != IAGNO_EMPTY) {
                double cx = offset_x + x * cell + cell / 2.0;
                double cy = offset_y + y * cell + cell / 2.0;
                draw_disc(cr, cx, cy, cell * 0.38, piece);
            }
        }
    }

    cairo_destroy(cr);
    return FALSE;
}

static void cairo_text(cairo_t *cr, double x, double y, const char *text, double size, gboolean bold)
{
    cairo_select_font_face(cr, "Sans", bold ? CAIRO_FONT_SLANT_NORMAL : CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

static void canvas_button_rect(cairo_t *cr, double x, double y, double w, double h, const char *label)
{
    cairo_rectangle(cr, x, y, w, h);
    cairo_set_source_rgb(cr, 0.96, 0.96, 0.90);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_text(cr, x + 7, y + 19, label, 12, TRUE);
}

static gboolean point_in(double px, double py, double x, double y, double w, double h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void canvas_draw_board(cairo_t *cr, int ox, int oy, int size)
{
    int valid[IAGNO_MAX_MOVES][2];
    int valid_count = (is_viewing_latest() && !is_ai_turn()) ?
        iagno_collect_valid_moves(&app.game, app.game.turn, valid) : 0;
    int cell = size / IAGNO_SIZE;
    int x;
    int y;
    int i;

    cairo_rectangle(cr, ox, oy, cell * IAGNO_SIZE, cell * IAGNO_SIZE);
    cairo_set_source_rgb(cr, 0.70, 0.78, 0.63);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    for (i = 0; i <= IAGNO_SIZE; i++) {
        cairo_move_to(cr, ox + i * cell + 0.5, oy);
        cairo_line_to(cr, ox + i * cell + 0.5, oy + cell * IAGNO_SIZE);
        cairo_move_to(cr, ox, oy + i * cell + 0.5);
        cairo_line_to(cr, ox + cell * IAGNO_SIZE, oy + i * cell + 0.5);
    }
    cairo_stroke(cr);

    for (i = 0; i < valid_count; i++) {
        double cx = ox + valid[i][0] * cell + cell / 2.0;
        double cy = oy + valid[i][1] * cell + cell / 2.0;
        cairo_arc(cr, cx, cy, cell * 0.11, 0, 2 * G_PI);
        cairo_set_source_rgb(cr, 0.05, 0.18, 0.05);
        cairo_fill(cr);
    }

    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            IagnoPiece piece = viewed_piece_at(x, y);
            if (piece != IAGNO_EMPTY) {
                draw_disc(cr,
                          ox + x * cell + cell / 2.0,
                          oy + y * cell + cell / 2.0,
                          cell * 0.38,
                          piece);
            }
        }
    }
}

static gboolean canvas_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    cairo_t *cr = gdk_cairo_create(widget->window);
    char buf[80];
    int i;
    int start;

    (void) event;
    (void) data;
    app_log("canvas expose");

    cairo_set_source_rgb(cr, 0.88, 0.86, 0.78);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_text(cr, 8, 18, "Kindle Iagno", 15, TRUE);
    cairo_text(cr, 112, 18, app.message, 11, FALSE);

    canvas_button_rect(cr, 4, 25, 74, 26, "New");
    canvas_button_rect(cr, 82, 25, 74, 26, "Undo");
    canvas_button_rect(cr, 160, 25, 74, 26, "Save");
    canvas_button_rect(cr, 238, 25, 74, 26, "Load");
    canvas_button_rect(cr, 316, 25, 74, 26, "Quit");

    snprintf(buf, sizeof(buf), "Mode: %s", mode_name(app.mode));
    canvas_button_rect(cr, 4, 56, 188, 26, buf);
    snprintf(buf, sizeof(buf), "Level: %s", app.level == 0 ? "Easy" : app.level == 1 ? "Medium" : "Hard");
    canvas_button_rect(cr, 202, 56, 188, 26, buf);

    canvas_draw_board(cr, 4, 88, 264);

    cairo_set_source_rgb(cr, 0, 0, 0);
    snprintf(buf, sizeof(buf), "Black: %d", app.game.black_count);
    cairo_text(cr, 278, 104, buf, 13, TRUE);
    snprintf(buf, sizeof(buf), "White: %d", app.game.white_count);
    cairo_text(cr, 278, 124, buf, 13, TRUE);
    cairo_text(cr, 278, 150, "Moves", 13, TRUE);

    start = app.game.move_count > 10 ? app.game.move_count - 10 : 0;
    for (i = start; i < app.game.move_count; i++) {
        if (app.game.moves[i][0] == '\0')
            continue;
        snprintf(buf, sizeof(buf), "%2d. %s", i + 1, app.game.moves[i]);
        cairo_text(cr, 278, 170 + (i - start) * 16, buf, 12, FALSE);
    }

    cairo_destroy(cr);
    return TRUE;
}

static gboolean canvas_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    int board_x = 4;
    int board_y = 88;
    int cell = 264 / IAGNO_SIZE;
    int x;
    int y;

    (void) widget;
    (void) data;

    if (event->button != 1)
        return FALSE;

    if (point_in(event->x, event->y, 4, 25, 74, 26)) {
        new_game();
        return TRUE;
    }
    if (point_in(event->x, event->y, 82, 25, 74, 26)) {
        undo_cb(NULL, NULL);
        return TRUE;
    }
    if (point_in(event->x, event->y, 160, 25, 74, 26)) {
        save_cb(NULL, NULL);
        return TRUE;
    }
    if (point_in(event->x, event->y, 238, 25, 74, 26)) {
        load_cb(NULL, NULL);
        return TRUE;
    }
    if (point_in(event->x, event->y, 316, 25, 74, 26)) {
        gtk_main_quit();
        return TRUE;
    }
    if (point_in(event->x, event->y, 4, 56, 188, 26)) {
        app.mode = (AppMode) (((int) app.mode + 1) % 4);
        new_game();
        return TRUE;
    }
    if (point_in(event->x, event->y, 202, 56, 188, 26)) {
        app.level = (app.level + 1) % 3;
        snprintf(app.message, sizeof(app.message), "Difficulty changed. %s to move.", piece_name(app.game.turn));
        update_ui();
        return TRUE;
    }

    if (is_ai_turn() || !point_in(event->x, event->y, board_x, board_y, cell * 8, cell * 8))
        return FALSE;

    x = ((int) event->x - board_x) / cell;
    y = ((int) event->y - board_y) / cell;

    if (!iagno_is_valid_move(&app.game, x, y, app.game.turn)) {
        set_message("Invalid move.");
        update_ui();
        return TRUE;
    }

    apply_move_and_update(x, y);
    return TRUE;
}

static void apply_move_and_update(int x, int y)
{
    IagnoMoveState state = iagno_apply_move(&app.game, x, y);
    app.view_ply = -1;

    if (state == IAGNO_PASS) {
        snprintf(app.message, sizeof(app.message), "%s has no move. %s plays again.",
                 piece_name(iagno_other_player(app.game.turn)), piece_name(app.game.turn));
    } else if (state == IAGNO_GAME_OVER) {
        IagnoPiece winner = iagno_winner(&app.game);
        app.game_over = TRUE;
        if (winner == IAGNO_EMPTY)
            set_message("Game over. Draw.");
        else
            snprintf(app.message, sizeof(app.message), "Game over. %s wins.", piece_name(winner));
    } else {
        snprintf(app.message, sizeof(app.message), "%s to move.", piece_name(app.game.turn));
    }

    update_ui();
}

static gboolean board_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    GtkAllocation allocation;
    int side;
    int offset_x;
    int offset_y;
    int cell;
    int x;
    int y;

    (void) data;

    if (event->button != 1 || !is_viewing_latest() || is_ai_turn())
        return FALSE;

    gtk_widget_get_allocation(widget, &allocation);
    side = allocation.width < allocation.height ? allocation.width : allocation.height;
    side -= 8;
    cell = side / IAGNO_SIZE;
    side = cell * IAGNO_SIZE;
    offset_x = (allocation.width - side) / 2;
    offset_y = (allocation.height - side) / 2;

    x = ((int) event->x - offset_x) / cell;
    y = ((int) event->y - offset_y) / cell;

    if (x < 0 || x >= IAGNO_SIZE || y < 0 || y >= IAGNO_SIZE)
        return FALSE;

    if (!iagno_is_valid_move(&app.game, x, y, app.game.turn)) {
        set_message("Invalid move.");
        update_ui();
        return FALSE;
    }

    apply_move_and_update(x, y);
    return TRUE;
}

static gboolean ai_timeout(gpointer data)
{
    int x = -1;
    int y = -1;

    (void) data;
    app.ai_source = 0;

    if (!is_ai_turn())
        return FALSE;

    iagno_ai_pick_move(&app.game, app.game.turn, app.level, &x, &y);
    if (x >= 0 && y >= 0)
        apply_move_and_update(x, y);
    else
        update_ui();

    return FALSE;
}

static void update_history_label(void)
{
    GString *text = g_string_new("");
    int view_ply = current_view_ply();
    int i;

    if (view_ply == 0)
        g_string_append(text, "> Start\n");
    else
        g_string_append(text, "  Start\n");

    for (i = 0; i < app.game.move_count; i++) {
        if (app.game.moves[i][0] == '\0')
            continue;
        g_string_append_printf(text, "%c%2d. %s\n", view_ply == i + 1 ? '>' : ' ', i + 1, app.game.moves[i]);
    }

    gtk_label_set_text(GTK_LABEL(app.moves_label), text->str);
    g_string_free(text, TRUE);
}

static void update_ui(void)
{
    char buf[128];
    int black_count;
    int white_count;
    int view_ply = current_view_ply();

    if (app.status == NULL) {
        gtk_widget_queue_draw(app.window);
        if (is_ai_turn() && app.ai_source == 0)
            app.ai_source = g_timeout_add(300, ai_timeout, NULL);
        return;
    }

    if (is_viewing_latest()) {
        gtk_label_set_text(GTK_LABEL(app.status), app.message);
    } else {
        snprintf(buf, sizeof(buf), "Viewing move %d of %d. Tap >| to resume play.", view_ply, app.game.move_count);
        gtk_label_set_text(GTK_LABEL(app.status), buf);
    }

    count_viewed_pieces(&black_count, &white_count);
    snprintf(buf, sizeof(buf), "Black: %d", black_count);
    gtk_label_set_text(GTK_LABEL(app.black_score), buf);
    snprintf(buf, sizeof(buf), "White: %d", white_count);
    gtk_label_set_text(GTK_LABEL(app.white_score), buf);
    update_history_label();
    gtk_widget_queue_draw(app.board);

    gtk_widget_set_sensitive(app.history_first_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_prev_button, view_ply > 0);
    gtk_widget_set_sensitive(app.history_next_button, view_ply < app.game.move_count);
    gtk_widget_set_sensitive(app.history_latest_button, view_ply < app.game.move_count);

    if (is_ai_turn()) {
        if (app.ai_source == 0)
            app.ai_source = g_timeout_add(300, ai_timeout, NULL);
    }
}

static void new_game(void)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }
    iagno_game_init(&app.game);
    app.view_ply = -1;
    app.game_over = FALSE;
    set_message("Black to move.");
    update_ui();
}

static void new_game_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    new_game();
}

static void undo_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;

    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (iagno_undo(&app.game)) {
        if (app.mode == MODE_PLAY_BLACK && app.game.turn == IAGNO_WHITE)
            iagno_undo(&app.game);
        else if (app.mode == MODE_PLAY_WHITE && app.game.turn == IAGNO_BLACK)
            iagno_undo(&app.game);
        app.view_ply = -1;
        app.game_over = FALSE;
        snprintf(app.message, sizeof(app.message), "%s to move.", piece_name(app.game.turn));
    } else {
        set_message("Nothing to undo.");
    }
    update_ui();
}

static void quit_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    gtk_main_quit();
}

static void save_cb(GtkWidget *widget, gpointer data)
{
    FILE *f;
    int x;
    int y;

    (void) widget;
    (void) data;

    f = fopen(SAVE_PATH, "w");
    if (!f) {
        set_message("Could not save game.");
        update_ui();
        return;
    }

    fprintf(f, "turn %d\n", app.game.turn);
    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++)
            fputc('0' + app.game.board[y][x], f);
        fputc('\n', f);
    }
    fclose(f);
    set_message("Game saved.");
    update_ui();
}

static void load_cb(GtkWidget *widget, gpointer data)
{
    FILE *f;
    IagnoGame loaded;
    int turn;
    int x;
    int y;
    char line[32];

    (void) widget;
    (void) data;

    f = fopen(SAVE_PATH, "r");
    if (f == NULL)
        f = fopen(LEGACY_SAVE_PATH, "r");
    if (!f) {
        set_message("No saved game found.");
        update_ui();
        return;
    }

    memset(&loaded, 0, sizeof(loaded));
    if (fscanf(f, "turn %d\n", &turn) != 1) {
        fclose(f);
        set_message("Save file is invalid.");
        update_ui();
        return;
    }

    for (y = 0; y < IAGNO_SIZE; y++) {
        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            set_message("Save file is incomplete.");
            update_ui();
            return;
        }
        for (x = 0; x < IAGNO_SIZE; x++) {
            if (line[x] < '0' || line[x] > '2') {
                fclose(f);
                set_message("Save file is invalid.");
                update_ui();
                return;
            }
            loaded.board[y][x] = (IagnoPiece) (line[x] - '0');
        }
    }
    fclose(f);

    loaded.turn = turn == IAGNO_WHITE ? IAGNO_WHITE : IAGNO_BLACK;
    app.game = loaded;
    app.game_over = FALSE;
    app.view_ply = -1;
    app.game.move_count = 0;
    iagno_game_init(&loaded);
    memcpy(app.game.history[0], app.game.board, sizeof(app.game.board));
    app.game.turn_history[0] = app.game.turn;
    app.game.black_count = 0;
    app.game.white_count = 0;
    for (y = 0; y < IAGNO_SIZE; y++) {
        for (x = 0; x < IAGNO_SIZE; x++) {
            if (app.game.board[y][x] == IAGNO_BLACK)
                app.game.black_count++;
            else if (app.game.board[y][x] == IAGNO_WHITE)
                app.game.white_count++;
        }
    }
    snprintf(app.message, sizeof(app.message), "Loaded. %s to move.", piece_name(app.game.turn));
    update_ui();
}

static void mode_changed(GtkComboBox *combo, gpointer data)
{
    (void) data;
    app.mode = (AppMode) gtk_combo_box_get_active(combo);
    new_game();
}

static void level_changed(GtkComboBox *combo, gpointer data)
{
    (void) data;
    app.level = gtk_combo_box_get_active(combo);
    if (app.level < 0)
        app.level = 1;
    snprintf(app.message, sizeof(app.message), "Difficulty changed. %s to move.", piece_name(app.game.turn));
    update_ui();
}

static void set_view_ply(int ply)
{
    if (app.ai_source) {
        g_source_remove(app.ai_source);
        app.ai_source = 0;
    }

    if (ply < 0)
        ply = 0;
    if (ply > app.game.move_count)
        ply = app.game.move_count;

    app.view_ply = (ply == app.game.move_count) ? -1 : ply;
    update_ui();
}

static void history_first_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    set_view_ply(0);
}

static void history_prev_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    set_view_ply(current_view_ply() - 1);
}

static void history_next_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    set_view_ply(current_view_ply() + 1);
}

static void history_latest_cb(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    set_view_ply(app.game.move_count);
}

static GtkWidget *combo_with_items(const char **items, int active)
{
    GtkWidget *combo = gtk_combo_box_new_text();
    int i;

    for (i = 0; items[i] != NULL; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo), items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    gtk_widget_set_size_request(combo, 300, -1);
    app_apply_high_contrast(combo);
    return combo;
}

static GtkWidget *labeled_combo(GtkWidget *row, const char *label_text, GtkWidget *combo)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 4);
    GtkWidget *label = gtk_label_new(label_text);

    gtk_box_pack_start(GTK_BOX(row), box, TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    gtk_widget_set_size_request(label, 130, -1);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
    return combo;
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    (void) widget;
    (void) data;

    if (event->keyval == GDK_Escape || event->keyval == GDK_q) {
        gtk_main_quit();
        return TRUE;
    }
    if (event->keyval == GDK_r) {
        new_game();
        return TRUE;
    }
    if (event->keyval == GDK_u) {
        undo_cb(NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

static void add_button(GtkWidget *box, const char *label, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(button, "clicked", callback, NULL);
}

static GtkWidget *add_history_button(GtkWidget *box, const char *label, GCallback callback)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 0);
    g_signal_connect(button, "clicked", callback, NULL);
    return button;
}

static void build_ui_complete(void)
{
    static const char *modes[] = {"Play Black", "Play White", "2 Player", "AI Demo", NULL};
    static const char *levels[] = {"Easy", "Medium", "Hard", NULL};
    GtkWidget *vbox;
    GtkWidget *title;
    GtkWidget *controls;
    GtkWidget *settings;
    GtkWidget *content;
    GtkWidget *sidebar;
    GtkWidget *score_box;
    GtkWidget *history_frame;
    GtkWidget *history_scroll;
    GtkWidget *history_nav_box;

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), kindle_window_title());
    gtk_window_set_default_size(GTK_WINDOW(app.window), KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    gtk_window_move(GTK_WINDOW(app.window), 0, 0);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 8);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(key_press), NULL);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Kindle Iagno</b>");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    app.status = gtk_label_new("Black to move.");
    gtk_misc_set_alignment(GTK_MISC(app.status), 0.5f, 0.5f);
    gtk_box_pack_start(GTK_BOX(vbox), app.status, FALSE, FALSE, 0);

    controls = gtk_hbox_new(TRUE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), controls, FALSE, FALSE, 0);
    add_button(controls, "New", G_CALLBACK(new_game_cb));
    add_button(controls, "Undo", G_CALLBACK(undo_cb));
    add_button(controls, "Save", G_CALLBACK(save_cb));
    add_button(controls, "Load", G_CALLBACK(load_cb));
    add_button(controls, "Quit", G_CALLBACK(quit_cb));

    settings = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), settings, FALSE, FALSE, 0);
    app.mode_combo = combo_with_items(modes, 0);
    labeled_combo(settings, "Mode", app.mode_combo);
    g_signal_connect(app.mode_combo, "changed", G_CALLBACK(mode_changed), NULL);
    app.level_combo = combo_with_items(levels, 1);
    labeled_combo(settings, "Level", app.level_combo);
    g_signal_connect(app.level_combo, "changed", G_CALLBACK(level_changed), NULL);
    app.history_toggle_button = gtk_button_new_with_label("Hide Moves");
    gtk_box_pack_start(GTK_BOX(settings), app.history_toggle_button, FALSE, FALSE, 0);
    g_signal_connect(app.history_toggle_button, "clicked", G_CALLBACK(toggle_history_cb), NULL);

    content = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), content, TRUE, TRUE, 0);

    app.board = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.board, 760, 760);
    gtk_box_pack_start(GTK_BOX(content), app.board, TRUE, TRUE, 0);
    gtk_widget_add_events(app.board, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(app.board, "expose-event", G_CALLBACK(board_expose), NULL);
    g_signal_connect(app.board, "button-press-event", G_CALLBACK(board_button), NULL);

    sidebar = gtk_vbox_new(FALSE, 8);
    app.history_sidebar = sidebar;
    app.history_visible = TRUE;
    gtk_box_pack_start(GTK_BOX(content), sidebar, FALSE, TRUE, 0);

    score_box = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(sidebar), score_box, FALSE, FALSE, 0);
    app.black_score = gtk_label_new("Black: 2");
    app.white_score = gtk_label_new("White: 2");
    gtk_misc_set_alignment(GTK_MISC(app.black_score), 0.0f, 0.5f);
    gtk_misc_set_alignment(GTK_MISC(app.white_score), 0.0f, 0.5f);
    gtk_box_pack_start(GTK_BOX(score_box), app.black_score, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(score_box), app.white_score, FALSE, FALSE, 0);

    history_frame = gtk_frame_new("Moves");
    gtk_box_pack_start(GTK_BOX(sidebar), history_frame, TRUE, TRUE, 0);
    history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(history_frame), history_scroll);
    gtk_widget_set_size_request(history_scroll, 260, 700);
    app.moves_label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(app.moves_label), 0.0f, 0.0f);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(history_scroll), app.moves_label);

    history_nav_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sidebar), history_nav_box, FALSE, FALSE, 0);
    app.history_first_button = add_history_button(history_nav_box, "|<", G_CALLBACK(history_first_cb));
    app.history_prev_button = add_history_button(history_nav_box, "<", G_CALLBACK(history_prev_cb));
    app.history_next_button = add_history_button(history_nav_box, ">", G_CALLBACK(history_next_cb));
    app.history_latest_button = add_history_button(history_nav_box, ">|", G_CALLBACK(history_latest_cb));
}

static void build_canvas_ui(void)
{
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), kindle_window_title());
    gtk_window_set_default_size(GTK_WINDOW(app.window), KINDLE_APP_WIDTH, KINDLE_APP_HEIGHT);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    gtk_window_move(GTK_WINDOW(app.window), 0, 0);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_NONE);
    gtk_widget_add_events(app.window, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app.window, "key-press-event", G_CALLBACK(key_press), NULL);
    g_signal_connect(app.window, "expose-event", G_CALLBACK(canvas_expose), NULL);
    g_signal_connect(app.window, "button-press-event", G_CALLBACK(canvas_button), NULL);
    app_log("canvas ui built");
}

int main(int argc, char **argv)
{
    (void) argv;

    app_log("startup");
    gtk_init(&argc, &argv);
    app_install_kindle_style();
    app_log("gtk initialized");

    app.mode = MODE_PLAY_BLACK;
    app.level = 1;
    app.view_ply = -1;
    iagno_game_init(&app.game);
    set_message("Black to move.");
    build_ui_complete();
    update_ui();
    gtk_widget_show_all(app.window);
    gtk_window_present(GTK_WINDOW(app.window));
    app_log("window shown");
    gtk_main();
    app_log("shutdown");

    return 0;
}
