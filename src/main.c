#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ncurses.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include "map.h"
#include <signal.h>
#include "fifohelper.h"

//TODO: CELE PROJEKTOWE:

// serwer powinnien odpowiadać za:
//0. inicjalizacje, załadowanie(bądź generowanie) planszy
//1. do serwera powinna docierac kazda operacja na mapie poprzez interakcje międzyprocesowe(przesylanie informacji poprzez FIFO)
//2. spawnowanie nowych agentów, istot, rzeczy na mapie
//3. zdefiniowanie autonomicznego zachowania bestii, każda bestia na nowym wątku
//4. wywoływanie procesów graczy

#define COIN_SPAWNER_NUM 40
#define BEAST_SPAWNER_NUM 20
#define DEBUG 0
#define NO_INPUT 1
#define MULTITHREADED_INPUT 1
#define TOTAL_BEAST_LIMIT 8
#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

//TODO these 2 parameters will be transferred to the players for error checking

//STUFF NECESSARY FOR MAP RENDERING AND VIEW
const int MAP_WIDTH = 32;
const int MAP_LENGTH = 32;
const int STAT_WINDOW_START_Y = 0;
const int STAT_WINDOW_START_X = 40;
extern struct entity_ncurses_attributes_t attribute_list[];

//STRUCTS NEEDED FOR THE MANAGAMENT OF COINS AND BEASTS
typedef struct beast_tracker_t
{
    map_point_t * beasts[TOTAL_BEAST_LIMIT];
    int size;
} beast_tracker_t;
typedef struct coin_spawn_manager_t
{
    map_point_t * spawner_array[COIN_SPAWNER_NUM];
    int free_spawners;
} coin_spawn_manager_t;

typedef struct beast_spawn_manager_t
{
    map_point_t * spawner_array[BEAST_SPAWNER_NUM];
    int free_spawners;
} beast_spawn_manager_t;

//TODO THEORETICAL SERVER STUFF: STRUCT
typedef struct server_side_player_t
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
} server_side_player_t;

typedef struct player_manager_t
{
    server_side_player_t players[4];
    int cur_size;
} player_manager_t;
//COIN MANAGEMENT STUFF
void spawn_coin(entity_t coin_size, map_point_t map[]);
int coin_spawn_init(map_point_t map[]);

//BEAST STUFF
int spawn_beast(beast_tracker_t * beast_tracker);
int beast_spawn_init(map_point_t map[]);
int beast_move(map_point_t map[], beast_tracker_t * beast_tracker, map_point_t * beast);
int handle_beasts(map_point_t map[], beast_tracker_t * beast_tracker);

//PLAYER STUFF
void player_manager_init();
void player_send_map_dimensions_internal(int which_player);
int player_send_map_update(map_point_t map[], int which_player, point_t curr_location);
void player_send_serverside_stats(int which_player, int carried, int brought);
void player_send_spawn_internal(map_point_t * map, int map_width, int map_length, int which_player);
void player_receive_move(map_point_t map[],int which_player, int map_width);
void player_send_turn(int turn_counter, int which_player);

void handle_event(int c, map_point_t map[], beast_tracker_t * beast_tracker);

//THREAD STUFF
void *input_routine(void *input_storage);
void *listening_join_routine(void * param);
void *listening_disconnect_routine(void * param);

coin_spawn_manager_t coinSpawnManager;
beast_spawn_manager_t beastSpawnManager;
player_manager_t playerManager;

#if MULTITHREADED_INPUT
pthread_mutex_t mutex_input = PTHREAD_MUTEX_INITIALIZER;
sem_t input_found_blockade;
#endif
pthread_mutex_t mutex_player_manag = PTHREAD_MUTEX_INITIALIZER;

bool user_did_not_quit = true;
bool listening_join_routine_not_terminated = true;
bool disconnect_routine_not_terminated = true;

int main() {
    srand((unsigned int) time(NULL));
    make_fifos();

    player_manager_init();
    map_point_t map[MAP_LENGTH * MAP_WIDTH];

    int err = map_init(map, MAP_WIDTH, MAP_LENGTH);
    if(err != 0)
        return 1;

    beast_tracker_t beast_tracker = { .size = 0};
    coin_spawn_init(map);
    beast_spawn_init(map);

    //required ncurses initialization functions
    ncurses_funcs_init();
    int server_pid = getpid();

    WINDOW * stats_window = newwin(10, 50, STAT_WINDOW_START_Y, STAT_WINDOW_START_X);
    WINDOW * game_window = newwin(MAP_WIDTH + 1, MAP_LENGTH + 1,0,0);

    refresh();
    box(game_window, 0, 0);
    box(stats_window, 0, 0);

    //assigning background color and letter color to every entity
    attribute_list_init();

    //pthread, sem related stuff
#if MULTITHREADED_INPUT
    int input = NO_INPUT;
    sem_init(&input_found_blockade, 0 ,0);
    pthread_t thread1;
    pthread_create(&thread1, NULL, input_routine, &input);
#endif
    pthread_t thread2;
    pthread_create(&thread2, NULL, listening_join_routine, map);
    pthread_t thread3;
    pthread_create(&thread3, NULL, listening_disconnect_routine, NULL);
    //MULTITHREADED_INPUT loop

#if MULTITHREADED_INPUT
    int turn_counter = 0;
    while(1)
    {
        pthread_mutex_lock(&mutex_input);
        if(input == (int) 'q')
        {
            sem_post(&input_found_blockade);
            break;
        }
        pthread_mutex_unlock(&mutex_input);

        pthread_mutex_lock(&mutex_input);
        if(input != NO_INPUT)
        {
            handle_event(input, map, &beast_tracker);
            pthread_mutex_unlock(&mutex_input);
            sem_post(&input_found_blockade);
        }
        else
        {
            pthread_mutex_unlock(&mutex_input);
        }

        handle_beasts(map, &beast_tracker);

        //handle_players
        for(int i = 0; i < 4; i++)
        {
            if( playerManager.players[i].is_free == false)
            {// each one of these functions is thread safe
                player_send_serverside_stats(i, playerManager.players[i].carried, playerManager.players[i].brought);
                player_send_map_update(map, i, playerManager.players[i].curr_location);
                player_receive_move(map, i, MAP_WIDTH);
            }
        }

        render_map(map, game_window, MAP_WIDTH, MAP_LENGTH);
        stat_window_display_server(stats_window, server_pid, turn_counter);

        wrefresh(game_window);
        wrefresh(stats_window);
        turn_counter++;
        usleep(1000 * 1000);
        for(int i = 0; i < 4; i++)
        {
            if( playerManager.players[i].is_free == false && playerManager.players[i].has_received_move == true &&
                playerManager.players[i].has_sent_map == true && playerManager.players[i].has_sent_stats == true)
            {// each one of these functions is thread safe
                player_send_turn(turn_counter, i);
            }
        }
    }
#else
    //singlethreaded mode: made for the ease of debugging
    int i = 0;
    while (user_did_not_quit)
    {
        render_map(map, game_window, MAP_WIDTH, MAP_LENGTH);
        stat_window_display_server(stats_window, server_pid, i);
        wrefresh(game_window);
        wrefresh(stats_window);
        int c = getch();
        flushinp();
        handle_event(c, map, &beast_tracker);
        handle_beasts(map, &beast_tracker);
        i++;
    }
#endif

    endwin();

    listening_join_routine_not_terminated = false;
    disconnect_routine_not_terminated = false;
    err = unlink_fifos();
    if(err == -1) printf("%s\n", strerror(errno));

    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
#if MULTITHREADED_INPUT
    pthread_join(thread1, NULL);

    pthread_mutex_destroy(&mutex_input);
    pthread_mutex_destroy(&mutex_player_manag);
    sem_destroy(&input_found_blockade);
#endif
    return 0;
}

//Keystroke handling: adding entities based on which key was pressed
void handle_event(int c, map_point_t map[], beast_tracker_t * beast_tracker)
{
    switch (c) {
        //arrow key handling:
        case KEY_RIGHT:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_UP:
        {
            break;
        }
        case 'B':
        case 'b':
        {
            spawn_beast(beast_tracker);
            break;
        }
        case 'c':
        {
            spawn_coin(ENTITY_COIN_SMALL, map);
            break;
        }
        case 'C':
        {
            spawn_coin(ENTITY_COIN_BIG, map);
            break;
        }
        case 'T':
        case 't':
        {
            spawn_coin(ENTITY_COIN_TREASURE, map);
            break;
        }
        case 'Q':
        case 'q':
        {
            user_did_not_quit = false;
            break;
        }
        default:
            break;
    }
}

#if MULTITHREADED_INPUT
void *input_routine(void *input_storage) {
    while (1) {
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
#endif

void *listening_join_routine(void * param)
{
    map_point_t * map = (map_point_t *) param;
    while(listening_join_routine_not_terminated)
    {
        int fd_read = open("fifo_p_to_s_init", O_RDONLY | O_NONBLOCK);
        int fd_write = open("fifo_s_to_p_init", O_WRONLY | O_NONBLOCK);
        int player_pid;

        if( fd_write != -1 && fd_read != -1 ) {
            int errs = 0;
            if ( read(fd_read, &player_pid, sizeof(int)) != sizeof(int)) {
                errs++;
            }

            pthread_mutex_lock(&mutex_player_manag);
            if (playerManager.cur_size < 4) {
                pthread_mutex_unlock(&mutex_player_manag);

                pthread_mutex_lock(&mutex_player_manag);
                int acc_var = 123;
                if( write(fd_write, &acc_var, sizeof(int) ) != sizeof(int ))
                {   //for debug purposes
                    errs++;
                }
                playerManager.cur_size = playerManager.cur_size + 1;
                int i;
                for (i = 0; i < 4; i++) {
                    if (playerManager.players[i].is_free == true) {
                        break;
                    }
                }
                //cppcheck bothered me about this
                if (i == 4) i--;

                playerManager.players[i].process_id = getpid();
                playerManager.players[i].is_free = false;
                if(write(fd_write, &playerManager.players[i], sizeof(server_side_player_t)) == sizeof(server_side_player_t))
                {   //for debug purposes
                    errs++;
                }

                int new_fd_write = open(playerManager.players[i].serv_to_p_fifo_name, O_WRONLY);
                int new_fd_read = open(playerManager.players[i].p_to_serv_fifo_name, O_RDONLY);
                playerManager.players[i].serv_to_p_fd = new_fd_write;
                playerManager.players[i].p_to_serv_fd = new_fd_read;
                playerManager.players[i].process_id = player_pid;

                player_send_map_dimensions_internal(i);
                player_send_spawn_internal(map, MAP_WIDTH, MAP_LENGTH, i);
                pthread_mutex_unlock(&mutex_player_manag);
            } else {

                pthread_mutex_unlock(&mutex_player_manag);
                int rej_var = 234;
                if(write(fd_write, &rej_var, sizeof(int)) == sizeof(int ))
                {   //for debug purposes
                    errs++;
                }
            }

            close(fd_write);
            close(fd_read);
        }
        usleep(250 * 1000);
    }
    return NULL;
}

void * listening_disconnect_routine(void * params)
{
    while(disconnect_routine_not_terminated)
    {
        pthread_mutex_lock(&mutex_player_manag);

        for(int i = 0; i < 4; i++)
        {
            if( playerManager.players[i].is_free == false)
            { // checking if the process is still alive
                int err = kill(playerManager.players[i].process_id, 0);
                if(err == -1 && errno == ESRCH) //process doesnt exist
                {
                    playerManager.players[i].is_free = true;
                    playerManager.cur_size--;
                }
            }
        }
        pthread_mutex_unlock(&mutex_player_manag);
        usleep(500 * 1000);
    }
    return NULL;
}

void player_manager_init()
{
    for(int i = 0; i < 4; i++)
    {
        playerManager.players[i].is_free = true;
        playerManager.players[i].player_num = i;

        switch (i)
        {
            case 0:
            {
                strcpy(playerManager.players[i].p_to_serv_fifo_name, "fifo_p_to_s1");
                strcpy(playerManager.players[i].serv_to_p_fifo_name, "fifo_s_to_p1");
                break;
            }
            case 1:
            {
                strcpy(playerManager.players[i].p_to_serv_fifo_name, "fifo_p_to_s2");
                strcpy(playerManager.players[i].serv_to_p_fifo_name, "fifo_s_to_p2");
                break;
            }
            case 2:
            {
                strcpy(playerManager.players[i].p_to_serv_fifo_name, "fifo_p_to_s3");
                strcpy(playerManager.players[i].serv_to_p_fifo_name, "fifo_s_to_p3");
                break;
            }
            case 3:
            {
                strcpy(playerManager.players[i].p_to_serv_fifo_name, "fifo_p_to_s4");
                strcpy(playerManager.players[i].serv_to_p_fifo_name, "fifo_s_to_p4");
                break;
            }
            default:
                break;
        }
    }
}

void player_send_map_dimensions_internal(int which_player)
{
    // as this function is internal, they dont need mutexes, to lock shared resources.
    // the calling function does need them, however.
    int buf = MAP_WIDTH;
    int err;
    err = write(playerManager.players[which_player].serv_to_p_fd, &buf, sizeof(int) );
    if( err != sizeof(int))
    {
        return;
    }


    buf = MAP_LENGTH;
    err = write(playerManager.players[which_player].serv_to_p_fd, &buf, sizeof(int) );
    if( err != sizeof(int))
    {
        return;
    }
}
int player_send_map_update(map_point_t map[], int which_player, point_t curr_location)
{
    const int map_fragment_size = 26;
    map_point_t map_fragment[map_fragment_size];
    int array_cnt = 0;
    for(int i = -2; i < 3; i++) {
        if ((int) curr_location.y + i >= 0 && (int) curr_location.y + i <= MAP_WIDTH) {
            for (int j = -2; j < 3; j++) {
                if ((int) curr_location.x + j >= 0 && (int) curr_location.x + j <= MAP_LENGTH)
                {
                    if ((int) curr_location.x + i == MAP_LENGTH || (int) curr_location.y + j == MAP_WIDTH)
                    {
                        map_fragment[array_cnt].point.y = 999;
                        map_fragment[array_cnt].point.x = 999;
                        array_cnt++;
                    } else {
                        map_fragment[array_cnt] = map[(curr_location.y + i )* MAP_WIDTH + curr_location.x + j];
                        array_cnt++;
                    }
                }
            }
        }
    }
    map_fragment[array_cnt].point.y = 999;
    map_fragment[array_cnt].point.x = 999;

    //error checking of the map
    for(int i = 0; i < map_fragment_size; i++)
    {
        if( map_fragment[i].point.y == 999 && map_fragment[i].point.x == 999)
            break;
        if( map_fragment[i].point.x > 32 || map_fragment[i].point.y > 32 ||
            map_fragment[i].point_display_entity > ENTITY_COIN_DROPPED || map_fragment[i].point_terrain_entity > ENTITY_COIN_DROPPED ||
            map_fragment[i].point_display_entity == ENTITY_UNKNOWN || map_fragment[i].point_terrain_entity == ENTITY_UNKNOWN)
        {
            return -1;
        }
    }
    pthread_mutex_lock(&mutex_player_manag);
    int err;
    err = write(playerManager.players[which_player].serv_to_p_fd, map_fragment, sizeof(map_point_t) * map_fragment_size);
    if( err != sizeof(map_point_t) * map_fragment_size)
    {
        pthread_mutex_unlock(&mutex_player_manag);
        return -1;
    }
    pthread_mutex_unlock(&mutex_player_manag);

    playerManager.players[which_player].has_sent_map = true;
    return 0;
}

void player_send_serverside_stats(int which_player, int carried, int brought)
{
    pthread_mutex_lock( &mutex_player_manag);
    int err = write(playerManager.players[which_player].serv_to_p_fd, &brought, sizeof(int));
    if(err != sizeof(int))
    {
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = write(playerManager.players[which_player].serv_to_p_fd, &carried, sizeof(int) );
    if(err != sizeof(int))
    {
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    pthread_mutex_unlock(&mutex_player_manag);
    playerManager.players[which_player].has_sent_stats = true;
}
void player_receive_move(map_point_t map[], int which_player, int map_width)
{
    point_t old_loc;
    point_t new_loc;
    pthread_mutex_lock(&mutex_player_manag);
    int err = read(playerManager.players[which_player].p_to_serv_fd, &old_loc, sizeof(point_t));
    if ( err != sizeof(point_t)) {
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = read(playerManager.players[which_player].p_to_serv_fd, &new_loc, sizeof(point_t));
    if ( err != sizeof(point_t)) {
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    pthread_mutex_unlock(&mutex_player_manag);

    //
    if(new_loc.y > MAP_WIDTH || new_loc.x > MAP_LENGTH){
        return;
    }
    int total_distance_x = abs(new_loc.x - old_loc.x);
    int total_distance_y = abs(new_loc.y - old_loc.y);
    if(total_distance_x + total_distance_y > 2)
    {
        return;
    }
    if( map[new_loc.y * MAP_WIDTH + new_loc.x].point_display_entity == ENTITY_WALL) {
        return;
    }

    pthread_mutex_lock(&mutex_player_manag);
    int old_x = playerManager.players[which_player].curr_location.x;
    int old_y = playerManager.players[which_player].curr_location.y;

    map[old_y * map_width + old_x].point_display_entity = map[old_x * map_width + old_x].point_terrain_entity;
    playerManager.players[which_player].curr_location = new_loc;
    entity_t player_num = attribute_list[which_player + 5].entity;
    map[new_loc.y * map_width + new_loc.x].point_display_entity = player_num;

    pthread_mutex_unlock(&mutex_player_manag);
    playerManager.players[which_player].has_received_move = true;
}

void player_send_spawn_internal(map_point_t * map, int map_width, int map_length, int which_player)
{
    // as this function is internal, they dont need mutexes, to lock shared resources.
    // the calling function does need them, however.
    int baseline_x = map_length / 4;
    int baseline_y = map_width / 4;

    int spawn_loc_x;
    int spawn_loc_y;
    while(1)
    {
        spawn_loc_x = baseline_x + rand() % (map_length / 2);
        spawn_loc_y = baseline_y + rand() % (map_width / 2);

        if (map[spawn_loc_y * map_width + spawn_loc_x].point_display_entity == ENTITY_FREE
        && map[spawn_loc_y * map_width + spawn_loc_x].spawnerType == NO_SPAWNER)
            break;
    }

    entity_t plr = attribute_list[which_player + 5].entity;
    map_point_t temp = { .point.x = (unsigned) spawn_loc_x, .point.y = (unsigned) spawn_loc_y,
                         .point_display_entity = plr, .point_terrain_entity = ENTITY_FREE, .spawnerType = NO_SPAWNER};
    int err = write(playerManager.players[which_player].serv_to_p_fd, &temp, sizeof(map_point_t));
    if(err != sizeof(temp))
    {
        return;
    }
    playerManager.players[which_player].curr_location.y = (unsigned) spawn_loc_y;
    playerManager.players[which_player].curr_location.x = (unsigned) spawn_loc_x;
    playerManager.players[which_player].spawn_location.y = (unsigned) spawn_loc_y;
    playerManager.players[which_player].spawn_location.x = (unsigned) spawn_loc_x;
    map[spawn_loc_y * map_width + spawn_loc_x].point_display_entity = temp.point_display_entity;
}

void player_send_turn(int turn_counter, int which_player)
{
    int buf = turn_counter;
    pthread_mutex_lock(&mutex_player_manag);
    int err;
    err = write(playerManager.players[which_player].serv_to_p_fd, &buf, sizeof(int ));
    if(err != sizeof(int ))
    {
        fprintf(stderr, "Runtime error: %s returned -1 at %s:%d", strerror(errno),__FILE__, __LINE__);
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    pthread_mutex_unlock(&mutex_player_manag);
    playerManager.players[which_player].has_received_move = false;
    playerManager.players[which_player].has_sent_map = false;
    playerManager.players[which_player].has_sent_stats = false;
}

// Function that initializes the beastManager variable
int beast_spawn_init(map_point_t map[])
{
    unsigned long total_spawners = 0;
    unsigned long free_spawners = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].point_display_entity == ENTITY_FREE && map[i].spawnerType == BEAST_SPAWNER)
        {
            total_spawners++;
            free_spawners++;
        }
    }

    beastSpawnManager.free_spawners = (int) free_spawners;

    int arr_cnt = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].point_display_entity == ENTITY_FREE && map[i].spawnerType == BEAST_SPAWNER)
        {
            beastSpawnManager.spawner_array[arr_cnt] = &map[i];
            arr_cnt++;
        }
    }
    return 0;
}
int spawn_beast(beast_tracker_t * beast_tracker)
{
    if(beast_tracker->size < 0 || beast_tracker->size == TOTAL_BEAST_LIMIT || beastSpawnManager.free_spawners == 0)
    {
        return -1;
    }
    int which_beast_spawner = rand() % beastSpawnManager.free_spawners;
    beastSpawnManager.spawner_array[which_beast_spawner]->point_display_entity = ENTITY_BEAST;
    int loc = beastSpawnManager.free_spawners - 1;
    swap(beastSpawnManager.spawner_array[which_beast_spawner], beastSpawnManager.spawner_array[loc]);
    beastSpawnManager.free_spawners--;

    beast_tracker->beasts[beast_tracker->size] = beastSpawnManager.spawner_array[loc];
    beast_tracker->size = beast_tracker->size + 1;

    return 0;
}
int beast_move( map_point_t map[],  beast_tracker_t * beast_tracker, map_point_t * beast)
{
    //TODO Player collision, Player chasing, fixing the rand check so that the beast doesnt move
    //error checking
    if( beast->point_display_entity != ENTITY_BEAST) {
        return -1;
    }
    int beast_tracker_index = 0;
    bool beast_found = false;
    for(int i = 0; i < beast_tracker->size; i++)
    {
        if( beast_tracker->beasts[i] == beast)
        {
            beast_found = true;
            break;
        }
        beast_tracker_index++;
    }

    if(!beast_found)
        return -1;

    //checking which of the 4 sides are occupied
    map_point_t * possible_locs[4];
    int locs_cnt = 0;
    int b_x = (int) beast->point.x;
    int b_y = (int) beast->point.y;
    for(int i = -1; i < 2; i++)
    {
        for(int j = -1; j < 2; j++)
        {
            if( (i == -1 && j == 0) || (i == 1 && j == 0) || (i == 0 && j == -1) || (i == 0 && j == 1)  )
            {
                if (map[(b_y + i) * MAP_WIDTH + b_x + j].point_display_entity == ENTITY_FREE)
                {
                    possible_locs[locs_cnt] = &map[(b_y + i) * MAP_WIDTH + b_x + j];
                    locs_cnt++;
                }
            }
        }
    }
    if(locs_cnt == 0)
        return 0;

    int which_loc = rand() % locs_cnt;
    possible_locs[which_loc]->point_display_entity = ENTITY_BEAST;
    beast->point_display_entity = ENTITY_FREE;

    beast_tracker->beasts[beast_tracker_index] = possible_locs[which_loc];
    return 0;
}

int handle_beasts(map_point_t map[], beast_tracker_t * beast_tracker)
{
    for(int i = 0; i < beast_tracker->size; i++)
    {
        beast_move(map, beast_tracker, beast_tracker->beasts[i]);
    }
    return 0;
}


// Function that initializes the coinManager variable which allows oversight over all the coin spawns over the map
int coin_spawn_init(map_point_t map[])
{
    unsigned long free_spawners = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].point_display_entity == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
        {
            free_spawners++;
        }
    }

    coinSpawnManager.free_spawners = (int) free_spawners;

    int arr_cnt = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].point_display_entity == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
        {
            coinSpawnManager.spawner_array[arr_cnt] = &map[i];
            arr_cnt++;
        }
    }
    return 0;
}

void spawn_coin(entity_t coin_size, map_point_t map[])
{
    if( coin_size < ENTITY_COIN_SMALL || coinSpawnManager.free_spawners == 0)
    {
        return;
    }

    int which_spawner_to_use = rand() % coinSpawnManager.free_spawners;
    int y = (int )coinSpawnManager.spawner_array[which_spawner_to_use]->point.y;
    int x = (int) coinSpawnManager.spawner_array[which_spawner_to_use]->point.x;
#if DEBUG
    if(x > 32 || y > 32 || y < 0 || x < 0 )
    {
        printf("a");
    }
#endif
    map[y * MAP_WIDTH + x].point_display_entity = coin_size;

    int free_spwn_cnt = coinSpawnManager.free_spawners;
    swap(coinSpawnManager.spawner_array[which_spawner_to_use], coinSpawnManager.spawner_array[free_spwn_cnt - 1]);
    coinSpawnManager.free_spawners--;

}
