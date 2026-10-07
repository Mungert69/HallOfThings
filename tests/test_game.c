#include <hall_of_things/game.h>
#include <assert.h>
#include <stdio.h>

static int reachable(const Game *g, int floor, int sx, int sy, int tx, int ty) {
    unsigned char seen[HOTT_MAP_H][HOTT_MAP_W] = {{0}};
    int qx[HOTT_MAP_W * HOTT_MAP_H];
    int qy[HOTT_MAP_W * HOTT_MAP_H];
    int head = 0, tail = 0;
    qx[tail] = sx; qy[tail] = sy; ++tail;
    seen[sy][sx] = 1;
    static const int dx[4] = {1,-1,0,0};
    static const int dy[4] = {0,0,1,-1};
    while (head < tail) {
        int x = qx[head], y = qy[head]; ++head;
        if (x == tx && y == ty) return 1;
        for (int d=0; d<4; ++d) {
            int nx=x+dx[d], ny=y+dy[d];
            if (nx<0 || ny<0 || nx>=HOTT_MAP_W || ny>=HOTT_MAP_H || seen[ny][nx]) continue;
            if (!game_tile_walkable(g->floors[floor].tiles[ny][nx])) continue;
            seen[ny][nx]=1; qx[tail]=nx; qy[tail]=ny; ++tail;
        }
    }
    return 0;
}

int main(void) {
    /* Stress several seeds because the original algorithm links independently
       grown walks back into the existing maze. */
    for (unsigned seed = 1; seed <= 32; ++seed) {
        Game probe;
        game_init(&probe, seed);
        for (int fl = 1; fl < HOTT_FLOORS-1; ++fl)
            assert(reachable(&probe, fl, 4, 4, HOTT_MAP_W-5, HOTT_MAP_H-5));
    }

    Game g;
    game_init(&g, 0x12345678u);
    assert(g.floor == 0);
    assert(g.magic == 120);
    assert(g.arrows == 255);
    assert(g.wounds == 0);
    g.wounds = 126; g.magic = 1; g.arrows = 0; g.dead = true;
    game_test_refill(&g);
    assert(g.wounds == 0);
    assert(g.magic == 255);
    assert(g.arrows == 255);
    assert(!g.dead);

    int rings = 0;
    for (int t = TILE_RING_1; t <= TILE_RING_7; ++t) rings += game_debug_count_tile(&g, (Tile)t);
    assert(rings == 7);
    assert(game_debug_count_tile(&g, TILE_GOLD_KEY) == 1);
    assert(game_debug_count_tile(&g, TILE_STAIRS_UP) == 7);
    assert(game_debug_count_tile(&g, TILE_STAIRS_DOWN) == 7);

    /* Original 16x16 room topology must expand to a connected 113x113 floor. */
    for (int fl = 1; fl < HOTT_FLOORS-1; ++fl)
        assert(reachable(&g, fl, 4, 4, HOTT_MAP_W-5, HOTT_MAP_H-5));

    /* 8-way movement: diagonal movement updates facing and moves one tile. */
    for (int i = 0; i < HOTT_MAX_MONSTERS; ++i) g.floors[0].monsters[i].active = false;
    g.floor = 0;
    g.player_x = 10; g.player_y = 10;
    g.floors[0].tiles[10][10] = TILE_FLOOR;
    g.floors[0].tiles[10][11] = TILE_FLOOR;
    g.floors[0].tiles[9][10] = TILE_FLOOR;
    g.floors[0].tiles[9][11] = TILE_FLOOR;
    game_move(&g, DIR_UP_RIGHT);
    assert(g.player_x == 11 && g.player_y == 9);
    assert(g.facing == DIR_UP_RIGHT);

    /* Facing persists even when movement is blocked, so weapons keep aiming there. */
    g.player_x = 20; g.player_y = 20;
    g.floors[0].tiles[20][20] = TILE_FLOOR;
    g.floors[0].tiles[20][19] = TILE_FLOOR;
    g.floors[0].tiles[21][20] = TILE_FLOOR;
    g.floors[0].tiles[21][19] = TILE_WALL;
    game_move(&g, DIR_DOWN_LEFT);
    assert(g.player_x == 20 && g.player_y == 20);
    assert(g.facing == DIR_DOWN_LEFT);

    /* Lightning always starts along the persistent facing vector and reflects. */
    for (int i = 0; i < HOTT_MAX_PROJECTILES; ++i) g.projectiles[i].active = false;
    g.magic = 100;
    g.player_x = 30; g.player_y = 30; g.facing = DIR_UP_RIGHT;
    for (int y = 28; y <= 31; ++y) for (int x = 28; x <= 32; ++x) g.floors[0].tiles[y][x] = TILE_FLOOR;
    g.floors[0].tiles[29][31] = TILE_WALL;
    g.floors[0].tiles[30][31] = TILE_WALL;
    game_lightning(&g);
    Projectile *light = NULL;
    for (int i = 0; i < HOTT_MAX_PROJECTILES; ++i)
        if (g.projectiles[i].active && g.projectiles[i].kind == PROJ_LIGHTNING) light = &g.projectiles[i];
    assert(light && light->dx == 1 && light->dy == -1);
    game_update(&g, 50);
    assert(light->active && light->dx == -1 && light->dy == -1);
    assert(light->x == 29 && light->y == 29);

    /* Fireball acquires a Thing and retargets when it moves. */
    for (int i = 0; i < HOTT_MAX_PROJECTILES; ++i) g.projectiles[i].active = false;
    for (int i = 0; i < HOTT_MAX_MONSTERS; ++i) g.floors[0].monsters[i].active = false;
    for (int y = 44; y <= 52; ++y) for (int x = 46; x <= 55; ++x) g.floors[0].tiles[y][x] = TILE_FLOOR;
    g.player_x = 50; g.player_y = 50; g.facing = DIR_LEFT; g.magic = 100;
    Monster *target = &g.floors[0].monsters[0];
    target->active = true; target->x = 54; target->y = 50; target->wounds = 100; target->move_cooldown_ms = 100000;
    game_fireball(&g);
    Projectile *fire = NULL;
    for (int i = 0; i < HOTT_MAX_PROJECTILES; ++i)
        if (g.projectiles[i].active && g.projectiles[i].kind == PROJ_FIREBALL) fire = &g.projectiles[i];
    assert(fire && fire->target_index == 0 && fire->dx == 1 && fire->dy == 0);
    target->x = 50; target->y = 45;
    game_update(&g, 80);
    assert(fire->active && fire->dy == -1);

    /* Pickups are automatic on entry, but capacity-limited objects remain if full. */
    for (int i = 0; i < HOTT_MAX_MONSTERS; ++i) g.floors[0].monsters[i].active = false;
    for (int i = 0; i < HOTT_MAX_PROJECTILES; ++i) g.projectiles[i].active = false;
    g.player_x = 70; g.player_y = 70; g.facing = DIR_RIGHT;
    g.floors[0].tiles[70][70] = TILE_FLOOR;
    g.floors[0].tiles[70][71] = TILE_MAGIC;
    g.magic = 255;
    game_move(&g, DIR_RIGHT);
    assert(g.player_x == 71 && g.magic == 255);
    assert(g.floors[0].tiles[70][71] == TILE_MAGIC);

    /* As soon as capacity is available while standing on it, the object is collected. */
    g.magic = 240;
    game_update(&g, 1);
    assert(g.magic > 240 && g.magic <= 255);
    assert(g.floors[0].tiles[70][71] == TILE_FLOOR);

    g.player_x = 80; g.player_y = 80; g.facing = DIR_RIGHT;
    g.floors[0].tiles[80][80] = TILE_FLOOR;
    g.floors[0].tiles[80][81] = TILE_TREASURE;
    g.treasure_count = HOTT_MAX_TREASURE - 1;
    game_move(&g, DIR_RIGHT);
    assert(g.treasure_count == HOTT_MAX_TREASURE);
    assert(g.floors[0].tiles[80][81] == TILE_FLOOR);

    printf("core tests passed: original 16x16 maze topology, movement, weapons and automatic pickups\n");
    return 0;
}
