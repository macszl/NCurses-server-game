#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ncurses.h>

//TODO: CELE PROJEKTOWE:
// serwer powinnien odpowiadać za:
//0. inicjalizacje, załadowanie(bądź generowanie) planszy
//1. do serwera powinna docierac kazda operacja na mapie poprzez interakcje międzyprocesowe(przesylanie informacji poprzez FIFO)
//2. spawnowanie nowych agentów, istot, rzeczy na mapie
//3. zdefiniowanie autonomicznego zachowania bestii, każda bestia na nowym wątku
//4. wywoływanie procesów graczy


#define DEBUG 0

//TODO these 2 parameters will be transferred to the players for error checking
const int MAP_LENGTH = 32;
const int MAP_WIDTH = 32;

typedef enum entity_t
{
    ENTITY_FREE = 0,
    ENTITY_WALL ,
    ENTITY_BUSH ,
    ENTITY_CAMPSITE ,
    ENTITY_PLAYER_1 ,
    ENTITY_PLAYER_2 ,
    ENTITY_PLAYER_3 ,
    ENTITY_PLAYER_4 ,
    ENTITY_BEAST ,
    ENTITY_COIN_SMALL ,
    ENTITY_COIN_BIG ,
    ENTITY_COIN_TREASURE ,
    ENTITY_COIN_DROPPED ,
} entity_t;

typedef enum spawner_type
{
    NO_SPAWNER = 0,
    COIN_SPAWNER ,
    BEAST_SPAWNER
} spawner_type;

typedef struct point_t
{
    unsigned int x;
    unsigned int y;
} point_t;

typedef struct map_point_t
{
    point_t point;
    entity_t entity_type;
    spawner_type spawnerType;
} map_point_t;

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

int map_init(map_point_t map[]);
int render_map(map_point_t map[], WINDOW * window);
void add_new_entity(entity_t, map_point_t map[]);
void spawn_beast();
void spawn_coin( int x, int y, entity_t coin_size, map_point_t map[]);
void beast_move(beast_t * beast_ptr, beast_t new_beast_loc);
int main() {
    map_point_t map[MAP_LENGTH * MAP_WIDTH];
    //TODO pre-game intialization menus

    //TODO map will be initialized from an existing file

    int err = map_init(map);
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

    endwin();
    return 0;
}
void spawn_coin(int x, int y,  entity_t coin_size, map_point_t map[])
{
    if( coin_size < ENTITY_COIN_SMALL)
    {
        return;
    }
    map[y * MAP_WIDTH + x].entity_type = coin_size;
}

void add_new_entity(entity_t entity, map_point_t map[])
{
    //this function finds a free map node and spawns an entity
    //TODO naive algorithm that works somewhat well for relatively empty maps
    //chokes when map is relatively full

    int free_x = 1;
    int free_y = 1;

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
            spawn_coin(free_x, free_y, entity, map);
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
int map_init(map_point_t map[]) {
    //initializes the map array with a simple 3x3 map
    //may load a bigger one from the file later on

//    for(int i = 0; i < MAP_WIDTH; i++)
//    {
//        for(int j = 0; j < MAP_LENGTH; j++)
//        {
//            map[i * MAP_WIDTH + j].point.y = (unsigned int ) i;
//            map[i * MAP_WIDTH + j].point.x = (unsigned int ) j;
//            map[i * MAP_WIDTH + j].entity_type = ENTITY_WALL;
//        }
//    }
//
//    map[3+1].entity_type = ENTITY_FREE;
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
