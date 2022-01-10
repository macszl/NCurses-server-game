//
// Created by maciek on 1/7/22.
//

#include "map.h"
#include <stdio.h>

const int MAP_LENGTH = 32;
const int MAP_WIDTH = 32;

struct entity_ncurses_attributes_t attribute_list[] =
        {
                {.entity = ENTITY_FREE, .ch = '.', .color_p = 5},
                {.entity = ENTITY_WALL, .ch = 'O', .color_p = 2},
                {.entity = ENTITY_BUSH, .ch = '$', .color_p = 5},
                {.entity = ENTITY_CAMPSITE, .ch = '*', .color_p = 4},
                {.entity = ENTITY_PLAYER_1, .ch = '1', .color_p = 3},
                {.entity = ENTITY_PLAYER_2, .ch = '2', .color_p = 3},
                {.entity = ENTITY_PLAYER_3, .ch = '3', .color_p = 3},
                {.entity = ENTITY_PLAYER_4, .ch = '4', .color_p = 3},
                {.entity = ENTITY_BEAST, .ch = '@', .color_p = 6},
                {.entity = ENTITY_COIN_SMALL, .ch = 'c', .color_p = 1},
                {.entity = ENTITY_COIN_BIG, .ch = 'C' , .color_p = 1},
                {.entity = ENTITY_COIN_TREASURE, .ch ='T', .color_p = 1},
                {.entity = ENTITY_COIN_DROPPED, .ch = 'D', .color_p = 1}
        };


int map_init(map_point_t map[]) {

    //4x4 test map initialization
//    for(int i = 0; i < MAP_WIDTH; i++)
//    {
//        for(int j = 0; j < MAP_LENGTH; j++)
//        {
//            if( i == 0 || j == 0 || i == 3 || j == 3)
//            {
//                map[i * MAP_WIDTH + j].point.y = (unsigned int ) i;
//                map[i * MAP_WIDTH + j].point.x = (unsigned int ) j;
//                map[i * MAP_WIDTH + j].entity_type = ENTITY_WALL;
//            }
//        }
//    }
//
//    map[3+1].entity_type = ENTITY_FREE;

    //larger map - initialization from a file
    FILE *fptr = fopen("map.bin", "rb");
    if (!fptr)
    {
        return -1;
    }

    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        fread(&map[i], sizeof(map_point_t), 1, fptr);
    }
    fclose(fptr);

    return 0;
}

int render_map(map_point_t map[], WINDOW * window)
{
    for(int i = 0; i < MAP_WIDTH; i++)
    {
        for(int j = 0; j < MAP_LENGTH; j++)
        {
            int ent_index = (int) map[i * MAP_WIDTH + j].entity_type;
            mvwaddch(window ,i, j, attribute_list[ent_index].ch);
        }
    }
    return 0;
}
void ncurses_funcs_init()
{
    initscr();
    noecho();
    start_color();
    keypad(stdscr, true);
}

void attribute_list_init()
{
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_YELLOW);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);
    init_pair(6, COLOR_BLACK, COLOR_RED);
    for(int i = 0; i < 13; i++)
    {
        int col = attribute_list[i].color_p;
        attribute_list[i].ch = attribute_list[i].ch | A_REVERSE | COLOR_PAIR(col);
    }
}