//
// Created by maciek on 1/12/22.
//

#include "player.h"
#include <stdlib.h>
#include <time.h>


#define CPU_INPUT -9
const int MAX_POSSIBLE_MAP_WIDTH = 32; //those need to be 128 later on, or removed
const int MAX_POSSIBLE_MAP_LENGTH = 32;

bool CPU_MODE = false;

int main() {
    srand(time(NULL));
    //320 kb array allocated on the stack, will need to replace later with a heap allocated object
    map_point_t map[MAX_POSSIBLE_MAP_LENGTH * MAX_POSSIBLE_MAP_WIDTH];


    int err = map_init(map, MAX_POSSIBLE_MAP_WIDTH, MAX_POSSIBLE_MAP_LENGTH);
    if(err != 0)
        return 1;

    ncurses_funcs_init();

    int stat_window_start_x = MAX_POSSIBLE_MAP_LENGTH + 8;
    int stat_window_start_y = 0;
    WINDOW * stats_window = newwin(10, 50, stat_window_start_y, stat_window_start_x);
    WINDOW * game_window = newwin(MAX_POSSIBLE_MAP_WIDTH + 1, MAX_POSSIBLE_MAP_LENGTH + 1,0,0);
    int c;
    while(1)
    {
        render_map(map, game_window, MAX_POSSIBLE_MAP_WIDTH, MAX_POSSIBLE_MAP_LENGTH);
        if(CPU_MODE == false)
        {
            c = getch();
            handle_event(c, map);
        }
        else
        {
            handle_event(CPU_INPUT, map);
        }
    }
    return 0;
}

void handle_event(int c, map_point_t map[])
{
    switch (c) {
        //arrow key handling: works only with unix distros as unix represents arrow keys as 3 characters
        case KEY_RIGHT:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_UP:
        {
            break;
        }
        case 'a':
        {
            CPU_MODE = !CPU_MODE;
            break;
        }
        default:
            break;
    }
}

void spawn_player(player_t * player, map_point_t * map, int map_width, int map_length)
{

    int baseline_x = map_length / 4;
    int baseline_y = map_width / 4;

    int spawn_loc_x;
    int spawn_loc_y;
    while(1)
    {
        spawn_loc_x = baseline_x + rand() % (map_length / 2);
        spawn_loc_y = baseline_y + rand() % (map_width / 2);

        if (map[spawn_loc_y * map_width + spawn_loc_x].point_display_entity == ENTITY_FREE)
            break;
    }

    player->spawn = &map[spawn_loc_y * map_width + spawn_loc_x];
    player->player = player->spawn;
}
void player_move_human(dir_t dir, player_t * player)
{
    if ( dir == LEFT)
    {

    }
    else if( dir == RIGHT)
    {

    }
    else if( dir == UP)
    {

    }
    else if( dir == DOWN)
    {

    }
}

void player_move_cpu(player_t * player)
{

}