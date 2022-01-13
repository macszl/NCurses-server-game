//
// Created by maciek on 1/12/22.
//

#ifndef NCURSES_SERVER_GAME_PLAYER_H
#define NCURSES_SERVER_GAME_PLAYER_H

#include "map.h"

typedef enum dir_t {
    LEFT = 0,
    RIGHT,
    UP,
    DOWN
} dir_t;

typedef struct player_t {
    map_point_t * player;
    map_point_t * spawn;
} player_t;

void handle_event(int c, map_point_t map[]);
void spawn_player(player_t * player, map_point_t * map, int map_width, int map_length);
void player_move_human(dir_t, player_t * player);
void player_move_cpu(player_t * player);

#endif //NCURSES_SERVER_GAME_PLAYER_H
