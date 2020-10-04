#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/common.h"
#include "frontend/gamelist.h"
#include "frontend/gamestate.h"

static void serialiseWriteCallback(void *ctx, const void *buf, int len)
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

static bool deserialiseReadCallback(void *ctx, void *buf, int len)
{
    struct serialise_buf *const rctx = ctx;

    if (len > rctx->len - rctx->pos)
        return false;

    memcpy(buf, rctx->buf + rctx->pos, len);
    rctx->pos += len;
    return true;
}

void gamestateSerialise(midend *me) {
    sfree(game_save.buf);
    game_save.buf = NULL;
    game_save.len = game_save.pos = 0;
    midend_serialise(me, serialiseWriteCallback, &game_save);
    gamestateInitialized = true;
}

const char *gamestateDeserialise(midend *me) {
    if (game_save.buf == NULL) return "No saved game state";
    game_save.pos = 0;
    return midend_deserialise(me, deserialiseReadCallback, &game_save);
}

const char *gamestateGamesaveName(char **name) {
    game_save.pos = 0;
    return identify_game(name, deserialiseReadCallback, &game_save);
}

void gamestateFree() {
    if (gamestateInitialized) {
        sfree(game_save.buf);
        gamestateInitialized = false;
    }
}

