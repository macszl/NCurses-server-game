//
// Created by maciek on 1/12/22.
//

#include "player.h"

#define CPU_INPUT (-9)
#define NO_INPUT 1
#define MAX_MAP_DIMENSION 128

#define CHECK(x, y) do { \
  int retval = (x); \
  if (retval == -1) { \
    fprintf(stderr, "Runtime error: %s returned %d at %s:%d", #x, retval, __FILE__, __LINE__); \
    goto y;\
  } \
} while (0)

bool CPU_MODE = true;
bool PLAYER_QUIT = false;

void *input_routine(void *input_storage);

pthread_mutex_t mutex_input = PTHREAD_MUTEX_INITIALIZER;
sem_t input_found_blockade;

int main() {
    ncurses_funcs_init();
    timeout(100);
    srand((unsigned int) time(NULL));

    make_fifos();
    pid_t pid = getpid();

    int input = NO_INPUT;
    sem_init(&input_found_blockade, 0 ,0);
    pthread_t thread1;
    pthread_create(&thread1, NULL, input_routine, &input);

    int fd_write = open("fifo_p_to_s_init", O_WRONLY);
    int fd_read = open("fifo_s_to_p_init", O_RDONLY);
    CHECK (write(fd_write, &pid, sizeof(int)) , error0);

    int response;
    CHECK (read(fd_read, &response, sizeof(int) ), error0 );
    if(response == 234)
    {
        close(fd_read);
        close(fd_write);
        printf("Server is full... Leaving.\n");
        return 0;
    }

    server_info_t temp;
    CHECK( read(fd_read, &temp, sizeof(server_info_t)), error0 );
    if(temp.p_to_serv_fifo_name[0] == 0)
    {
        printf("Error reading server info... Quitting.\n");
        goto error0;
    }


    close(fd_read);
    close(fd_write);
    int serv_process_id = temp.process_id;
    entity_t player_num = (unsigned int) ENTITY_PLAYER_1 + (unsigned int) temp.player_num;


    CHECK(fd_read = open(temp.serv_to_p_fifo_name, O_RDONLY), error0);
    CHECK (fd_write = open(temp.p_to_serv_fifo_name, O_WRONLY), error0);
    int map_length, map_width;

    CHECK ( server_receive_map_dimensions(&map_width, &map_length, fd_read), error0 );

    if(map_width != 32 || map_length != 32) goto error0;

    map_point_t * map = malloc(sizeof(map_point_t) * map_width * map_length);

    if(map == NULL) goto error0;

    map_place_fow_player(map, map_width, map_length);

    ncurses_funcs_init();
    int stat_window_start_x = map_length + 8, stat_window_start_y = 0;

    WINDOW * stats_window = newwin(10, 50, stat_window_start_y, stat_window_start_x);
    WINDOW * game_window = newwin(map_width + 1, map_length + 1,0,0);

    refresh();
    box(game_window, 0, 0);
    box(stats_window, 0, 0);
    attribute_list_init();

    stats_t stats;
    player_t player;
    CHECK(server_receive_spawn(&player, map,fd_read, map_width), error1);
    player.which_player = player_num;
    int turn_counter = 0;
    int iter = 0;
    while(!PLAYER_QUIT)
    {
        map_place_fow_player(map, map_width, map_length);
        wrefresh(game_window);
        wrefresh(stats_window);

        CHECK( server_receive_serverside_stats(&stats, fd_read), error1);
        wrefresh(game_window);
        wrefresh(stats_window);

        CHECK( server_receive_map_update(map, fd_read, map_width), error1);
        wrefresh(game_window);
        wrefresh(stats_window);

        pthread_mutex_lock(&mutex_input);
        if(input == (int) 'q' || input == (int) 'Q')
        {
            sem_post(&input_found_blockade);
            break;
        }
        else if(input == (int) 'a' || input == (int) 'A') {
            CPU_MODE = !CPU_MODE;
            break;
        }
        pthread_mutex_unlock(&mutex_input);

        pthread_mutex_lock(&mutex_input);
        if(CPU_MODE == true)
        {
            CHECK( handle_event( CPU_INPUT, map, &player, map_width), error1 );
            pthread_mutex_unlock(&mutex_input);
            sem_post(&input_found_blockade);
        }
        else
        {
            if(input != NO_INPUT)
            {
                CHECK (handle_event(input, map, &player, map_width), error1);
                pthread_mutex_unlock(&mutex_input);
                sem_post(&input_found_blockade);
            }
            else
            {
                pthread_mutex_unlock(&mutex_input);
            }
        }

        CHECK ( server_send_move( &player, fd_write), error1);
        wrefresh(game_window);
        wrefresh(stats_window);
        CHECK (render_map(map, game_window, map_width, map_length), error1 );
        wrefresh(game_window);
        wrefresh(stats_window);
        CHECK( stat_window_display_player(stats_window, serv_process_id, turn_counter, stats.carried, stats.brought), error1 );
        wrefresh(game_window);
        wrefresh(stats_window);
        CHECK( server_receive_turn_counter(&turn_counter, fd_read), error1);
        iter++;
    }

    close(fd_write);
    close(fd_read);
    free(map);
    PLAYER_QUIT = true;
    pthread_join(thread1, NULL);
    pthread_mutex_destroy(&mutex_input);
    sem_destroy(&input_found_blockade);
    endwin();
    return 0;

    error0:


    close(fd_write);
    close(fd_read);
    pthread_mutex_destroy(&mutex_input);
    sem_destroy(&input_found_blockade);
    endwin();
    return -1;

    error1:

    free(map);
    close(fd_read);
    close(fd_write);
    PLAYER_QUIT = true;
    pthread_join(thread1, NULL);
    pthread_mutex_destroy(&mutex_input);
    sem_destroy(&input_found_blockade);
    endwin();
    return -1;

}

int handle_event(int c, map_point_t map[], player_t * player, int map_width)
{
    switch (c) {
        case KEY_RIGHT:
            return player_move_human (map, RIGHT, player, map_width);
        case KEY_DOWN:
            return player_move_human (map, DOWN, player, map_width);
        case KEY_LEFT:
            return player_move_human (map, LEFT, player, map_width);
        case KEY_UP:
            return player_move_human (map, UP, player, map_width);
        case CPU_INPUT:
            return player_move_cpu (map, player, map_width);
        default:
            break;
    }
    return 0;
}

int player_move_human(map_point_t* map, dir_t dir, player_t * player, int map_width)
{

    int p_x = (int )player->player->point.x;
    int p_y = (int )player->player->point.y;
    if( p_y < 0 || p_x < 0 || p_x > MAX_MAP_DIMENSION || p_y > MAX_MAP_DIMENSION)
    {
        return -1;
    }

    if ( dir == LEFT && player->player->point.x > 0)
    {
        player->player = &map[ p_y * map_width + p_x - 1];
    }
    else if( dir == RIGHT && player->player->point.x < 31)
    {
        player->player = &map[ p_y * map_width + p_x + 1];
    }
    else if( dir == UP && player->player->point.y > 0)
    {
        player->player = &map[ (p_y - 1) * map_width + p_x];
    }
    else if( dir == DOWN && player->player->point.y < 31)
    {
        player->player = &map[ (p_y + 1)* map_width + p_x ];
    }
    else {
        return 0;
    }

    p_x = (int )player->player->point.x;
    p_y = (int )player->player->point.y;
    if( p_y < 0 || p_x < 0 || p_x > MAX_MAP_DIMENSION || p_y > MAX_MAP_DIMENSION)
    {
        return -1;
    }
    return 0;
}

int player_move_cpu(map_point_t* map, player_t * player, int map_width)
{
    if( player->player->point.y > MAX_MAP_DIMENSION || player->player->point.x > MAX_MAP_DIMENSION ||
        player->player->point_display_entity != player->which_player ||
        player->spawn->point.y > MAX_MAP_DIMENSION || player->spawn->point.x > MAX_MAP_DIMENSION)
    {
        return -1;
    }
    map_point_t * possible_locs[5];
    int locs_cnt = 1;
    int p_x = (int) player->player->point.x;
    int p_y = (int) player->player->point.y;

    if( p_x > MAX_MAP_DIMENSION || p_y > MAX_MAP_DIMENSION) {
        return -1;
    }
    for(int i = -1; i < 2; i++)
    {
        for(int j = -1; j < 2; j++)
        {
            if( (i == -1 && j == 0 && player->player->point.y > 0) || (i == 1 && j == 0 && player->player->point.y < 31)
             || (i == 0 && j == -1 && player->player->point.x > 0) || (i == 0 && j == 1 && player->player->point.x < 31)  )
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

    for(int i = 0; i < locs_cnt;i++)
    {
        if(possible_locs[i]->point.x > MAX_MAP_DIMENSION || possible_locs[i]->point.y > MAX_MAP_DIMENSION) {
            return -1;
        }
    }
    int which_loc = rand() % locs_cnt;
    possible_locs[which_loc]->point_display_entity = player->which_player;
    player->player->point_display_entity = ENTITY_FREE;
    player->player = possible_locs[which_loc];
    return 0;
}

int server_receive_map_dimensions(int * map_width_p, int * map_length_p, int fd_read)
{
    int buf;
    if ( read(fd_read, &buf, sizeof(int) ) != sizeof(int)) {
        return -1;
    }
    if(buf < 0 || buf > MAX_MAP_DIMENSION) {
        return -1;
    }
    *map_width_p = buf;

    read(fd_read, &buf, sizeof(int) );
    if(buf < 0 || buf > MAX_MAP_DIMENSION) {
        return -1;
    }
    *map_length_p = buf;
    return 0;
}
int server_receive_map_update(map_point_t * map, int fd_read, int map_width)
{
    const int map_fragment_size = 26;
    map_point_t map_fragment[map_fragment_size];
    int err;
    err = read(fd_read, map_fragment, sizeof(map_point_t) * map_fragment_size);
    if( err != sizeof(map_point_t) * map_fragment_size) {
        fprintf(stderr, "Runtime error: %s returned -1 at %s:%d", strerror(errno),__FILE__, __LINE__);
        return -1;
    }
    //error checking of the map
    for(int i = 0; i < map_fragment_size; i++)
    {
        if( map_fragment[i].point.y == 999 && map_fragment[i].point.x == 999)
            break;
        if( map_fragment[i].point.x > MAX_MAP_DIMENSION || map_fragment[i].point.y > MAX_MAP_DIMENSION ||
            map_fragment[i].point_display_entity > ENTITY_COIN_DROPPED || map_fragment[i].point_terrain_entity > ENTITY_COIN_DROPPED ||
            map_fragment[i].point_display_entity == ENTITY_UNKNOWN || map_fragment[i].point_terrain_entity == ENTITY_UNKNOWN)
        {
            return -1;
        }
    }
    for(int i = 0; i < map_fragment_size; i++)
    {
        unsigned int mapf_x = map_fragment[i].point.x;
        unsigned int mapf_y = map_fragment[i].point.y;
        if(mapf_x == 999 && mapf_y == 999) //our 'null terminator', used when against the wall
            break;
        map[(int) mapf_y * map_width + mapf_x] = map_fragment[i];
    }
    return 0;
}

int server_receive_spawn(player_t * player, map_point_t * map, int fd_read, int map_width)
{
    map_point_t buf;
    if (read(fd_read, &buf, sizeof(map_point_t )) != sizeof(map_point_t)) {
        return -1;
    }
    if(buf.point.y > MAX_MAP_DIMENSION || buf.point.x > MAX_MAP_DIMENSION) {
        return -1;
    }
    player->player = &map[buf.point.y * map_width + buf.point.x];
    player->spawn = &map[buf.point.y * map_width + buf.point.x];
    player->player->point_display_entity = buf.point_display_entity;
    player->player->point_terrain_entity = buf.point_terrain_entity;
    return 0;
}
int server_receive_serverside_stats(stats_t * stats_p, int fd_read)
{

    int buf;
    if (read(fd_read, &buf, sizeof(int) )!= sizeof(int)) {
        return -1;
    }
    stats_p->brought = buf;

    if (read(fd_read, &buf, sizeof(int) ) != sizeof(int)) {
        return -1;
    }
    stats_p->carried = buf;

    return 0;
}
int server_send_move(player_t * moved_player, int fd_write)
{
    if( moved_player->player->point.x > MAX_MAP_DIMENSION || moved_player->player->point.y > MAX_MAP_DIMENSION){
        return -1;
    }
    point_t temp;
    temp.y = moved_player->player->point.y;
    temp.x = moved_player->player->point.x;
    if (write(fd_write, &temp, sizeof(point_t)) != sizeof(point_t)) {
        return -1;
    }
    return 0;
}

int server_receive_turn_counter(int * turn_counter_p, int fd_read)
{
    int buf;
    if( read(fd_read, &buf, sizeof(int)) != sizeof(int ) ){
        return -1;
    }
    *turn_counter_p = buf;
    return 0;
}
void *input_routine(void *input_storage) {
    while (!PLAYER_QUIT) {
        pthread_mutex_lock(&mutex_input);
        *(int *) input_storage = NO_INPUT;
        pthread_mutex_unlock(&mutex_input);

        char c = (char) getch();
        flushinp();
        if( c != NO_INPUT) {
            pthread_mutex_lock(&mutex_input);
            *(int *) input_storage = (int) c;
            pthread_mutex_unlock(&mutex_input);
            sem_wait(&input_found_blockade);
            if ( c ==  'q' ) {
                break;
            }else {
                continue;
            }
        }
    }
    return NULL;
}

