//
// Created by maciek on 1/7/22.
//

#include "map.h"
#include <stdio.h>

const int MAP_LENGTH = 32;
const int MAP_WIDTH = 32;

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
