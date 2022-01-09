#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ncurses.h>
#include <string.h>
#include "map.h"

//TODO: CELE PROJEKTOWE:
// serwer powinnien odpowiadać za:
//0. inicjalizacje, załadowanie(bądź generowanie) planszy
//1. do serwera powinna docierac kazda operacja na mapie poprzez interakcje międzyprocesowe(przesylanie informacji poprzez FIFO)
//2. spawnowanie nowych agentów, istot, rzeczy na mapie
//3. zdefiniowanie autonomicznego zachowania bestii, każda bestia na nowym wątku
//4. wywoływanie procesów graczy


#define DEBUG 0

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


typedef struct beast_t
{
    point_t curr_place;
} beast_t;

struct entity_ncurses_attributes_t
{
    entity_t entity;
    chtype ch;
    int color_p; // which color pair is assigned to the given entity
};

typedef struct coin_spawn_manager_t
{
    map_point_t * spawner_array;
    int free_spawners;
    int total_spawners;
} coin_spawn_manager_t;

//database storing the ncurses colors for each entity type
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

coin_spawn_manager_t coinSpawnManager;


int render_map(map_point_t map[], WINDOW * window);
void add_new_entity(entity_t, map_point_t map[]);
void spawn_beast();
void spawn_coin(entity_t coin_size, map_point_t map[]);
void beast_move(beast_t * beast_ptr, beast_t new_beast_loc);
int coin_spawn_init(map_point_t map[]);

int coin_spawn_occ(map_point_t occupied_point);
int coin_spawn_free(map_point_t freed_point);
int coin_spawn_search_for_element_internal(map_point_t element);

int main() {
    srand(time(NULL));
    map_point_t map[MAP_LENGTH * MAP_WIDTH];
    //TODO pre-game intialization menus

    int err = map_init(map);
    if(err != 0)
    {
        return 1;
    }

    err = coin_spawn_init(map);
    if(err != 0)
    {
        return 1;
    }
    //required ncurses initialization functions
    initscr();
    noecho();
    start_color();
    WINDOW * window = newwin(MAP_WIDTH + 1, MAP_LENGTH + 1,0,0);
    refresh();
    box(window, 0, 0);
    wrefresh(window);
    keypad(stdscr, true);

    //initializing colors for our database of tiles
    //NUM, BACKGROUND COLOR, LETTER COLOR
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
    //TODO main game loop
    int i = 0;
    bool user_did_not_quit = true;
    while (user_did_not_quit)
    {
        render_map(map, window);
        wrefresh(window);
        int c = getch();
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
        i++;
    }

    free(coinSpawnManager.spawner_array);
    endwin();
    return 0;
}
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
    int i = 0;
    for(;i < coinSpawnManager.total_spawners; i++)
    {
        if( element.point.x == arr->point.x  && element.point.y == arr->point.y)
        {
            break;
        }
    }
    if(i == coinSpawnManager.total_spawners)
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

    //swap znalezionego elemetu z ostatnim wolnym elementem
    int last_free_pos = coinSpawnManager.free_spawners - 1;
    swap(coinSpawnManager.spawner_array[last_free_pos], coinSpawnManager.spawner_array[pos] );
    return 0;
}
int coin_spawn_free(map_point_t freed_point)
{
    if(coinSpawnManager.free_spawners == coinSpawnManager.total_spawners)
        return -1;

    //znajdowanie elementu
    int pos = coin_spawn_search_for_element_internal(freed_point);

    //swap znalezionego elemetu z ostatnim zajetym elementem
    int last_occ_pos = coinSpawnManager.free_spawners;
    swap(coinSpawnManager.spawner_array[last_occ_pos], coinSpawnManager.spawner_array[pos]);
    return 0;
}
void spawn_coin(entity_t coin_size, map_point_t map[])
{
    if( coin_size < ENTITY_COIN_SMALL)
    {
        return;
    }

    int which_spawner_to_use = rand() % coinSpawnManager.free_spawners;
    int y = (int )coinSpawnManager.spawner_array[which_spawner_to_use].point.y;
    int x = (int) coinSpawnManager.spawner_array[which_spawner_to_use].point.x;
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
