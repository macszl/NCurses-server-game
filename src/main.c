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


#define COIN_SPAWNER_NUM 40
#define BEAST_SPAWNER_NUM 20
#define DEBUG 0
#define NO_INPUT 1
#define TOTAL_BEAST_LIMIT 8
#define TOTAL_DROPPED_COIN_LIMIT 10
#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)


//STUFF NECESSARY FOR MAP RENDERING AND VIEW
const int MAP_WIDTH = 32;
const int MAP_LENGTH = 32;
const int STAT_WINDOW_START_Y = 0;
const int STAT_WINDOW_START_X = 40;
extern struct entity_ncurses_attributes_t attribute_list[];

//STRUCTS NEEDED FOR THE MANAGAMENT OF COINS AND BEASTS

typedef struct dropped_coin_t {
    point_t dropped_coin_loc;
    size_t val;
} dropped_coin_t;
typedef struct dropped_coin_manager_t {
    dropped_coin_t coins[TOTAL_DROPPED_COIN_LIMIT];
    int size;
} dropped_coin_manager_t;

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
    bool is_cpu;
    bool is_currently_in_a_bush;
    bool can_escape_bush;
    size_t carried;
    size_t brought;
    int deaths;
    bool has_sent_stats;
    bool has_sent_map;
    bool has_received_move;
} server_side_player_t;

typedef struct player_manager_t
{
    server_side_player_t players[4];
    int cur_size;
} player_manager_t;
//CONTROL STRUCT FOR THE THREADS DEDICATED TO THE BEASTS
typedef struct mt_beast_manager_t
{
    sem_t input_done_blockades[TOTAL_BEAST_LIMIT];
    sem_t start_blockades[TOTAL_BEAST_LIMIT];
    pthread_t beast_threads[TOTAL_BEAST_LIMIT];
    int cur_size;
    bool start_blockade_passed[TOTAL_BEAST_LIMIT];
} mt_beast_manager_t;

typedef struct beast_move_params_t
{
    beast_tracker_t * beast_tracker;
    map_point_t * beast;
    map_point_t * map;
    sem_t * beast_done_blockade;
    sem_t * start_blockade;
} beast_move_params_t;

//COIN MANAGEMENT STUFF
void spawn_coin(entity_t coin_size, map_point_t map[]);
int coin_spawn_init(map_point_t map[]);

//BEAST STUFF
int beast_spawn_init(map_point_t map[]);
//*** INITIALIZER FUNCTION

int mt_beast_manager_init();
//*** SETTING THE SIZE TO 0(EMPTY ARRAY, NO BEASTS ON THE MAP)
//*** ALSO CALLING SEM_INIT ON EVERY SEMAPHORE IN THE CONTROL STRUCT
int mt_beast_manager_destroy();
//*** DESTROYING THE SEMAPHORES
int mt_beast_spawn(beast_move_params_t * params);
//*** THE SPAWN_BEAST_MULTITHREADED FUNCTION SHOULD CONTAIN A PTHREAD_CREATE.
//*** IT SHOULD FAIL THE CALL AND RETURN AN ERROR IF THE THREAD LIMIT / BEAST LIMIT IS REACHED.
//*** IT SHOULD ALSO CHANGE THE N VALUE.

void mt_beast_release_all();
//*** THE FUNCTION SHOULD RELEASE(POST) THE SEMAPHORES ON ALL WAITING BEASTS

point_t mt_beast_player_find_internal(map_point_t * map, int b_y, int b_x);
void mt_beast_choose_tiles_internal(point_t beast_loc, point_t player_loc, map_point_t * map,
                                    int * tile_cnt, int curr_distance, map_point_t * acceptable_tiles);
//***UTILIY FUNCTIONS FOR BEAST_MOVE



//PLAYER STUFF
void player_manager_init();
void player_send_map_dimensions_internal(int which_player);
int player_send_map_update(map_point_t map[], int which_player, point_t curr_location);
void player_send_serverside_stats(int which_player, size_t carried, size_t brought, int deaths);
void player_send_spawn_internal(map_point_t * map, int map_width, int map_length, int which_player);
void player_receive_move(map_point_t map[],int which_player, int map_width);
int player_respawn_killed_player_internal(map_point_t map[], point_t killed_player_loc);
void player_send_turn(int turn_counter, int which_player);
void player_dropped_coin_spawn(map_point_t map[],point_t drop_location, size_t carried_coins);
void player_dropped_coin_manager_init();
int player_dropped_coin_manager_find(point_t dropped_coin_location);
void player_dropped_coin_remove(map_point_t map[], point_t dropped_coin_loc);
//MISC STUFF
void handle_event(int c, map_point_t map[], beast_move_params_t * params);
void stat_window_display_server(WINDOW * window, int pid, int turn_cnt);
int erase_player_if_error(map_point_t * map, int which_player);
//THREAD STUFF
void *input_routine(void *input_storage);
void *listening_join_routine(void * param);
void *listening_disconnect_routine(void * param);

void* mt_beast_move_routine(void * params);
//*** THE FUNCTION THAT MOVES THE BEAST. SAME RULES AS THE SINGLETHREADED BEAST.
//*** BEASTS MOVE SEQUENTIALLY. EACH AND EVERY ONE SHOULD MOVE ONLY ONCE, DURING A GIVEN TURN.
//*** EXAMPLE: BEAST1 moves, then gets blocked, BEAST2 moves, then gets blocked, BEAST 3 moves, then gets blocked,
//*** All the beasts are blocked, awaiting on signal that releases them from the main thread (the release_all function)
//*** CONSISTS OF AN INFINITE WHILE LOOP AND BREAKS THAT TERMINATE THE LOOP.

coin_spawn_manager_t coinSpawnManager;
beast_spawn_manager_t beastSpawnManager;
player_manager_t playerManager;
dropped_coin_manager_t droppedCoinManager;
mt_beast_manager_t mtBeastManager;

pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_player_manag = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t beast_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t beast_done_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t beast_thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t  beast_done_cond = PTHREAD_COND_INITIALIZER;

sem_t input_found_blockade;
int beast_threads_working = 0;
bool user_did_not_quit = true;
bool listening_join_routine_not_terminated = true;
bool disconnect_routine_not_terminated = true;
bool beasts_not_terminated = true;

int main() {
    srand((unsigned int) time(NULL));
    make_fifos();


    player_manager_init();
    map_point_t map[MAP_LENGTH * MAP_WIDTH];
    map_point_t basic_map_for_validation[MAP_LENGTH * MAP_WIDTH];
    ssize_t err = map_init(map, MAP_WIDTH, MAP_LENGTH);
    if(err != 0)
        return 1;
    err = map_init(basic_map_for_validation, MAP_WIDTH, MAP_LENGTH);
    if(err != 0)
        return 1;

    beast_tracker_t beast_tracker = { .size = 0};
    coin_spawn_init(map);
    beast_spawn_init(map);
    player_dropped_coin_manager_init();
    mt_beast_manager_init();

    //required ncurses initialization functions
    ncurses_funcs_init();
    int server_pid = getpid();

    WINDOW * stats_window = newwin(11, 70, STAT_WINDOW_START_Y, STAT_WINDOW_START_X);
    WINDOW * game_window = newwin(MAP_WIDTH + 1, MAP_LENGTH + 1,0,0);

    signal(SIGPIPE, SIG_IGN);

    refresh();
    box(game_window, 0, 0);
    box(stats_window, 0, 0);

    //assigning background color and letter color to every entity
    attribute_list_init();

    //pthread, sem related stuff
    int input = NO_INPUT;
    sem_init(&input_found_blockade, 0 ,0);
    pthread_t thread1;
    pthread_create(&thread1, NULL, input_routine, &input);
    pthread_t thread2;
    pthread_create(&thread2, NULL, listening_join_routine, map);
    pthread_t thread3;
    pthread_create(&thread3, NULL, listening_disconnect_routine, NULL);

    // loop

    int turn_counter = 0;
    while(1)
    {
        beast_move_params_t params = {.beast_tracker = &beast_tracker, .map = map};
        pthread_mutex_lock(&input_mutex);
        if(input == (int) 'q')
        {
            sem_post(&input_found_blockade);
            break;
        }
        pthread_mutex_unlock(&input_mutex);

        pthread_mutex_lock(&input_mutex);
        if(input != NO_INPUT)
        {
            handle_event(input, map, &params);
            pthread_mutex_unlock(&input_mutex);
            sem_post(&input_found_blockade);
        }
        else
        {
            pthread_mutex_unlock(&input_mutex);
        }

        // RELEASING THE SEMAPHORE LOCKS IN EACH BEAST THREAD SO THAT THE BEASTS MIGHT BEGIN WORKING
        mt_beast_release_all();

        // THIS COND_WAIT BLOCKS THE MAIN THREAD UNTIL ALL THE BEASTS ARE DONE WORKING.
        pthread_mutex_lock(&beast_thread_count_mutex);
        while(beast_threads_working != 0)
        {
            pthread_cond_wait(&beast_done_cond, &beast_thread_count_mutex);
        }
        pthread_mutex_unlock(&beast_thread_count_mutex);
        //handle_players
        for(int i = 0; i < 4; i++)
        {
            if( playerManager.players[i].is_free == false)
            {// each one of these functions is thread safe

                player_send_serverside_stats(i, playerManager.players[i].carried, playerManager.players[i].brought,
                                             playerManager.players[i].deaths);

                //Erasing a player if SIGPIPE happens
                int error_erase = erase_player_if_error(map, i);
                if(error_erase == -1)
                    continue;

                player_send_map_update(map, i, playerManager.players[i].curr_location);

                error_erase = erase_player_if_error(map, i);
                if(error_erase == -1)
                    continue;

                player_receive_move(map, i, MAP_WIDTH);

                error_erase = erase_player_if_error(map, i);
                if(error_erase == -1)
                    continue;

            }
        }
        render_map(map, game_window, MAP_WIDTH, MAP_LENGTH);
        stat_window_display_server(stats_window, server_pid, turn_counter);

        wrefresh(game_window);
        wrefresh(stats_window);
        turn_counter++;
        for(int i = 0; i < 4; i++)
        {
            if( playerManager.players[i].is_free == false && playerManager.players[i].has_received_move == true &&
                playerManager.players[i].has_sent_map == true && playerManager.players[i].has_sent_stats == true)
            {// each one of these functions is thread safe
                player_send_turn(turn_counter, i);
            }
        }
        usleep(1000 * 1000);
    }
    endwin();

    listening_join_routine_not_terminated = false;
    disconnect_routine_not_terminated = false;
    err = unlink_fifos();
    if(err == -1) fprintf(stderr,"%s\n", strerror(errno));

    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread1, NULL);

    pthread_mutex_destroy(&input_mutex);
    pthread_mutex_destroy(&mutex_player_manag);
    pthread_mutex_destroy(&beast_mutex);
    pthread_mutex_destroy(&beast_done_mutex);
    pthread_cond_destroy(&beast_done_cond);
    sem_destroy(&input_found_blockade);
    mt_beast_manager_destroy();
    return 0;
}

//Keystroke handling: adding entities based on which key was pressed
void handle_event(int c, map_point_t map[], beast_move_params_t * params)
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
            mt_beast_spawn(params);
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


void *input_routine(void *input_storage) {
    while (1) {

        pthread_mutex_lock(&input_mutex);
        *(int *) input_storage = NO_INPUT;
        pthread_mutex_unlock(&input_mutex);

        char c = (char) getch();
        flushinp();
        if( c != NO_INPUT) {

            pthread_mutex_lock(&input_mutex);
            *(int *) input_storage = (int) c;
            pthread_mutex_unlock(&input_mutex);

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


void *listening_join_routine(void * param)
{
    map_point_t * map = (map_point_t *) param;
    while(listening_join_routine_not_terminated)
    {
        int fd_read = open("fifo_p_to_s_init", O_RDONLY | O_NONBLOCK);
        if(fd_read == -1)
        {
            fprintf(stderr, "Errno: %s\n", strerror(errno));
        }

        int fd_write = open("fifo_s_to_p_init", O_WRONLY | O_NONBLOCK);
        int player_pid;

        //SELECT STUFF
        //INITIALIZING THE STRUCTS NEEDED FOR SELECT
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fd_read, &read_set);
        //TIME INIT
        struct timeval time_struct;
        time_struct.tv_sec = 0;
        time_struct.tv_usec = 1050;

        //A CLIENT HAS BEEN FOUND
        if( fd_write != -1 && fd_read != -1 ) {
            ssize_t errs = 0;
            //CHECKING THE RESULT OF THE SELECT FUNCTION, USED MOSTLY IN DEBUG
            if (select(fd_read + 1, &read_set, NULL, NULL, &time_struct) == -1) {
                errs++;
            }

            //READING FROM THE CLIENT IF SELECT FOUND ANYTHING
            if(FD_ISSET(fd_read, &read_set))
                errs = read(fd_read, &player_pid, sizeof(int));

            pthread_mutex_lock(&mutex_player_manag);
            if (playerManager.cur_size < 4) {
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
                if(write(fd_write, &playerManager.players[i], sizeof(server_side_player_t)) != sizeof(server_side_player_t))
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
                //CONNECTION REQUEST REJECTED
                pthread_mutex_unlock(&mutex_player_manag);
                int rej_var = 234;
                //SENDING OUT 234 AS A SIGNAL TO PLAYER THAT HE HAS BEEN REJECTED
                if(write(fd_write, &rej_var, sizeof(int)) == sizeof(int ))
                {   //for debug purposes
                    errs++;
                }
            }

        }
        if(fd_write != -1) close(fd_write);
        if(fd_read != -1) close(fd_read);
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
                ssize_t err = kill(playerManager.players[i].process_id, 0);
                if(err == -1 && errno == ESRCH) //process doesnt exist
                {
                    playerManager.players[i].is_free = true;
                    playerManager.players[i].is_currently_in_a_bush = false;
                    playerManager.cur_size--;
                    close(playerManager.players[i].p_to_serv_fd);
                    close(playerManager.players[i].serv_to_p_fd);
                }
            }
        }
        pthread_mutex_unlock(&mutex_player_manag);
        usleep(50 * 1000);
    }
    return NULL;
}

void * mt_beast_move_routine(void * params)
{
    beast_move_params_t * temp = (beast_move_params_t *) params;

    map_point_t * map = temp->map;
    map_point_t * beast = temp->beast;
    beast_tracker_t * beast_tracker = temp->beast_tracker;
    sem_t * beast_done_blockade = temp->beast_done_blockade;
    sem_t * start_blockade = temp->start_blockade;

    sem_wait(start_blockade);

    while(true)
    {
        pthread_mutex_lock(&beast_mutex);
        if(!beasts_not_terminated)
        {
            pthread_mutex_unlock(&beast_mutex);
            break;
        }


        //stuff
        //error checking
        if( beast->point_display_entity != ENTITY_BEAST) {
            return NULL;
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
        if(!beast_found) {
            return NULL;
        }
        bool player_found = false;
        //BEAST CHECKING IF THE PLAYER IS SOMEWHERE NEAR

        int locs_cnt = 0;
        int b_x = (int) beast->point.x, b_y = (int) beast->point.y;
        int p_x = -1, p_y = -1;
        point_t plr = mt_beast_player_find_internal(map, beast->point.y, beast->point.x);
        if(plr.x != -1 && plr.y != -1)
        {
            player_found = true;
            p_x = plr.x;
            p_y = plr.y;
        }
        //BEAST FOUND PLAYER, CHASING PLAYER
        //BEAST RANDOMLY CHOOSES A TILE WHICH DECREASES ITS DISTANCE TO THE PLAYER
        if(player_found)
        {
            int tile_cnt = 0;
            map_point_t acceptable_tiles[2];
            int curr_distance = abs(b_x - p_x) + abs(b_y - p_y);
            point_t bst = { .y = b_y, .x = b_x};
            mt_beast_choose_tiles_internal(bst, plr, map, &tile_cnt, curr_distance, acceptable_tiles);
            bool can_kill_player = false;
            int plr_tile = 0;
            for(int i = 0; i < tile_cnt; i++)
            {
                if(is_player(acceptable_tiles[i]) )
                {
                    plr_tile = i;
                    can_kill_player = true;
                    break;
                }
            }

            if(can_kill_player) {

                int which_player;
                //finding player
                pthread_mutex_lock(&mutex_player_manag);

                point_t old_loc;
                bool p_found = false;
                for (int j = 0; j < playerManager.cur_size; j++) {
                    if (playerManager.players[j].curr_location.x == acceptable_tiles[plr_tile].point.x &&
                        playerManager.players[j].curr_location.y == acceptable_tiles[plr_tile].point.y) {
                        which_player = j;
                        old_loc = playerManager.players[which_player].curr_location;
                        p_found = true;
                        break;
                    }
                }

                if(p_found != true)
                    return NULL;
                //taking away all his coins
                playerManager.players[which_player].has_received_move = true;
                size_t carried_coins = playerManager.players[which_player].carried;
                playerManager.players[which_player].carried = 0;

                pthread_mutex_unlock(&mutex_player_manag);
                //respawning player
                player_respawn_killed_player_internal(map, old_loc);
                //making player coins a part of a DROPPED_COIN_ENTITY
                player_dropped_coin_spawn(map, old_loc, carried_coins);

                //decrementing the counter
                pthread_mutex_lock(&beast_thread_count_mutex);
                beast_threads_working = beast_threads_working - 1;
                pthread_mutex_unlock(&beast_thread_count_mutex);

                pthread_mutex_unlock(&beast_mutex);
                //signalling main that 1 thread has finished working
                pthread_cond_signal(&beast_done_cond);
                sem_wait(beast_done_blockade);
                continue;
            }
            //just moving to one of the tiles
            if( tile_cnt != 0) {
                int which_tile = rand() % tile_cnt;

                beast->point_display_entity = ENTITY_FREE;

                int tile_x = acceptable_tiles[which_tile].point.x;
                int tile_y = acceptable_tiles[which_tile].point.y;
                beast_tracker->beasts[beast_tracker_index] = &map[tile_y * MAP_WIDTH + tile_x];
                beast_tracker->beasts[beast_tracker_index]->point_display_entity = ENTITY_BEAST;
            }
            beast = beast_tracker->beasts[beast_tracker_index];

            pthread_mutex_lock(&beast_thread_count_mutex);
            beast_threads_working = beast_threads_working - 1;
            pthread_mutex_unlock(&beast_thread_count_mutex);
            pthread_mutex_unlock(&beast_mutex);

            pthread_cond_signal(&beast_done_cond);
            sem_wait(beast_done_blockade);
            continue;
        }

        //RANDOM MOVEMENT FOR THE BEAST IF IT DIDNT FIND THE PLAYER
        map_point_t * possible_locs[4];
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
        if(locs_cnt != 0) {
            int which_loc = rand() % locs_cnt;
            possible_locs[which_loc]->point_display_entity = ENTITY_BEAST;
            beast->point_display_entity = ENTITY_FREE;

            beast_tracker->beasts[beast_tracker_index] = possible_locs[which_loc];
            beast = beast_tracker->beasts[beast_tracker_index];
        }
        pthread_mutex_lock(&beast_thread_count_mutex);
        beast_threads_working = beast_threads_working - 1;
        pthread_mutex_unlock(&beast_thread_count_mutex);

        pthread_mutex_unlock(&beast_mutex);
        pthread_cond_signal(&beast_done_cond);
        sem_wait(beast_done_blockade);
    }

    return NULL;
}

int mt_beast_spawn(beast_move_params_t * params)
{

    if( params->beast_tracker->size < 0 || params->beast_tracker->size == TOTAL_BEAST_LIMIT || beastSpawnManager.free_spawners == 0)
    {
        return -1;
    }
    int which_beast_spawner = rand() % beastSpawnManager.free_spawners;
    beastSpawnManager.spawner_array[which_beast_spawner]->point_display_entity = ENTITY_BEAST;
    int loc = beastSpawnManager.free_spawners - 1;
    swap(beastSpawnManager.spawner_array[which_beast_spawner], beastSpawnManager.spawner_array[loc]);
    beastSpawnManager.free_spawners--;

    params->beast_tracker->beasts[params->beast_tracker->size] = beastSpawnManager.spawner_array[loc];
    params->beast_tracker->size = params->beast_tracker->size + 1;

    params->beast = params->beast_tracker->beasts[params->beast_tracker->size - 1];
    params->start_blockade = &mtBeastManager.start_blockades[mtBeastManager.cur_size];
    params->beast_done_blockade = &mtBeastManager.input_done_blockades[mtBeastManager.cur_size];

    pthread_mutex_lock(&beast_thread_count_mutex);
    beast_threads_working = mtBeastManager.cur_size + 1;
    pthread_mutex_unlock(&beast_thread_count_mutex);

    int size = mtBeastManager.cur_size;
    pthread_create(&mtBeastManager.beast_threads[size], NULL, mt_beast_move_routine, params);
    mtBeastManager.cur_size = mtBeastManager.cur_size + 1;
    return 0;
}

void mt_beast_release_all()
{
    //problems: ???

    pthread_mutex_lock(&beast_thread_count_mutex);
    beast_threads_working = mtBeastManager.cur_size;
    pthread_mutex_unlock(&beast_thread_count_mutex);
    for(int i = 0; i < mtBeastManager.cur_size; i++)
    {
        if(mtBeastManager.start_blockade_passed[i] == true)
            sem_post( &mtBeastManager.input_done_blockades[i]);
    }
    for(int i = 0; i < mtBeastManager.cur_size; i++)
    {
        if(mtBeastManager.start_blockade_passed[i] == false)
        {
            sem_post(&mtBeastManager.start_blockades[i]);
            mtBeastManager.start_blockade_passed[i] = true;
        }
    }
}
int mt_beast_manager_init()
{
    for(int i = 0; i < TOTAL_BEAST_LIMIT; i++)
    {
        sem_init(&mtBeastManager.input_done_blockades[i], 0, 0);
        sem_init(&mtBeastManager.start_blockades[i], 0, 0);
        mtBeastManager.start_blockade_passed[i] = false;
    }
    mtBeastManager.cur_size = 0;

    return 0;
}

int mt_beast_manager_destroy()
{
    for(int i = 0; i < TOTAL_BEAST_LIMIT; i++)
    {
        sem_destroy(&mtBeastManager.input_done_blockades[i]);
        sem_destroy(&mtBeastManager.start_blockades[i]);
        mtBeastManager.start_blockade_passed[i] = false;
    }
    mtBeastManager.cur_size = 0;

    return 0;
}
void player_manager_init()
{
    for(int i = 0; i < 4; i++)
    {
        playerManager.players[i].is_free = true;
        playerManager.players[i].player_num = i;
        playerManager.players[i].is_currently_in_a_bush = false;
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
point_t mt_beast_player_find_internal(map_point_t * map, int b_y, int b_x)
{
    point_t return_val;
    int p_x = -1, p_y = -1;
    bool player_found = false;
    for(int i = -2; i <= 2; i++)
    {
        if ((int) b_y + i >= 0 && (int) b_y + i < MAP_WIDTH) {
            for (int j = -2; j <= 2; j++) {
                if ((int) b_x + j >= 0 && (int) b_x + j < MAP_LENGTH)
                {
                    entity_t entity_type_of_tile = map[(b_y + i) * MAP_WIDTH + b_x + j].point_display_entity;
                    if(entity_type_of_tile >= ENTITY_PLAYER_1 &&
                       entity_type_of_tile <= ENTITY_PLAYER_4)
                    {
                        player_found = true;
                        p_x = b_x + j;
                        p_y = b_y + i;
                        break;
                    }
                }
            }
        }
        if(player_found == true)
        {
            break;
        }
    }
    return_val.x = p_x;
    return_val.y = p_y;
    return return_val;
}

void mt_beast_choose_tiles_internal(point_t beast_loc, point_t player_loc, map_point_t * map,
                                    int * tile_cnt, int curr_distance, map_point_t * acceptable_tiles)
{
    int b_y = beast_loc.y, b_x = beast_loc.x;
    int p_y = player_loc.y, p_x = player_loc.x;
    for(int i = -1; i <= 1; i++)
    {
        if ((int) b_y + i >= 0 && (int) b_y + i < MAP_WIDTH) {
            for (int j = -1; j <= 1; j++) {
                if ((int) b_x + j >= 0 && (int) b_x + j < MAP_LENGTH)
                {
                    if( (i == -1 && j == 0) || (i == 1 && j == 0) || (i == 0 && j == -1) || (i == 0 && j == 1)  )
                    {
                        if (abs(b_x + j - p_x) + abs(b_y + i - p_y) < curr_distance &&
                            (  map[(b_y + i) * MAP_WIDTH + b_x + j].point_display_entity == ENTITY_FREE
                               || is_player(map[(b_y + i) * MAP_WIDTH + b_x + j]) ) )
                        {
                            acceptable_tiles[*tile_cnt] = map[(b_y + i) * MAP_WIDTH + b_x + j];
                            *tile_cnt = *tile_cnt + 1;
                        }
                    }
                }
            }
        }
    }
}
void player_send_map_dimensions_internal(int which_player)
{
    // as this function is internal, they dont need mutexes, to lock shared resources.
    // the calling function does need them, however.
    int buf = MAP_WIDTH;
    ssize_t err;
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
        if ((int) curr_location.y + i >= 0 && (int) curr_location.y + i < MAP_WIDTH) {
            for (int j = -2; j < 3; j++) {
                if ((int) curr_location.x + j >= 0 && (int) curr_location.x + j < MAP_LENGTH)
                {
                    map_fragment[array_cnt] = map[(curr_location.y + i )* MAP_WIDTH + curr_location.x + j];
                    array_cnt++;
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
    ssize_t err;
    err = write(playerManager.players[which_player].serv_to_p_fd, map_fragment, sizeof(map_point_t) * map_fragment_size);
    if( err != sizeof(map_point_t) * map_fragment_size)
    {
        playerManager.players[which_player].is_free = true;
        pthread_mutex_unlock(&mutex_player_manag);
        return 0;
    }
    pthread_mutex_unlock(&mutex_player_manag);

    playerManager.players[which_player].has_sent_map = true;
    return 0;
}

void player_send_serverside_stats(int which_player, size_t carried, size_t brought, int deaths)
{
    pthread_mutex_lock( &mutex_player_manag);
    ssize_t err = write(playerManager.players[which_player].serv_to_p_fd, &brought, sizeof(int));
    if(err != sizeof(int))
    {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = write(playerManager.players[which_player].serv_to_p_fd, &carried, sizeof(int) );
    if(err != sizeof(int))
    {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = write(playerManager.players[which_player].serv_to_p_fd, &deaths, sizeof(int) );
    if(err != sizeof(int))
    {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
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
    bool is_cpu;
    pthread_mutex_lock(&mutex_player_manag);
    ssize_t err = read(playerManager.players[which_player].p_to_serv_fd, &old_loc, sizeof(point_t));
    if ( err != sizeof(point_t)) {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = read(playerManager.players[which_player].p_to_serv_fd, &new_loc, sizeof(point_t));
    if ( err != sizeof(point_t)) {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    err = read(playerManager.players[which_player].p_to_serv_fd, &is_cpu, sizeof(bool));
    if ( err != sizeof(bool)) {
        playerManager.players[which_player].is_free = true;
        playerManager.players[which_player].is_currently_in_a_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }

    pthread_mutex_unlock(&mutex_player_manag);
    if(new_loc.y > MAP_WIDTH || new_loc.x > MAP_LENGTH){
        return;
    }
    int total_distance_x = abs(new_loc.x - old_loc.x);
    int total_distance_y = abs(new_loc.y - old_loc.y);
    int total_distance = total_distance_y + total_distance_x;
    if(total_distance > 2)
    {
        pthread_mutex_lock(&mutex_player_manag);
        playerManager.players[which_player].is_cpu = is_cpu;
        playerManager.players[which_player].has_received_move = true;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    if( map[new_loc.y * MAP_WIDTH + new_loc.x].point_display_entity == ENTITY_WALL) {
        pthread_mutex_lock(&mutex_player_manag);
        playerManager.players[which_player].is_cpu = is_cpu;
        playerManager.players[which_player].has_received_move = true;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }


    pthread_mutex_lock(&mutex_player_manag);
    //BUSH HANDLING
    //IF A PLAYER CANT ESCAPE THE BUSH, FUNCTION SETS A FLAG SO HE CAN ESCAPE NEXT TURN
    playerManager.players[which_player].is_cpu = is_cpu;
    if( playerManager.players[which_player].is_currently_in_a_bush == true
     && playerManager.players[which_player].can_escape_bush == false)
    {
        playerManager.players[which_player].can_escape_bush = true;
        playerManager.players[which_player].has_received_move = true;
        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    pthread_mutex_unlock(&mutex_player_manag);

    //PLAYER COLLISION AND PLACING ONE OF THE PLAYERS INTO THE SPAWN, SPAWNS A DROPPED COIN ENTITY

    if( map[new_loc.y * map_width + new_loc.x].point_display_entity >= ENTITY_PLAYER_1 &&
        map[new_loc.y * map_width + new_loc.x].point_display_entity <= ENTITY_PLAYER_4 && total_distance > 0)
    {
        int killed_player_index = player_respawn_killed_player_internal(map, new_loc);
        pthread_mutex_lock(&mutex_player_manag);

        size_t monies = playerManager.players[killed_player_index].carried;
        playerManager.players[killed_player_index].carried = 0;
        playerManager.players[which_player].has_received_move = true;
        pthread_mutex_unlock(&mutex_player_manag);

        player_dropped_coin_spawn(map, new_loc, monies);
        return;
    }
    //BEAST COLLISION AND PLACING THE PLAYER INTO THE SPAWN, SPAWNS A DROPPED COIN ENTITY
    if( map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_BEAST)
    {
        pthread_mutex_lock(&mutex_player_manag);
        playerManager.players[which_player].has_received_move = true;
        pthread_mutex_unlock(&mutex_player_manag);
        player_respawn_killed_player_internal(map,old_loc);
        size_t carried_coins = playerManager.players[which_player].carried;
        playerManager.players[which_player].carried = 0;
        player_dropped_coin_spawn(map, old_loc, carried_coins);
        return;
    }

    //IF THE PLAYER WANTS TO GO INTO THE BUSH THE FUNCTION LETS HIM DO THAT, BUT SETS A FLAG

    if( map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_BUSH ||
        map[new_loc.y * map_width + new_loc.x].point_terrain_entity == ENTITY_BUSH)
    {
        pthread_mutex_lock(&mutex_player_manag);
        playerManager.players[which_player].is_currently_in_a_bush = true;
        playerManager.players[which_player].can_escape_bush = false;
        pthread_mutex_unlock(&mutex_player_manag);
    }

    //CAMPSITE HANDLING, WHEN PLAYER MOVES INTO A CAMPSITE ALL ITS CARRIED MONEY BECOMES BROUGHT MONEY
    if( map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_CAMPSITE)
    {
        pthread_mutex_lock(&mutex_player_manag);

        playerManager.players[which_player].brought += playerManager.players[which_player].carried;
        playerManager.players[which_player].carried = 0;
        playerManager.players[which_player].has_received_move = true;

        pthread_mutex_unlock(&mutex_player_manag);
        return;
    }
    //COIN HANDLING, COIN DISAPPEARS, BECOMES CARRIED MONEY
    if(is_coin(map[new_loc.y * map_width + new_loc.x]))
    {
        if(map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_COIN_SMALL)
        {
            pthread_mutex_lock(&mutex_player_manag);
            playerManager.players[which_player].carried += 10;
            pthread_mutex_unlock(&mutex_player_manag);
        }
        else if(map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_COIN_BIG)
        {
            pthread_mutex_lock(&mutex_player_manag);
            playerManager.players[which_player].carried += 50;
            pthread_mutex_unlock(&mutex_player_manag);
        }
        else if(map[new_loc.y * map_width + new_loc.x].point_display_entity == ENTITY_COIN_TREASURE)
        {
            pthread_mutex_lock(&mutex_player_manag);
            playerManager.players[which_player].carried += 200;
            pthread_mutex_unlock(&mutex_player_manag);
        }
        else
        {
            pthread_mutex_lock(&mutex_player_manag);
            int old_x = playerManager.players[which_player].curr_location.x;
            int old_y = playerManager.players[which_player].curr_location.y;

            map[old_y * map_width + old_x].point_display_entity = map[old_y * map_width + old_x].point_terrain_entity;

            int d_index = player_dropped_coin_manager_find(new_loc);
            playerManager.players[which_player].carried += droppedCoinManager.coins[d_index].val;
            player_dropped_coin_remove(map, new_loc);

            entity_t plr = attribute_list[which_player + 5].entity;
            map[new_loc.y * map_width + new_loc.x].point_display_entity = plr;

            playerManager.players[which_player].curr_location = new_loc;
            playerManager.players[which_player].has_received_move = true;
            pthread_mutex_unlock(&mutex_player_manag);
            return;
        }
    }
    //PLAYER JUST WANTING TO MOVE TO AN EMPTY TILE
    pthread_mutex_lock(&mutex_player_manag);
    int old_x = playerManager.players[which_player].curr_location.x;
    int old_y = playerManager.players[which_player].curr_location.y;

    map[old_y * map_width + old_x].point_display_entity = map[old_y * map_width + old_x].point_terrain_entity;
    playerManager.players[which_player].curr_location = new_loc;
    entity_t player_num = attribute_list[which_player + 5].entity;

    map[new_loc.y * map_width + new_loc.x].point_display_entity = player_num;

    playerManager.players[which_player].has_received_move = true;
    pthread_mutex_unlock(&mutex_player_manag);
}
int player_respawn_killed_player_internal(map_point_t map[], point_t killed_player_loc)
{
    int killed_plr_indx = 0;
    pthread_mutex_lock(&mutex_player_manag);
    for(int i = 0; i < playerManager.cur_size; i++)
    {
        if( playerManager.players[i].curr_location.x == killed_player_loc.x
            && playerManager.players[i].curr_location.y == killed_player_loc.y)
        {
            killed_plr_indx = i;
            break;
        }
    }
    playerManager.players[killed_plr_indx].deaths = playerManager.players[killed_plr_indx].deaths + 1;
    int old_x = killed_player_loc.x;
    int old_y = killed_player_loc.y;
    map[old_y * MAP_WIDTH + old_x].point_display_entity = map[old_y * MAP_WIDTH + old_x].point_terrain_entity;
    // if the spawn is occupied, we will move right and up to check if there is a free space
    int spawn_x = playerManager.players[killed_plr_indx].spawn_location.x;
    int spawn_y = playerManager.players[killed_plr_indx].spawn_location.y;
    if( map[spawn_y * MAP_WIDTH + spawn_x].point_display_entity == ENTITY_FREE)
    {
        playerManager.players[killed_plr_indx].curr_location.x = playerManager.players[killed_plr_indx].spawn_location.x;
        playerManager.players[killed_plr_indx].curr_location.y = playerManager.players[killed_plr_indx].spawn_location.y;
    }
    else
    {
        for(int i = 0; i < 4; i++)
        {
            for(int j = 0; j < 4; j++)
            {
                if(map[(spawn_y + i )* MAP_WIDTH + spawn_x + j].point_display_entity == ENTITY_FREE)
                {
                    playerManager.players[killed_plr_indx].curr_location.x = playerManager.players[killed_plr_indx].spawn_location.x + j;
                    playerManager.players[killed_plr_indx].curr_location.y = playerManager.players[killed_plr_indx].spawn_location.y + i;
                    break;
                }
            }
        }
    }
    entity_t plr = attribute_list[killed_plr_indx + 5].entity;
    int new_x = playerManager.players[killed_plr_indx].curr_location.x;
    int new_y = playerManager.players[killed_plr_indx].curr_location.y;
    map[new_y * MAP_WIDTH + new_x].point_display_entity = plr;

    pthread_mutex_unlock(&mutex_player_manag);
    return killed_plr_indx;
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
    map_point_t temp = { .point.x = spawn_loc_x, .point.y = spawn_loc_y,
                         .point_display_entity = plr, .point_terrain_entity = ENTITY_FREE, .spawnerType = NO_SPAWNER};
    ssize_t err = write(playerManager.players[which_player].serv_to_p_fd, &temp, sizeof(map_point_t));
    if(err != sizeof(temp))
    {
        return;
    }
    playerManager.players[which_player].curr_location.y = spawn_loc_y;
    playerManager.players[which_player].curr_location.x = spawn_loc_x;
    playerManager.players[which_player].spawn_location.y = spawn_loc_y;
    playerManager.players[which_player].spawn_location.x = spawn_loc_x;
    map[spawn_loc_y * map_width + spawn_loc_x].point_display_entity = temp.point_display_entity;
}

void player_send_turn(int turn_counter, int which_player)
{
    int buf = turn_counter;
    pthread_mutex_lock(&mutex_player_manag);
    ssize_t err;
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

void player_dropped_coin_spawn(map_point_t map[],point_t drop_location, size_t carried_coins)
{
    int cnt = droppedCoinManager.size;
    if(cnt >= 10)
    {
        return;
    }

    droppedCoinManager.coins[cnt].dropped_coin_loc = drop_location;
    droppedCoinManager.coins[cnt].val = carried_coins;

    map[drop_location.y * MAP_WIDTH + drop_location.x].point_display_entity = ENTITY_COIN_DROPPED;
    droppedCoinManager.size++;
}
void player_dropped_coin_manager_init()
{
    droppedCoinManager.size = 0;
}
int player_dropped_coin_manager_find(point_t dropped_coin_location)
{
    int i;
    for(i = 0; i < droppedCoinManager.size; i++)
    {
        if( droppedCoinManager.coins[i].dropped_coin_loc.y ==  dropped_coin_location.y &&
            droppedCoinManager.coins[i].dropped_coin_loc.x ==  dropped_coin_location.x)
        {
            return i;
        }
    }
    return -1;
}
void player_dropped_coin_remove(map_point_t map[], point_t dropped_coin_loc)
{
    int index = player_dropped_coin_manager_find(dropped_coin_loc);
    int size = droppedCoinManager.size;

    swap(droppedCoinManager.coins[index], droppedCoinManager.coins[size - 1]);

    droppedCoinManager.size--;
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

void stat_window_display_server(WINDOW * window, int pid, int turn_cnt)
{
    mvwprintw(window, 1, 1, "Current turn: %d", turn_cnt);
    mvwprintw(window, 2, 1, "Server process ID: %d", pid);
    mvwprintw(window, 3, 1, "Player num:");
    mvwprintw(window, 4, 1, "Player PID:");
    mvwprintw(window, 5, 1, "Player type:");
    mvwprintw(window, 6, 1, "Player X/Y:");
    mvwprintw(window, 7, 1, "Player Deaths:");
    mvwprintw(window, 8, 1, "Player carried coins:");
    mvwprintw(window, 9, 1, "Player brought coins:");
    for(int i = 0; i < 4; i++)
    {
        if(playerManager.players[i].is_free == true)
        {
            mvwprintw(window, 3, 1 + 14 + 12 * (i + 1), "x",i);
            mvwprintw(window, 4, 1 + 10+ 12 * (i + 1), "xxxxx");
            mvwprintw(window, 5, 1 + 12+ 12 * (i + 1), "xxx");
            mvwprintw(window, 6, 1 + 10+ 12 * (i + 1), "xx/xx");
            mvwprintw(window, 7, 1 + 12+ 12 * (i + 1), "xxx");
            mvwprintw(window, 8, 1 + 8 + 12 * (i + 1), "xxxxxxx");
            mvwprintw(window, 9, 1 + 8 + 12 * (i + 1), "xxxxxxx");
        }
        else
        {
            pthread_mutex_lock(&mutex_player_manag);
            mvwprintw(window, 3, 1 + 14 + 12 * (i + 1), "%d",i + 1);
            mvwprintw(window, 4, 1 + 10 + 12 * (i + 1), "%5d", playerManager.players[i].process_id);
            if(playerManager.players[i].is_cpu)
                mvwprintw(window, 5,1 + 12 + 12 * (i + 1), "CPU");
            else
                mvwprintw(window, 5,1 + 12 + 12 * (i + 1), "HUM");
            int x = playerManager.players[i].curr_location.x;
            int y = playerManager.players[i].curr_location.y;
            mvwprintw(window, 6, 1 + 10 + 12 * (i + 1), "%02d/%02d", x, y);
            mvwprintw(window, 7, 1 + 12 + 12 * (i + 1), "%3d", playerManager.players[i].deaths);
            mvwprintw(window, 8, 1 + 8 + 12 * (i + 1), "%7d", playerManager.players[i].carried);
            mvwprintw(window, 9, 1 + 8 + 12 * (i + 1), "%7d", playerManager.players[i].brought);
            pthread_mutex_unlock(&mutex_player_manag);
        }
    }
}

int erase_player_if_error(map_point_t * map, int which_player)
{
    pthread_mutex_lock(&mutex_player_manag);
    if(playerManager.players[which_player].is_free == true) {
        int p_x = playerManager.players[which_player].curr_location.x;
        int p_y = playerManager.players[which_player].curr_location.y;
        map[ p_y * MAP_WIDTH + p_x].point_display_entity = map[ p_y * MAP_WIDTH + p_x].point_terrain_entity;
        close(playerManager.players[which_player].p_to_serv_fd);
        close(playerManager.players[which_player].serv_to_p_fd);
        playerManager.cur_size--;
        pthread_mutex_unlock(&mutex_player_manag);
        return -1;
    }
    else
    {
        pthread_mutex_unlock(&mutex_player_manag);
        return 0;
    }
}
