//
// Created by maciek on 1/12/22.
//

#include "player.h"

#define CPU_INPUT -9

bool CPU_MODE = false;
bool PLAYER_QUIT = false;
int main() {
    srand((unsigned int) time(NULL));

    make_fifos();
    pid_t pid = getpid();

    int fd_write = open("fifo_p_to_s_init", O_WRONLY);
    int fd_read = open("fifo_s_to_p_init", O_RDONLY);
    write(fd_write, &pid, sizeof(int));

    int response;
    read(fd_read, &response, sizeof(int) );
    if(response == 234)
    {
        printf("Server is full... Leaving.\n");
        return 0;
    }

    server_info_t temp;
    int err = read(fd_read, &temp, sizeof(server_info_t));
    if(err == -1 || temp.p_to_serv_fifo_name[0] == 0)
    {
        printf("Error reading server info... Quitting.\n");
        return -1;
    }

    close(fd_read);
    close(fd_write);
    int serv_process_id = temp.process_id;
    int carried = 0;
    int brought = 0;
    entity_t player_num = (unsigned int) ENTITY_PLAYER_1 + (unsigned int) temp.player_num;

    fd_read = open(temp.serv_to_p_fifo_name, O_RDONLY);
    if(fd_read == -1)
    {
        printf("Failed to open server to player FIFO... Leaving.\n");
        return -1;
    }
    fd_write = open(temp.p_to_serv_fifo_name, O_WRONLY);
    if(fd_write == -1)
    {
        printf("Failed to open player to server FIFO... Leaving.\n");
        return -1;
    }

    int map_length;
    int map_width;


    server_receive_map_dimensions(&map_width, &map_length, fd_read);

    if(map_width != 32 || map_length != 32)
    {
        printf("Wrong map dimension data received... Leaving\n");
        return -1;
    }
    map_point_t * map = malloc(sizeof(map_point_t) * map_width * map_length);
    if(map == NULL)
    {
        printf("Failed to allocate memory\n");
        return -1;
    }
    map_place_fow_player(map, map_width, map_length);

    ncurses_funcs_init();
    int stat_window_start_x = map_length + 8;
    int stat_window_start_y = 0;
    WINDOW * stats_window = newwin(10, 50, stat_window_start_y, stat_window_start_x);
    WINDOW * game_window = newwin(map_width + 1, map_length + 1,0,0);

    stats_t stats;
    player_t player;
    server_receive_spawn(&player, map,fd_read, map_width, map_length);
    player.which_player = player_num;
    int c;
    while(!PLAYER_QUIT)
    {
        map_place_fow_player(map, map_width, map_length);
        server_receive_serverside_stats(&stats, fd_read);
        server_receive_map_update(map, fd_read, map_width, map_length);
        render_map(map, game_window, map_width, map_length);
        stat_window_display_player(stats_window, serv_process_id, carried, brought);
        wrefresh(game_window);
        wrefresh(stats_window);
        if(CPU_MODE == false)
        {
            c = getch();
            handle_event(c, map, &player, map_width);
        }
        else
        {
            handle_event(CPU_INPUT, map, &player, map_width);
        }
        server_send_move(player, fd_write);
    }
    close(fd_write);
    close(fd_read);
    free(map);
    return 0;
}

void handle_event(int c, map_point_t map[], player_t * player, int map_width)
{
    switch (c) {
        case KEY_RIGHT: {
            player_move_human (map, RIGHT, player, map_width);
            break;
        }
        case KEY_DOWN: {
            player_move_human (map, DOWN, player, map_width);
            break;
        }
        case KEY_LEFT: {
            player_move_human (map, LEFT, player, map_width);
            break;
        }
        case KEY_UP: {
            player_move_human (map, UP, player, map_width);
            break;
        }
        case 'A':
        case 'a': {
            CPU_MODE = !CPU_MODE;
            break;
        }
        case 'q':
        case 'Q': {
            PLAYER_QUIT = true;
            break;
        }
        case CPU_INPUT: {
            player_move_cpu (map, player, map_width);
            break;
        }
        default:
            break;
    }
}

void player_move_human(map_point_t* map, dir_t dir, player_t * player, int map_width)
{
    int p_x = (int )player->player->point.x;
    int p_y = (int )player->player->point.y;


    if ( dir == LEFT)
    {
        player->player = &map[ p_y * map_width + p_x - 1];
    }
    else if( dir == RIGHT)
    {
        player->player = &map[ p_y * map_width + p_x + 1];
    }
    else if( dir == UP)
    {
        player->player = &map[ (p_y - 1) * map_width + p_x];
    }
    else if( dir == DOWN)
    {
        player->player = &map[ (p_y + 1)* map_width + p_x ];
    }
}

void player_move_cpu(map_point_t* map, player_t * player, int map_width)
{

    map_point_t * possible_locs[5];
    int locs_cnt = 1;
    int p_x = (int) player->player->point.x;
    int p_y = (int) player->player->point.y;
    for(int i = -1; i < 2; i++)
    {
        for(int j = -1; j < 2; j++)
        {
            if( (i == -1 && j == 0) || (i == 1 && j == 0) || (i == 0 && j == -1) || (i == 0 && j == 1)  )
            {
                if (map[(p_y + i) * map_width + p_x + j].point_display_entity == ENTITY_FREE)
                {
                    possible_locs[locs_cnt] = &map[(p_y + i) * map_width + p_x + j];
                    locs_cnt++;
                }
            }
        }
    }
    possible_locs[0] = &map[p_y + p_x];

    int which_loc = rand() % locs_cnt;
    possible_locs[which_loc]->point_display_entity = player->which_player;
    player->player->point_display_entity = ENTITY_FREE;
    player->player = possible_locs[which_loc];
}

void server_receive_map_dimensions(int * map_width_p, int * map_length_p, int fd_read)
{
    int buf;
    read(fd_read, &buf, sizeof(int) );
    if(buf < 0 || buf > 128) {
        return;
    }
    *map_width_p = buf;

    read(fd_read, &buf, sizeof(int) );
    if(buf < 0 || buf > 128) {
        return;
    }
    *map_length_p = buf;
}
void server_receive_map_update(map_point_t * map, int fd_read, int map_width, int map_length)
{
    map_point_t map_fragment[26];
    read(fd_read, map_fragment, sizeof(map_point_t) * 26);

    for(int i = 0; i < 26; i++)
    {
        unsigned int x = map_fragment[i].point.x;
        unsigned int y = map_fragment[i].point.y;
        if(x == 999 && y == 999) //our 'null terminator', used when against the wall
            break;
        map[(int) y * map_width + map_length] =  map_fragment[i];
    }
}

void server_receive_spawn(player_t * player, map_point_t * map, int fd_read, int map_width, int map_length)
{
    point_t buf;
    read(fd_read, &buf, sizeof(point_t));
    player->player = &map[buf.y * map_width + buf.x];
    player->spawn = &map[buf.y * map_width + buf.x];

}
void server_receive_serverside_stats(stats_t * stats_p, int fd_read)
{
    int buf;
    read(fd_read, &buf, sizeof(int) );
    stats_p->brought = buf;

    read(fd_read, &buf, sizeof(int) );
    stats_p->carried = buf;
}
void server_send_move(player_t moved_player, int fd_write)
{
    point_t temp = moved_player.player->point;
    write(fd_write, &temp, sizeof(player_t));
}

