//
// Created by maciek on 1/7/22.
//

#ifndef NCURSES_SERVER_GAME_MAP_H
#define NCURSES_SERVER_GAME_MAP_H

typedef enum entity_t
{
    ENTITY_FREE = 0,
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
    entity_t entity_type;
    spawner_type spawnerType;
} map_point_t;

int map_init(map_point_t map[]);
#endif //NCURSES_SERVER_GAME_MAP_H