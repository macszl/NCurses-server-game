#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdbool.h>
#include <ncurses.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>


#include "map.h"

//TODO: CELE PROJEKTOWE:

// serwer powinnien odpowiadać za:
//0. inicjalizacje, załadowanie(bądź generowanie) planszy
//1. do serwera powinna docierac kazda operacja na mapie poprzez interakcje międzyprocesowe(przesylanie informacji poprzez FIFO)
//2. spawnowanie nowych agentów, istot, rzeczy na mapie
//3. zdefiniowanie autonomicznego zachowania bestii, każda bestia na nowym wątku
//4. wywoływanie procesów graczy

#define COIN_SPAWNER_NUM 24
#define BEAST_SPAWNER_NUM 20
#define DEBUG 0
#define NO_INPUT 1
#define MULTITHREADED_INPUT 0
#define TOTAL_BEAST_LIMIT 8
#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

//TODO these 2 parameters will be transferred to the players for error checking

//2 parameters defined in the map.h header file
extern const int MAP_LENGTH;
extern const int MAP_WIDTH;
extern struct entity_ncurses_attributes_t attribute_list[];

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

void spawn_coin(entity_t coin_size, map_point_t map[]);
int coin_spawn_init(map_point_t map[]);

int spawn_beast(beast_tracker_t * beast_tracker);
int beast_spawn_init(map_point_t map[]);
int beast_move(map_point_t map[], beast_tracker_t * beast_tracker, map_point_t * beast);
int handle_beasts(map_point_t map[], beast_tracker_t * beast_tracker);

void *input_routine(void *input_storage);
void handle_event(int c, map_point_t map[], beast_tracker_t * beast_tracker);

coin_spawn_manager_t coinSpawnManager;
beast_spawn_manager_t beastSpawnManager;

#if MULTITHREADED_INPUT
pthread_mutex_t mutex_input = PTHREAD_MUTEX_INITIALIZER;
sem_t input_found_blockade;
#endif

bool user_did_not_quit = true;

int main() {
    srand(time(NULL));
    map_point_t map[MAP_LENGTH * MAP_WIDTH];
    //TODO pre-game intialization menus

    int err = map_init(map);
    if(err != 0)
        return 1;

    beast_tracker_t beast_tracker = { .size = 0};
    coin_spawn_init(map);
    beast_spawn_init(map);

    //required ncurses initialization functions
    ncurses_funcs_init();

    WINDOW * window = newwin(MAP_WIDTH + 1, MAP_LENGTH + 1,0,0);
    refresh();
    box(window, 0, 0);
    wrefresh(window);

    //assigning background color and letter color to every entity
    attribute_list_init();

    //pthread, sem related stuff
#if MULTITHREADED_INPUT
    int input = NO_INPUT;
    sem_init(&input_found_blockade, 0 ,0);
    pthread_t thread1;
    pthread_create(&thread1, NULL, input_routine, &input);
#endif
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
        if(input != NO_INPUT)
        {
            handle_event(input, map);
            pthread_mutex_unlock(&mutex_input);
            sem_post(&input_found_blockade);
            sleep(1);
            render_map(map, window);
            wrefresh(window);
            continue;
        }

        pthread_mutex_unlock(&mutex_input);
        turn_counter++;
    }

    pthread_join(thread1, NULL);
#endif
    //singlethreaded mode: made for the ease of debugging
    int i = 0;
    while (user_did_not_quit)
    {
        render_map(map, window);
        wrefresh(window);
        int c = getch();
        handle_event(c, map, &beast_tracker);
        handle_beasts(map, &beast_tracker);
        i++;
    }

    endwin();
#if MULTITHREADED_INPUT
    pthread_mutex_destroy(&mutex_input);
    sem_destroy(&input_found_blockade);
#endif
    return 0;
}

//Keystroke handling: adding entities based on which key was pressed
void handle_event(int c, map_point_t map[], beast_tracker_t * beast_tracker)
{
    switch (c) {
        //arrow key handling: works only with unix distros as unix represents arrow keys as 3 characters
        case KEY_RIGHT:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_UP:
        {
            getch();
            getch();
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
        char c = (char) getch();
        flushinp();
        if( c != NO_INPUT) {
            *(int *) input_storage = (int) c;
            pthread_mutex_unlock(&mutex_input);
            sem_wait(&input_found_blockade);
            if ( c ==  'q' ) {
                break;
            }else {
                continue;
            }
        }
        pthread_mutex_unlock(&mutex_input);
    }
    return NULL;
}
#endif
// Function that initializes the beastManager variable
int beast_spawn_init(map_point_t map[])
{
    unsigned long total_spawners = 0;
    unsigned long free_spawners = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == BEAST_SPAWNER)
        {
            total_spawners++;
            free_spawners++;
        }
    }

    beastSpawnManager.free_spawners = (int) free_spawners;

    int arr_cnt = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == BEAST_SPAWNER)
        {
            beastSpawnManager.spawner_array[arr_cnt] = &map[i];
            arr_cnt++;
        }
    }
    return 0;
}
int spawn_beast(beast_tracker_t * beast_tracker)
{
    if(beast_tracker->size < 0 ||beast_tracker->size == TOTAL_BEAST_LIMIT || beastSpawnManager.free_spawners == 0)
    {
        return -1;
    }
    int which_beast_spawner = rand() % beastSpawnManager.free_spawners;
    if(beast_tracker->size < 0) {
        return -1;
    }
    beastSpawnManager.spawner_array[which_beast_spawner]->entity_type = ENTITY_BEAST;
    if(beast_tracker->size < 0) {
        return -1;
    }
    int loc = beastSpawnManager.free_spawners - 1;
    if(beast_tracker->size < 0) {
        return -1;
    }
    swap(beastSpawnManager.spawner_array[which_beast_spawner], beastSpawnManager.spawner_array[loc]);
    if(beast_tracker->size < 0) {
        return -1;
    }
    beastSpawnManager.free_spawners--;
    if(beast_tracker->size < 0) {
        return -1;
    }

    beast_tracker->beasts[beast_tracker->size] = beastSpawnManager.spawner_array[loc];
    if(beast_tracker->size < 0) {
        return -1;
    }
    beast_tracker->size = beast_tracker->size + 1;
    if(beast_tracker->size < 0) {
        return -1;
    }
    return 0;
}
int beast_move( map_point_t map[],  beast_tracker_t * beast_tracker, map_point_t * beast)
{
    //TODO Player collision, beast_spawner_occupation
    //error checking
    if( beast->entity_type != ENTITY_BEAST || beast_tracker->size < 0) {
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
                if (map[(b_y + i) * MAP_WIDTH + b_x + j].entity_type == ENTITY_FREE)
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
    possible_locs[which_loc]->entity_type = ENTITY_BEAST;
    beast->entity_type = ENTITY_FREE;

    beast_tracker->beasts[beast_tracker_index] = possible_locs[which_loc];
    return 0;
}

int handle_beasts(map_point_t map[], beast_tracker_t * beast_tracker)
{
    if(beast_tracker->size < 0) {
        return -1;
    }
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
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
        {
            free_spawners++;
        }
    }

    coinSpawnManager.free_spawners = (int) free_spawners;

    int arr_cnt = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
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
    map[y * MAP_WIDTH + x].entity_type = coin_size;

    int free_spwn_cnt = coinSpawnManager.free_spawners;
    swap(coinSpawnManager.spawner_array[which_spawner_to_use], coinSpawnManager.spawner_array[free_spwn_cnt - 1]);
    coinSpawnManager.free_spawners--;

}
