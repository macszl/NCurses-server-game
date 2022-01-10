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


#define DEBUG 0
#define NO_INPUT 1
#define MULTITHREADED_INPUT 0
#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
     memcpy(swap_temp,&y,sizeof(x)); \
     memcpy(&y,&x,       sizeof(x)); \
     memcpy(&x,swap_temp,sizeof(x)); \
    } while(0)

//TODO these 2 parameters will be transferred to the players for error checking

//2 parameters defined in the map.h header file
extern int MAP_LENGTH;
extern int MAP_WIDTH;
extern struct entity_ncurses_attributes_t attribute_list[];

typedef struct beast_t
{
    point_t curr_place;
} beast_t;

typedef struct coin_spawn_manager_t
{
    map_point_t * spawner_array;
    int free_spawners;
    int total_spawners;
} coin_spawn_manager_t;

//database storing the ncurses colors for each entity type


void spawn_beast();
void spawn_coin(entity_t coin_size, map_point_t map[]);
void beast_move(beast_t * beast_ptr, beast_t new_beast_loc);
int coin_spawn_init(map_point_t map[]);

int coin_spawn_occ(map_point_t occupied_point);
int coin_spawn_free(map_point_t freed_point);
int coin_spawn_search_for_element_internal(map_point_t element);


void *input_routine(void *input_storage);
void handle_event(int c, map_point_t map[]);

coin_spawn_manager_t coinSpawnManager;

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

    err = coin_spawn_init(map);
    if(err != 0)
        return 1;

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
        handle_event(c, map);
        i++;
    }

    free(coinSpawnManager.spawner_array);
    endwin();
#if MULTITHREADED_INPUT
    pthread_mutex_destroy(&mutex_input);
    sem_destroy(&input_found_blockade);
#endif
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
            getch();
            getch();
        }
        case 'B':
        case 'b':
        {
            add_new_entity(ENTITY_BEAST, map);
            break;
        }
        case 'c':
        {
            add_new_entity(ENTITY_COIN_SMALL, map);
            break;
        }
        case 'C':
        {
            add_new_entity(ENTITY_COIN_BIG, map);
            break;
        }
        case 'T':
        case 't':
        {
            add_new_entity(ENTITY_COIN_TREASURE, map);
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
// Function that initializes the coinManager variable which allows oversight over all the coin spawns over the map
int coin_spawn_init(map_point_t map[])
{
    unsigned long total_spawners = 0;
    unsigned long free_spawners = 0;
    coinSpawnManager.spawner_array = NULL;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
        {
            total_spawners++;
            free_spawners++;
        }
    }

    coinSpawnManager.free_spawners = (int) free_spawners;
    coinSpawnManager.total_spawners = (int) total_spawners;
    if( ( coinSpawnManager.spawner_array = malloc(sizeof(map_point_t) * total_spawners)) == NULL)
    {
        return -1;
    }

    int arr_cnt = 0;
    for(int i = 0; i < MAP_WIDTH * MAP_LENGTH; i++)
    {
        if( map[i].entity_type == ENTITY_FREE && map[i].spawnerType == COIN_SPAWNER)
        {
            memcpy( &coinSpawnManager.spawner_array[arr_cnt], &map[i], sizeof(map_point_t));
            arr_cnt++;
        }
    }

    return 0;
}

//TODO Refactor this kludge into something prettier

//returns the position of a map_point_t element, returns -1 if not found
int coin_spawn_search_for_element_internal(map_point_t element)
{
    map_point_t * arr = coinSpawnManager.spawner_array;
    int i = 0; bool element_found = false;
    for(;i < coinSpawnManager.total_spawners; i++)
    {
        if( element.point.x == arr[i].point.x  && element.point.y == arr[i].point.y)
        {
            element_found = true;
            break;
        }
    }
    if( !element_found)
        return -1;
    else
        return i;
}
int coin_spawn_occ(map_point_t occupied_point)
{
    if(coinSpawnManager.free_spawners == 0)
        return -1;

    //znajdowanie elementu
    int pos = coin_spawn_search_for_element_internal(occupied_point);
    if(pos == -1)
        return -1;

    //swap znalezionego elemetu z ostatnim wolnym elementem
    int last_free_pos = coinSpawnManager.free_spawners - 1;
#if DEBUG
    map_point_t test1 = coinSpawnManager.spawner_array[last_free_pos];
    map_point_t test2 = coinSpawnManager.spawner_array[pos];
    if(test1.point.y > 32 || test1.point.y < 0 || test1.point.x > 32 || test1.point.x < 0 ||
            test2.point.y > 32 || test2.point.y < 0 || test2.point.x > 32 || test2.point.x < 0)
    {
        printf( " a");
    }
#endif
    swap(coinSpawnManager.spawner_array[last_free_pos], coinSpawnManager.spawner_array[pos] );
    coinSpawnManager.free_spawners--;
    return 0;
}
int coin_spawn_free(map_point_t freed_point)
{
    if(coinSpawnManager.free_spawners == coinSpawnManager.total_spawners)
        return -1;

    //znajdowanie elementu
    int pos = coin_spawn_search_for_element_internal(freed_point);
    if(pos == -1)
        return -1;

    //swap znalezionego elemetu z ostatnim zajetym elementem
    int last_occ_pos = coinSpawnManager.free_spawners;
#if DEBUG
    map_point_t test1 = coinSpawnManager.spawner_array[last_occ_pos];
    map_point_t test2 = coinSpawnManager.spawner_array[pos];
    if(test1.point.y > 32 || test1.point.y < 0 || test1.point.x > 32 || test1.point.x < 0 ||
       test2.point.y > 32 || test2.point.y < 0 || test2.point.x > 32 || test2.point.x < 0)
    {
        printf( " a");
    }
#endif
    swap(coinSpawnManager.spawner_array[last_occ_pos], coinSpawnManager.spawner_array[pos]);
    coinSpawnManager.free_spawners++;
    return 0;
}
void spawn_coin(entity_t coin_size, map_point_t map[])
{
    if( coin_size < ENTITY_COIN_SMALL || coinSpawnManager.free_spawners == 0)
    {
        return;
    }

    int which_spawner_to_use = rand() % coinSpawnManager.free_spawners;
    int y = (int )coinSpawnManager.spawner_array[which_spawner_to_use].point.y;
    int x = (int) coinSpawnManager.spawner_array[which_spawner_to_use].point.x;
#if DEBUG
    if(x > 32 || y > 32 || y < 0 || x < 0 )
    {
        printf("a");
    }
#endif
    map_point_t temp;
    memcpy(&temp, &coinSpawnManager.spawner_array[which_spawner_to_use], sizeof(map_point_t));
    coin_spawn_occ(temp);
    map[y * MAP_WIDTH + x].entity_type = coin_size;
}

void add_new_entity(entity_t entity, map_point_t map[])
{
    //entity spawn handler
    switch (entity) {
        case ENTITY_BEAST:
        {
            spawn_beast();
            break;
        }
        case ENTITY_COIN_SMALL:
        case ENTITY_COIN_BIG:
        case ENTITY_COIN_TREASURE:
        {
            spawn_coin(entity, map);
            break;
        }
        default:
            break;
    }
}

void beast_move(beast_t * beast_ptr, beast_t new_beast_loc)
{
#if DEBUG
    printf("Beast location updated from %d, %d to %d, %d\n", beast_ptr->curr_place.x, beast_ptr->curr_place.y,
           new_beast_loc.curr_place.x, new_beast_loc.curr_place.y);
#endif
    beast_ptr->curr_place.x = new_beast_loc.curr_place.x;
    beast_ptr->curr_place.y = new_beast_loc.curr_place.y;
}
void spawn_beast()
{
    //TODO this function spawns a beast on a new thread
}

//map updating function using ncurses

