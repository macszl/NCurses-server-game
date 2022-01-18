//
// Created by maciek on 1/12/22.
//

#ifndef NCURSES_SERVER_GAME_PLAYER_H
#define NCURSES_SERVER_GAME_PLAYER_H

#include "map.h"
#include "fifohelper.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>

typedef enum dir_t {
    LEFT = 0,
    RIGHT,
    UP,
    DOWN
} dir_t;

typedef struct player_t {
    point_t player_loc;
    point_t spawn_loc;
    entity_t which_player;
} player_t;

typedef struct stats_t {
    int carried;
    int brought;
} stats_t;
typedef struct server_info_t
{
    point_t curr_location;
    point_t spawn_location;
    char serv_to_p_fifo_name[20];
    char p_to_serv_fifo_name[20];
    int p_to_serv_fd;
    int serv_to_p_fd;
    int player_num;
    int process_id;
    bool is_free;
    int carried;
    int brought;
    bool has_sent_stats;
    bool has_sent_map;
    bool has_received_move;
} server_info_t;
int handle_event(int c, map_point_t map[], player_t * player, player_t * new_player, int map_width);
int player_move_human(dir_t, player_t * player, player_t * new_player);
int player_move_cpu(map_point_t* map, player_t * player, player_t * new_player, int map_width);

int server_receive_map_dimensions(int * map_width_p, int * map_length_p, int fd_read);
int server_receive_map_update(map_point_t * map, int fd_read, int map_width);
int server_receive_spawn(player_t * player,map_point_t * map, int fd_read , int map_width);
int server_receive_serverside_stats(stats_t * stats_p, int fd_read);
int server_send_move(player_t * player_before_move, player_t * player_after_move, int fd_write);
int server_receive_turn_counter(int * turn_counter_p, int fd_read);
#endif //NCURSES_SERVER_GAME_PLAYER_H
