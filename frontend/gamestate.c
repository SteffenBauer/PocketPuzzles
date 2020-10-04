#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/common.h"
#include "frontend/gamelist.h"
#include "frontend/gamestate.h"

static void serialise_write_callback(void *ctx, const void *buf, int len)
{
    struct serialise_buf *ser = (struct serialise_buf *)ctx;
    int new_len;

    new_len = ser->len + len;
    if (new_len > ser->pos) {
        ser->pos = new_len + new_len / 4 + 1024;
        ser->buf = sresize(ser->buf, ser->pos, char);
    }
    memcpy(ser->buf + ser->len, buf, len);
    ser->len = new_len;
}

static bool deserialise_read_callback(void *ctx, void *buf, int len)
{
    struct serialise_buf *const rctx = ctx;

    if (len > rctx->len - rctx->pos)
        return false;

    memcpy(buf, rctx->buf + rctx->pos, len);
    rctx->pos += len;
    return true;
}

void serialise_game(midend *me) {
    sfree(game_save.buf);
    game_save.buf = NULL;
    game_save.len = game_save.pos = 0;
    midend_serialise(me, serialise_write_callback, &game_save);
}

const char *deserialise_game(midend *me) {
    if (game_save.buf == NULL) return "No saved game state";
    game_save.pos = 0;
    return midend_deserialise(me, deserialise_read_callback, &game_save);
}

const char *get_gamesave_name(char **name) {
    game_save.pos = 0;
    return identify_game(name, deserialise_read_callback, &game_save);
}

void free_gamestate() {
    sfree(game_save.buf);
}

