#ifndef LUDO_H
#define LUDO_H

#include <stdbool.h>
#include <stdint.h>

#define LUDO_PLAYERS 4
#define LUDO_TOKENS 2
#define LUDO_TRACK 52
#define LUDO_CODE 7
#define LUDO_MAX_ROOMS 64
#define LUDO_NAME 32

typedef enum { RED, GREEN, YELLOW, BLUE } LudoColor;
typedef enum { TOKEN_BASE, TOKEN_TRACK, TOKEN_FINISHED } TokenState;
typedef struct { TokenState state; int step; } LudoToken;
typedef struct { char name[LUDO_NAME], owner[LUDO_NAME+8]; bool bot, connected, done; int rank; LudoToken token[LUDO_TOKENS]; } LudoPlayer;
typedef struct {
    LudoPlayer player[LUDO_PLAYERS]; int count, current, die, ranks, human_count;
    bool rolled, started, over, private_room, public_room;
    char code[LUDO_CODE];
} LudoRoom;

void ludo_init(LudoRoom *g, const char *code);
int ludo_roll(LudoRoom *g, int forced_die);
bool ludo_move(LudoRoom *g, int token_index);
bool ludo_valid_move(const LudoRoom *g, int player, int token_index, int die);
int ludo_track_square(int player, int step);
bool ludo_safe_square(int track_square);
int ludo_room_create(LudoRoom *rooms, const char *name, bool private_room, bool public_room);
int ludo_room_join(LudoRoom *room, const char *name);
void ludo_room_start(LudoRoom *room);
void ludo_disconnect(LudoRoom *room, int player);
int ludo_bot_move(const LudoRoom *room, int player, int die);

#endif
