#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "inkview.h"
#include "frontend/common.h"
#include "frontend/gamelist.h"
#include "frontend/state.h"
#include "puzzles.h"

const char *configFileName = STATEPATH "/sgtpuzzles.cfg";
/* const char *configFileName = STATEPATH "/sgtpuzzlesdev.cfg"; */
static dict_t *config = NULL;

int configLen() {
    int count = 0;
    dict_t *ptr;
    for (ptr = config; ptr != NULL; ptr = ptr->next)
        count++;
    return count;
}

void configAddItem(const char *key, const char *value) {
    dict_t *ptr;
    dict_t *new;
    configDelItem(key);
    for (ptr = config; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->key, key) == 0) {
            ptr->value = srealloc(ptr->value, strlen(value)+1);
            strcpy(ptr->value, value);
            return;
        }
    }
    new = smalloc(sizeof(struct dict_t_struct));
    new->key = smalloc(strlen(key)+1);
    new->value = smalloc(strlen(value)+1);
    strcpy(new->key, key);
    strcpy(new->value, value);
    new->next = config;
    config = new;
    return;
}

void configDel() {
    dict_t *ptr;
    while (config != NULL) {
        ptr = config;
        config = config->next;
        sfree(ptr->key);
        sfree(ptr->value);
        sfree(ptr);
    }
}

char *configGetItem(const char *key) {
    dict_t *ptr;
    for (ptr = config; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->key, key) == 0) {
            return ptr->value;
        }
    }
    return NULL;
}

void configDelItem(const char *key) {
    dict_t *ptr, *prev;
    ptr = config;
    prev = NULL;
    while(ptr != NULL) {
        if(strcmp(ptr->key, key) == 0) {
            if      (prev == NULL && ptr->next != NULL)
                config = ptr->next;
            else if (prev != NULL && ptr->next != NULL)
                prev->next = ptr->next;
            else if (prev == NULL && ptr->next == NULL)
                config = NULL;
            else
                prev->next = NULL;
            sfree(ptr->key);
            sfree(ptr->value);
            sfree(ptr);
            return;
        }
        prev = ptr;
        ptr = ptr->next;
    }
}

void configSave() {
    dict_t *ptr;
    FILE *fp = fopen(configFileName, "w");
    if (fp) {
        while (config != NULL) {
            ptr = config;
            config = config->next;
            fprintf(fp, "%s\t%s\n", ptr->key, ptr->value);
        }
        fclose(fp);
    }
}

void configLoad() {
  size_t buflen = 65536;
  size_t linelen;
  char *buf;
  char *value;
  char *key;
  FILE *fp;

  buf = smalloc(buflen);
  key = snewn(256, char);
  value = snewn(256, char);
  fp = fopen(configFileName, "r");
  if (fp != NULL) {
    while(getline(&buf, &buflen, fp) != EOF) {
      linelen = strlen(buf);
      if (linelen > 0) {
        value = sresize(value, linelen, char);
        sscanf(buf, "%s %s", key, value);
        if ((strncmp("params_",   key, 7) == 0) ||
            (strncmp("savegame",  key, 8) == 0) ||
            (strncmp("favorite_", key, 9) == 0) ||
            (strncmp("config_",   key, 7) == 0) ||
            (strncmp("settings_", key, 9) == 0))
            configAddItem(key, value);
      }
    }
    fclose(fp);
  }
  sfree(value);
  sfree(key);
  sfree(buf);
}

static void serialiseWriteCallback(void *ctx, const void *buf, int len)
{
    struct serialise_buf *ser = (struct serialise_buf *)ctx;
    int new_len;

    new_len = ser->len + len;
    if (new_len > ser->pos) {
        ser->pos = new_len + new_len / 4 + 1024;
        ser->buf = sresize(ser->buf, ser->pos, unsigned char);
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

void stateSerialise(midend *me) {
    char *buf;
    sfree(game_save.buf);
    game_save.buf = NULL;
    game_save.len = game_save.pos = 0;
    midend_serialise(me, serialiseWriteCallback, &game_save);
    buf = bin2hex(game_save.buf, game_save.len);
    configAddItem("savegame", buf);
    sfree(buf);
}

const char *stateDeserialise(midend *me) {
    char *buf;
    buf = configGetItem("savegame");
    if (buf != NULL) {
        sfree(game_save.buf);
        game_save.buf = hex2bin(buf, (strlen(buf)+1)/2);
        game_save.len = (strlen(buf)+1)/2;
        game_save.pos = 0;
        return midend_deserialise(me, deserialiseReadCallback, &game_save);
    }
    return "No saved gamestate";
}

const char *stateGamesaveName(char **name) {
    char *buf;
    buf = configGetItem("savegame");
    if (buf != NULL) {
        sfree(game_save.buf);
        game_save.buf = hex2bin(buf, (strlen(buf)+1)/2);
        game_save.len = (strlen(buf)+1)/2;
        game_save.pos = 0;
        return identify_game(name, deserialiseReadCallback, &game_save);
    }
    return "No saved gamestate";
}

void stateLoadParams(midend *me, const game *ourgame) {
    char key[128];
    char *value;
    sprintf(key, "params_%s", ourgame->name);
    value = configGetItem(key);
    if (value != NULL) {
        game_params *params = ourgame->default_params();
        ourgame->decode_params(params, value);
        midend_set_params(me, params);
        ourgame->free_params(params);
    }
}

void stateSaveParams(midend *me, const game *ourgame) {
    char key[128];
    char value[256];
    game_params *params = midend_get_params(me);
    sprintf(key, "params_%s", ourgame->name);
    sprintf(value, "%s", ourgame->encode_params(params, true));
    configAddItem(key, value);
    ourgame->free_params(params);
}

const char *stateLoadSettings(midend *me, const game *ourgame) {
    char key[128];
    char *buf;
    if (ourgame->has_preferences) {
        sprintf(key, "settings_%s", ourgame->name);
        buf = configGetItem(key);
        if (buf != NULL) {
            sfree(game_save.buf);
            game_save.buf = hex2bin(buf, (strlen(buf)+1)/2);
            game_save.len = (strlen(buf)+1)/2;
            game_save.pos = 0;
            return midend_load_prefs(me, deserialiseReadCallback, &game_save);
        }
        return "didn't find a setting in dict";
    }
    return "game has no preferences";
}

void stateSaveSettings(midend *me, const game *ourgame) {
    char key[128];
    char *buf;
    if (ourgame->has_preferences) {
        sfree(game_save.buf);
        game_save.buf = NULL;
        game_save.len = game_save.pos = 0;
        midend_save_prefs(me, serialiseWriteCallback, &game_save);
        buf = bin2hex(game_save.buf, game_save.len);
        sprintf(key, "settings_%s", ourgame->name);
        configAddItem(key, buf);
        sfree(buf);
    }
}

void stateSetFavorite(const char *name) {
    char key[128];
    sprintf(key, "favorite_%s", name);
    configAddItem(key, "favorite");
}

void stateUnsetFavorite(const char *name) {
    char key[128];
    sprintf(key, "favorite_%s", name);
    configDelItem(key);
}

bool stateIsFavorite(const char *name) {
    char key[128];
    char *entry;
    sprintf(key, "favorite_%s", name);
    entry = configGetItem(key);
    return (entry != NULL && (strcmp(entry, "favorite") == 0));
}

void stateInit() {
    configLoad();
    stateInitialized = true;
}

void stateFree() {
    if (stateInitialized) {
        configSave();
        sfree(game_save.buf);
        configDel();
        stateInitialized = false;
    }
}

