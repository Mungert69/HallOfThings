#ifndef HOTT_GAME_H
#define HOTT_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define HOTT_FLOORS 8
#define HOTT_MAP_W 113
#define HOTT_MAP_H 113
#define HOTT_MAX_MONSTERS 32
#define HOTT_MAX_PROJECTILES 32
#define HOTT_MAX_TREASURE 5

/* Native timing derived from the original relative projectile speeds. */
#define HOTT_ARROW_STEP_MS 45
#define HOTT_FIREBALL_STEP_MS 72
#define HOTT_LIGHTNING_STEP_MS 42

typedef enum {
    TILE_WALL = 0,
    TILE_FLOOR,
    TILE_DOOR_CLOSED,
    TILE_DOOR_OPEN,
    TILE_STAIRS_UP,
    TILE_STAIRS_DOWN,
    TILE_RING_1,
    TILE_RING_2,
    TILE_RING_3,
    TILE_RING_4,
    TILE_RING_5,
    TILE_RING_6,
    TILE_RING_7,
    TILE_GOLD_KEY,
    TILE_MAGIC,
    TILE_ARROWS,
    TILE_TREASURE,
    TILE_GATE_CLOSED
} Tile;

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
    DIR_UP_LEFT,
    DIR_UP_RIGHT,
    DIR_DOWN_RIGHT,
    DIR_DOWN_LEFT
} Direction;
typedef enum { PROJ_ARROW = 0, PROJ_FIREBALL = 1, PROJ_LIGHTNING = 2 } ProjectileKind;

typedef struct {
    bool active;
    int x, y;
    int wounds;
    int max_wounds;
    int move_type;
    int move_cooldown_ms;
    int attack_cooldown_ms;
} Monster;

typedef struct {
    bool active;
    int x, y;
    int prev_x, prev_y;
    int dx, dy;
    int life;
    int damage;
    int step_cooldown_ms;
    int target_index;
    ProjectileKind kind;
} Projectile;

typedef struct {
    Tile tiles[HOTT_MAP_H][HOTT_MAP_W];
    Monster monsters[HOTT_MAX_MONSTERS];
} FloorState;

typedef struct {
    FloorState floors[HOTT_FLOORS];
    int floor;
    int player_x, player_y;
    Direction facing;

    int magic;
    int wounds;
    int arrows;
    int rings;
    uint8_t rings_mask;
    bool has_key;
    int treasure_count;
    int score;
    int monsters_killed;

    Projectile projectiles[HOTT_MAX_PROJECTILES];
    bool dead;
    bool won;
    bool show_status;
    bool show_help;
    bool show_cheat;
    uint32_t rng;
    int sword_cooldown_ms;
    int message_ms;
    char message[128];
} Game;

void game_init(Game *g, uint32_t seed);
void game_update(Game *g, int dt_ms);
void game_move(Game *g, Direction d);
void game_open_door(Game *g);
void game_close_door(Game *g);
void game_keep(Game *g);
void game_drop(Game *g);
void game_sword(Game *g);
void game_fire_arrow(Game *g);
void game_fireball(Game *g);
void game_lightning(Game *g);
void game_heal(Game *g);
void game_test_refill(Game *g);
void game_toggle_status(Game *g);

bool game_tile_walkable(Tile t);
const char *game_tile_name(Tile t);
int game_debug_count_tile(const Game *g, Tile t);

#endif
