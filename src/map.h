//
// Created by maciek on 1/7/22.
//



#ifndef NCURSES_SERVER_GAME_MAP_H
#define NCURSES_SERVER_GAME_MAP_H

#include <ncurses.h>

typedef enum entity_t
{
    ENTITY_UNKNOWN = 0,
    ENTITY_FREE ,
    ENTITY_WALL ,
    ENTITY_BUSH ,
    ENTITY_CAMPSITE ,
    ENTITY_PLAYER_1 ,
    ENTITY_PLAYER_2 ,
    ENTITY_PLAYER_3 ,
    ENTITY_PLAYER_4 ,
    ENTITY_BEAST ,
    ENTITY_COIN_SMALL ,
    ENTITY_COIN_BIG ,
    ENTITY_COIN_TREASURE ,
    ENTITY_COIN_DROPPED ,
} entity_t;

typedef enum spawner_type
{
    NO_SPAWNER = 0,
    COIN_SPAWNER ,
    BEAST_SPAWNER
} spawner_type;

typedef struct point_t
{
    unsigned int x;
    unsigned int y;
} point_t;

typedef struct map_point_t
{
    point_t point;
    entity_t point_display_entity;
    entity_t point_terrain_entity;
    spawner_type spawnerType;
} map_point_t;

struct entity_ncurses_attributes_t
{
    entity_t entity;
    chtype ch;
    int color_p; // which color pair is assigned to the given entity
};
int map_init(map_point_t map[], int MAP_WIDTH, int MAP_LENGTH);
int map_place_fow_player(map_point_t map[], const int MAP_WIDTH, const int MAP_LENGTH);
void ncurses_funcs_init();
void attribute_list_init();
int render_map(map_point_t map[], WINDOW * window, int MAP_WIDTH, int MAP_LENGTH);
void stat_window_display_server(WINDOW * window, int pid, int turn_cnt);
int stat_window_display_player(WINDOW * window, int pid ,int turn_counter, int carried, int brought);
#endif //NCURSES_SERVER_GAME_MAP_H
