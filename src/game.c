#include <hall_of_things/game.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MAGIC 255
#define MAX_WOUNDS 127
#define START_MAGIC 120
#define START_ARROWS 255
#define FIREBALL_COST 2
#define LIGHTNING_COST 1
#define HEAL_COST 30
#define SWORD_DAMAGE 10
#define FIRE_DAMAGE 40
#define LIGHTNING_DAMAGE 40
#define ARROW_LIFE_MAN 32
#define FIREBALL_LIFE 50
#define FIREBALL_SEARCH_RANGE 40
#define LIGHTNING_LIFE 100

static const int DX[8] = {0, 1, 0, -1, -1, 1, 1, -1};
static const int DY[8] = {-1, 0, 1, 0, -1, -1, 1, 1};

static uint32_t rnd(Game *g) {
    uint32_t x = g->rng ? g->rng : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static int rr(Game *g, int n) {
    return n <= 1 ? 0 : (int)(rnd(g) % (uint32_t)n);
}

static bool inside(int x, int y) {
    return x > 0 && y > 0 && x < HOTT_MAP_W - 1 && y < HOTT_MAP_H - 1;
}

static void say(Game *g, const char *text) {
    snprintf(g->message, sizeof(g->message), "%s", text);
    g->message_ms = 2200;
}

static bool is_item(Tile t) {
    return (t >= TILE_RING_1 && t <= TILE_TREASURE);
}

static void auto_pickup_current(Game *g);

bool game_tile_walkable(Tile t) {
    return t != TILE_WALL && t != TILE_DOOR_CLOSED && t != TILE_GATE_CLOSED;
}

const char *game_tile_name(Tile t) {
    switch (t) {
        case TILE_WALL: return "wall";
        case TILE_FLOOR: return "floor";
        case TILE_DOOR_CLOSED: return "closed door";
        case TILE_DOOR_OPEN: return "open door";
        case TILE_STAIRS_UP: return "stairs up";
        case TILE_STAIRS_DOWN: return "stairs down";
        case TILE_RING_1: case TILE_RING_2: case TILE_RING_3: case TILE_RING_4:
        case TILE_RING_5: case TILE_RING_6: case TILE_RING_7: return "ring";
        case TILE_GOLD_KEY: return "golden key";
        case TILE_MAGIC: return "magic";
        case TILE_ARROWS: return "arrows";
        case TILE_TREASURE: return "treasure";
        case TILE_GATE_CLOSED: return "sealed gate";
    }
    return "unknown";
}

static Monster *monster_at(Game *g, int x, int y) {
    Monster *m = g->floors[g->floor].monsters;
    for (int i = 0; i < HOTT_MAX_MONSTERS; ++i)
        if (m[i].active && m[i].x == x && m[i].y == y) return &m[i];
    return NULL;
}

static bool monster_any_at(const FloorState *f, int x, int y) {
    for (int i = 0; i < HOTT_MAX_MONSTERS; ++i)
        if (f->monsters[i].active && f->monsters[i].x == x && f->monsters[i].y == y) return true;
    return false;
}

#define ROOM_GRID 16
#define ROOM_PITCH 7

/* Original Halls small-maze representation: one byte per room in a 16x16
   array. Each wall/connection uses two bits:
     bits 7..6 = up, 5..4 = down, 3..2 = left, 1..0 = right
   Values 0..3 are the original wall forms. */
#define CON_RIGHT_MASK 0x03u
#define CON_LEFT_MASK  0x0Cu
#define CON_DOWN_MASK  0x30u
#define CON_UP_MASK    0xC0u

static const uint8_t CON_PROB_EXTRA[8] = {0,255,200,150,127,100,60,20};
static const uint8_t CON_PROB_1[8]     = {0,150,100, 40, 23, 13, 7, 4};
static const uint8_t CON_PROB_2[8]     = {0,200,100, 50, 30, 20,10, 5};

static int original_connection_type(Game *g, int floor_no) {
    const int f = floor_no & 7;
    const uint8_t r = (uint8_t)rnd(g);
    if (r < CON_PROB_1[f]) return 1;
    if (r < CON_PROB_2[f]) return 2;
    return 3;
}

static int room_neighbor(int index, int dir) {
    const int x = index & 15;
    const int y = index >> 4;
    switch (dir) {
        case 0: return y > 0  ? index - 16 : -1; /* up */
        case 1: return y < 15 ? index + 16 : -1; /* down */
        case 2: return x > 0  ? index - 1  : -1; /* left */
        case 3: return x < 15 ? index + 1  : -1; /* right */
        default: return -1;
    }
}

static uint8_t room_side_bits(uint8_t value, int dir) {
    switch (dir) {
        case 0: return (uint8_t)((value & CON_UP_MASK) >> 6);
        case 1: return (uint8_t)((value & CON_DOWN_MASK) >> 4);
        case 2: return (uint8_t)((value & CON_LEFT_MASK) >> 2);
        case 3: return (uint8_t)( value & CON_RIGHT_MASK);
        default: return 0;
    }
}

static void room_set_side(uint8_t maze[256], int index, int dir, int type) {
    int n = room_neighbor(index, dir);
    if (n < 0) return;
    uint8_t t = (uint8_t)(type & 3);
    switch (dir) {
        case 0:
            maze[index] |= (uint8_t)(t << 6);
            maze[n]     |= (uint8_t)(t << 4);
            break;
        case 1:
            maze[index] |= (uint8_t)(t << 4);
            maze[n]     |= (uint8_t)(t << 6);
            break;
        case 2:
            maze[index] |= (uint8_t)(t << 2);
            maze[n]     |= t;
            break;
        case 3:
            maze[index] |= t;
            maze[n]     |= (uint8_t)(t << 2);
            break;
    }
}

static int original_find_unvisited_moves(const uint8_t maze[256], int index, uint8_t moves[4]) {
    int count = 0;
    for (int dir = 0; dir < 4; ++dir) {
        int n = room_neighbor(index, dir);
        if (n >= 0 && maze[n] == 0) moves[count++] = (uint8_t)dir;
    }
    return count;
}

static int original_find_links(const uint8_t maze[256], int index, uint8_t moves[4]) {
    int count = 0;
    for (int dir = 0; dir < 4; ++dir) {
        int n = room_neighbor(index, dir);
        if (n < 0 || maze[n] == 0) continue;

        /* Match TryLinkUP/DOWN/LEFT/RIGHT: only link when the existing
           neighbour has no reciprocal connection on this boundary. */
        int opposite = dir ^ 1;
        if (dir >= 2) opposite = (dir == 2) ? 3 : 2;
        if (room_side_bits(maze[n], opposite) == 0) moves[count++] = (uint8_t)dir;
    }
    return count;
}

static void original_move(Game *g, uint8_t maze[256], int *index,
                          const uint8_t *moves, int count, int floor_no) {
    if (count <= 0) return;
    int dir = moves[rr(g, count)];
    int type = original_connection_type(g, floor_no);
    room_set_side(maze, *index, dir, type);
    *index = room_neighbor(*index, dir);
}

static void original_link(Game *g, uint8_t maze[256], int index,
                          const uint8_t *moves, int count, int floor_no) {
    int temporary = index;
    original_move(g, maze, &temporary, moves, count, floor_no);
}

/* Port of the original Main/Draw/FindMoves/FindNext/FindLinks/Link topology
   generator. It intentionally keeps our native RNG, so a numeric seed is
   repeatable in this port but does not reproduce the Spectrum's byte-for-byte
   random sequence. */
static void build_original_small_maze(Game *g, uint8_t maze[256], int floor_no) {
    memset(maze, 0, 256);
    int index = (int)((uint8_t)rnd(g));
    uint8_t moves[4];

    for (;;) {
        /* Draw: random walk through unused rooms until boxed in. */
        for (;;) {
            int count = original_find_unvisited_moves(maze, index, moves);
            if (count == 0) break;
            original_move(g, maze, &index, moves, count, floor_no);
        }

        /* Rnd7: possibly add another link from the end of the walk. */
        if ((uint8_t)rnd(g) < CON_PROB_EXTRA[floor_no & 7]) {
            int count = original_find_links(maze, index, moves);
            if (count > 0) original_link(g, maze, index, moves, count, floor_no);
        }

        /* FindNext: scan forward, wrapping at 256, for the next untouched
           room that can be joined to the already-built maze. */
        bool found = false;
        for (int scanned = 0; scanned < 256; ++scanned) {
            index = (index + 1) & 255;
            if (maze[index] != 0) continue;
            int count = original_find_links(maze, index, moves);
            if (count == 0) continue;
            original_link(g, maze, index, moves, count, floor_no);
            found = true;
            break;
        }
        if (!found) return;
    }
}

static void fill_floor(FloorState *f, Tile t) {
    for (int y = 0; y < HOTT_MAP_H; ++y)
        for (int x = 0; x < HOTT_MAP_W; ++x)
            f->tiles[y][x] = t;
}

static void wall_h(FloorState *f, int x0, int x1, int y) {
    for (int x = x0; x <= x1; ++x)
        if (x >= 0 && x < HOTT_MAP_W && y >= 0 && y < HOTT_MAP_H)
            f->tiles[y][x] = TILE_WALL;
}

static void wall_v(FloorState *f, int x, int y0, int y1) {
    for (int y = y0; y <= y1; ++y)
        if (x >= 0 && x < HOTT_MAP_W && y >= 0 && y < HOTT_MAP_H)
            f->tiles[y][x] = TILE_WALL;
}

static void gap_h(FloorState *f, int x0, int x1, int y) {
    for (int x = x0; x <= x1; ++x)
        if (x > 0 && x < HOTT_MAP_W-1 && y > 0 && y < HOTT_MAP_H-1)
            f->tiles[y][x] = TILE_FLOOR;
}

static void gap_v(FloorState *f, int x, int y0, int y1) {
    for (int y = y0; y <= y1; ++y)
        if (x > 0 && x < HOTT_MAP_W-1 && y > 0 && y < HOTT_MAP_H-1)
            f->tiles[y][x] = TILE_FLOOR;
}

static void open_horizontal_original(FloorState *f, int x0, int y, int type) {
    /* Original WallDefs uses six characters between 7-cell-pitch corners:
       type 0 solid, 1 door, 2 open doorway, 3 no wall. In our modern visual
       language doors are gaps, so 1 and 2 are both passable openings. */
    if (type == 1) gap_h(f, x0 + 3, x0 + 4, y);
    else if (type == 2) gap_h(f, x0 + 2, x0 + 5, y);
    else if (type == 3) gap_h(f, x0 + 1, x0 + 6, y);
}

static void open_vertical_original(FloorState *f, int x, int y0, int type) {
    if (type == 1) gap_v(f, x, y0 + 3, y0 + 4);
    else if (type == 2) gap_v(f, x, y0 + 2, y0 + 5);
    else if (type == 3) gap_v(f, x, y0 + 1, y0 + 6);
}

static void expand_original_small_maze(FloorState *f, const uint8_t maze[256]) {
    fill_floor(f, TILE_FLOOR);

    /* Draw the 7-cell-pitch grid first. */
    for (int gy = 0; gy <= ROOM_GRID; ++gy) wall_h(f, 0, HOTT_MAP_W-1, gy * ROOM_PITCH);
    for (int gx = 0; gx <= ROOM_GRID; ++gx) wall_v(f, gx * ROOM_PITCH, 0, HOTT_MAP_H-1);

    /* Apply the generated room connection forms. Right/down are sufficient
       because both sides of each connection carry the same type. */
    for (int ry = 0; ry < ROOM_GRID; ++ry) {
        for (int rx = 0; rx < ROOM_GRID; ++rx) {
            int index = ry * 16 + rx;
            if (rx < 15) {
                int type = room_side_bits(maze[index], 3);
                if (type) open_vertical_original(f, (rx+1)*ROOM_PITCH, ry*ROOM_PITCH, type);
            }
            if (ry < 15) {
                int type = room_side_bits(maze[index], 1);
                if (type) open_horizontal_original(f, rx*ROOM_PITCH, (ry+1)*ROOM_PITCH, type);
            }
        }
    }
}

static void carve_original_floor(Game *g, FloorState *f, int floor_no) {
    if ((floor_no & 7) == 0) {
        /* The original PAGE routine treats floor 0 specially as open-plan
           rather than running Page over the generated room maze. */
        fill_floor(f, TILE_FLOOR);
        wall_h(f, 0, HOTT_MAP_W-1, 0);
        wall_h(f, 0, HOTT_MAP_W-1, HOTT_MAP_H-1);
        wall_v(f, 0, 0, HOTT_MAP_H-1);
        wall_v(f, HOTT_MAP_W-1, 0, HOTT_MAP_H-1);
        return;
    }

    uint8_t maze[256];
    build_original_small_maze(g, maze, floor_no);
    expand_original_small_maze(f, maze);
}

static void random_floor_pos(Game *g, FloorState *f, int *x, int *y) {
    for (;;) {
        int tx = 1 + rr(g, HOTT_MAP_W - 2);
        int ty = 1 + rr(g, HOTT_MAP_H - 2);
        if (f->tiles[ty][tx] == TILE_FLOOR && !monster_any_at(f, tx, ty)) {
            *x = tx; *y = ty; return;
        }
    }
}

static void place_tile_random(Game *g, FloorState *f, Tile t) {
    int x, y; random_floor_pos(g, f, &x, &y); f->tiles[y][x] = t;
}

static void build_sanctuary(Game *g, FloorState *f) {
    (void)g;
    const int cx = HOTT_MAP_W / 2, cy = HOTT_MAP_H / 2;
    for (int y = cy - 3; y <= cy + 3; ++y)
        for (int x = cx - 5; x <= cx + 5; ++x)
            f->tiles[y][x] = (x == cx - 5 || x == cx + 5 || y == cy - 3 || y == cy + 3) ? TILE_WALL : TILE_FLOOR;
    f->tiles[cy][cx - 5] = TILE_GATE_CLOSED;
    f->tiles[cy][cx] = TILE_GOLD_KEY;
}

static void spawn_monsters(Game *g, int floor_no) {
    FloorState *f = &g->floors[floor_no];
    int count = HOTT_MAX_MONSTERS;
    for (int i = 0; i < count; ++i) {
        int x, y; random_floor_pos(g, f, &x, &y);
        if ((floor_no == 0 && abs(x - HOTT_MAP_W/2) < 7 && abs(y - HOTT_MAP_H/2) < 5) ||
            (x < 5 && y < 5)) { --i; continue; }
        Monster *m = &f->monsters[i];
        m->active = true;
        m->x = x; m->y = y;
        m->max_wounds = 30 + rr(g, 21); /* mirrors 30..49 variation */
        m->wounds = m->max_wounds;
        m->move_type = rr(g, 4);
        m->move_cooldown_ms = 80 + rr(g, 300);
        m->attack_cooldown_ms = 0;
    }
}

static void generate_world(Game *g) {
    int ring_floor[7];
    for (int i = 0; i < 7; ++i) ring_floor[i] = 1 + rr(g, 7);

    for (int fl = 0; fl < HOTT_FLOORS; ++fl) {
        FloorState *f = &g->floors[fl];
        memset(f, 0, sizeof(*f));
        carve_original_floor(g, f, fl);

        /* Stable stair locations are forced open so every floor is reachable. */
        f->tiles[4][4] = fl > 0 ? TILE_STAIRS_DOWN : TILE_FLOOR;
        f->tiles[HOTT_MAP_H-5][HOTT_MAP_W-5] = fl < HOTT_FLOORS-1 ? TILE_STAIRS_UP : TILE_FLOOR;

        for (int i = 0; i < 7; ++i)
            if (ring_floor[i] == fl) place_tile_random(g, f, (Tile)(TILE_RING_1 + i));

        for (int i = 0; i < 10; ++i) place_tile_random(g, f, TILE_MAGIC);
        for (int i = 0; i < 8; ++i) place_tile_random(g, f, TILE_ARROWS);
        for (int i = 0; i < 10; ++i) place_tile_random(g, f, TILE_TREASURE);

        if (fl == 0) build_sanctuary(g, f);
        spawn_monsters(g, fl);
    }
}

void game_init(Game *g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0x19830303u;
    generate_world(g);
    g->floor = 0;
    g->player_x = 4;
    g->player_y = 4;
    g->facing = DIR_RIGHT;
    g->magic = START_MAGIC;
    g->arrows = START_ARROWS;
    g->wounds = 0;
    g->sword_cooldown_ms = 0;
    say(g, "Find the seven rings. The golden key is sealed below.");
}

static void floor_transition(Game *g, int new_floor, bool came_from_below) {
    if (new_floor < 0 || new_floor >= HOTT_FLOORS) return;
    g->floor = new_floor;
    memset(g->projectiles, 0, sizeof(g->projectiles));
    if (came_from_below) {
        g->player_x = 4; g->player_y = 4;
    } else {
        g->player_x = HOTT_MAP_W - 5; g->player_y = HOTT_MAP_H - 5;
    }
    char b[64]; snprintf(b, sizeof(b), "Floor %d", g->floor); say(g, b);
}

void game_move(Game *g, Direction d) {
    if (g->dead || g->won) return;
    g->facing = d;

    int nx = g->player_x + DX[d], ny = g->player_y + DY[d];
    if (!inside(nx, ny)) return;

    /* Do not allow diagonal movement to cut through the corner of two walls. */
    if (DX[d] != 0 && DY[d] != 0) {
        Tile tx = g->floors[g->floor].tiles[g->player_y][g->player_x + DX[d]];
        Tile ty = g->floors[g->floor].tiles[g->player_y + DY[d]][g->player_x];
        if (!game_tile_walkable(tx) || !game_tile_walkable(ty)) return;
    }

    if (monster_at(g, nx, ny)) { game_sword(g); return; }
    Tile t = g->floors[g->floor].tiles[ny][nx];
    if (!game_tile_walkable(t)) return;
    g->player_x = nx; g->player_y = ny;
    auto_pickup_current(g);
    if (g->won) return;
    if (t == TILE_STAIRS_UP && g->floor < HOTT_FLOORS - 1) floor_transition(g, g->floor + 1, true);
    else if (t == TILE_STAIRS_DOWN && g->floor > 0) floor_transition(g, g->floor - 1, false);
}

static void front_cell(Game *g, int *x, int *y) {
    *x = g->player_x + DX[g->facing];
    *y = g->player_y + DY[g->facing];
}

void game_open_door(Game *g) {
    int x,y; front_cell(g,&x,&y); if (!inside(x,y)) return;
    Tile *t = &g->floors[g->floor].tiles[y][x];
    if (*t == TILE_DOOR_CLOSED) { *t = TILE_DOOR_OPEN; say(g, "Door opened."); }
}

void game_close_door(Game *g) {
    int x,y; front_cell(g,&x,&y); if (!inside(x,y)) return;
    Tile *t = &g->floors[g->floor].tiles[y][x];
    if (*t == TILE_DOOR_OPEN && !monster_at(g,x,y) && !(g->player_x==x && g->player_y==y)) {
        *t = TILE_DOOR_CLOSED; say(g, "Door closed.");
    }
}

static void unlock_gold_gate(Game *g) {
    FloorState *f = &g->floors[0];
    for (int y=0;y<HOTT_MAP_H;++y) for (int x=0;x<HOTT_MAP_W;++x)
        if (f->tiles[y][x] == TILE_GATE_CLOSED) f->tiles[y][x] = TILE_DOOR_OPEN;
}

static bool pickup_at(Game *g, int x, int y) {
    if (!inside(x,y)) return false;
    Tile *t = &g->floors[g->floor].tiles[y][x];
    Tile item = *t;

    if (item >= TILE_RING_1 && item <= TILE_RING_7) {
        int bit = item - TILE_RING_1;
        if (g->rings_mask & (1u << bit)) return false;
        g->rings_mask |= (uint8_t)(1u << bit);
        g->rings++;
        g->score += 500;
        *t = TILE_FLOOR;
        if (g->rings == 7) {
            unlock_gold_gate(g);
            say(g, "All seven rings! The bottom-floor seal has opened.");
        } else {
            say(g, "Ring collected.");
        }
        return true;
    }

    if (item == TILE_GOLD_KEY) {
        if (g->rings < 7) return false;
        g->has_key = true;
        *t = TILE_FLOOR;
        g->score += 2000;
        g->won = true;
        say(g, "You have the golden key. You are victorious!");
        return true;
    }

    if (item == TILE_MAGIC) {
        if (g->magic >= MAX_MAGIC) return false;
        g->magic += 12 + rr(g, 20);
        if (g->magic > MAX_MAGIC) g->magic = MAX_MAGIC;
        *t = TILE_FLOOR;
        say(g, "Magic restored.");
        return true;
    }

    if (item == TILE_ARROWS) {
        if (g->arrows >= 255) return false;
        g->arrows += 8;
        if (g->arrows > 255) g->arrows = 255;
        *t = TILE_FLOOR;
        say(g, "Arrows collected.");
        return true;
    }

    if (item == TILE_TREASURE) {
        if (g->treasure_count >= HOTT_MAX_TREASURE) return false;
        g->treasure_count++;
        g->score += 50;
        *t = TILE_FLOOR;
        say(g, "Treasure collected.");
        return true;
    }

    return false;
}

static void auto_pickup_current(Game *g) {
    pickup_at(g, g->player_x, g->player_y);
}

void game_keep(Game *g) {
    Tile here = g->floors[g->floor].tiles[g->player_y][g->player_x];
    if (is_item(here)) {
        pickup_at(g, g->player_x, g->player_y);
        return;
    }

    int x,y;
    front_cell(g,&x,&y);
    pickup_at(g,x,y);
}

void game_drop(Game *g) {
    if (g->treasure_count <= 0) return;
    int x,y; front_cell(g,&x,&y); if (!inside(x,y)) return;
    Tile *t=&g->floors[g->floor].tiles[y][x];
    if (*t == TILE_FLOOR && !monster_at(g,x,y)) { *t=TILE_TREASURE; g->treasure_count--; say(g,"Treasure dropped."); }
}

static void hurt_monster(Game *g, Monster *m, int damage) {
    if (!m || !m->active) return;
    m->wounds -= damage;
    if (m->wounds <= 0) {
        m->active=false; g->monsters_killed++; g->score += 100;
        say(g,"Monster killed.");
    }
}

void game_sword(Game *g) {
    if (g->sword_cooldown_ms > 0 || g->dead || g->won) return;
    int x,y; front_cell(g,&x,&y);
    Monster *m=monster_at(g,x,y);
    if (m) hurt_monster(g,m,SWORD_DAMAGE);
    g->sword_cooldown_ms=180;
}

static Projectile *new_projectile(Game *g) {
    for (int i=0;i<HOTT_MAX_PROJECTILES;++i)
        if (!g->projectiles[i].active) return &g->projectiles[i];
    return NULL;
}

static Projectile *launch(Game *g, ProjectileKind kind, int damage, int life) {
    Projectile *p=new_projectile(g);
    if(!p) return NULL;
    memset(p,0,sizeof(*p));
    p->active=true;
    p->x=g->player_x; p->y=g->player_y;
    p->prev_x=p->x; p->prev_y=p->y;
    p->dx=DX[g->facing]; p->dy=DY[g->facing];
    p->life=life; p->damage=damage;
    p->target_index=-1;
    p->kind=kind; p->step_cooldown_ms=0;
    return p;
}

static int nearest_monster_index_in_range(const Game *g,int x,int y,int range) {
    const FloorState *f=&g->floors[g->floor];
    int best=-1,bestd=0x7fffffff;
    int range2=range*range;
    for(int i=0;i<HOTT_MAX_MONSTERS;++i){
        const Monster *m=&f->monsters[i];
        if(!m->active) continue;
        int dx=m->x-x,dy=m->y-y;
        int d=dx*dx+dy*dy;
        if(d<=range2 && d<bestd){best=i;bestd=d;}
    }
    return best;
}

void game_fire_arrow(Game *g) {
    if (g->dead || g->won) return;
    if (g->arrows <= 0) { say(g,"No arrows."); return; }
    if (!launch(g,PROJ_ARROW,18,ARROW_LIFE_MAN)) { say(g,"Too many shots in flight."); return; }
    g->arrows--;
}

void game_fireball(Game *g) {
    if (g->dead || g->won) return;
    if (g->magic < FIREBALL_COST) { say(g,"Not enough magic."); return; }
    Projectile *p=launch(g,PROJ_FIREBALL,FIRE_DAMAGE,FIREBALL_LIFE);
    if(!p) { say(g,"Too many shots in flight."); return; }
    g->magic-=FIREBALL_COST;
    p->target_index=nearest_monster_index_in_range(g,p->x,p->y,FIREBALL_SEARCH_RANGE);
    if(p->target_index>=0){
        const Monster *m=&g->floors[g->floor].monsters[p->target_index];
        p->dx=(m->x>p->x)-(m->x<p->x);
        p->dy=(m->y>p->y)-(m->y<p->y);
        say(g,"Fireball tracking.");
    } else {
        p->active=false;
        say(g,"No Thing in fireball range.");
    }
}

void game_lightning(Game *g) {
    if (g->dead || g->won) return;
    if (g->magic < LIGHTNING_COST) { say(g,"Not enough magic."); return; }
    Projectile *p=launch(g,PROJ_LIGHTNING,LIGHTNING_DAMAGE,LIGHTNING_LIFE);
    if(!p) { say(g,"Too many shots in flight."); return; }
    g->magic-=LIGHTNING_COST;
    say(g,"Lightning released.");
}

void game_heal(Game *g) {
    if (g->dead || g->won || g->magic < HEAL_COST || g->wounds <= 0) return;
    g->magic -= HEAL_COST;
    g->wounds -= 16 + rr(g,16);
    if(g->wounds<0)g->wounds=0;
    say(g,"Healing spell cast.");
}

void game_test_refill(Game *g){
    g->wounds=0;
    g->magic=MAX_MAGIC;
    g->arrows=255;
    g->dead=false;
    g->sword_cooldown_ms=0;
    memset(g->projectiles,0,sizeof(g->projectiles));
    say(g,"TEST REFILL: wounds 0, magic 255, arrows 255.");
}

void game_toggle_status(Game *g){g->show_status=!g->show_status;}

static void hurt_player(Game *g,int amount){
    g->wounds += amount;
    if(g->wounds>=MAX_WOUNDS){g->wounds=MAX_WOUNDS;g->dead=true;say(g,"You have been killed.");}
}

static bool projectile_blocked(const FloorState *f,int x,int y){
    if(!inside(x,y)) return true;
    Tile t=f->tiles[y][x];
    return t==TILE_WALL||t==TILE_DOOR_CLOSED||t==TILE_GATE_CLOSED;
}

static bool projectile_can_step(const FloorState *f,const Projectile *p,int dx,int dy){
    if(dx==0 && dy==0) return false;
    if(projectile_blocked(f,p->x+dx,p->y+dy)) return false;
    if(dx!=0 && dy!=0){
        if(projectile_blocked(f,p->x+dx,p->y) || projectile_blocked(f,p->x,p->y+dy)) return false;
    }
    return true;
}

static void steer_fireball(Game *g,Projectile *p){
    FloorState *f=&g->floors[g->floor];
    if(p->target_index<0 || p->target_index>=HOTT_MAX_MONSTERS || !f->monsters[p->target_index].active)
        p->target_index=nearest_monster_index_in_range(g,p->x,p->y,FIREBALL_SEARCH_RANGE);
    if(p->target_index<0) return;

    const Monster *target=&f->monsters[p->target_index];
    int best_dx=p->dx,best_dy=p->dy,best_score=0x7fffffff;
    for(int d=0;d<8;++d){
        int dx=DX[d],dy=DY[d];
        if(!projectile_can_step(f,p,dx,dy)) continue;
        int nx=p->x+dx,ny=p->y+dy;
        int tx=target->x-nx,ty=target->y-ny;
        int score=(tx*tx+ty*ty)*16;
        if(dx==p->dx && dy==p->dy) score-=2;
        else if(dx==-p->dx && dy==-p->dy) score+=24;
        else score+=1;
        if(score<best_score){best_score=score;best_dx=dx;best_dy=dy;}
    }
    p->dx=best_dx; p->dy=best_dy;
}

static void update_projectiles(Game *g,int dt){
    FloorState *f=&g->floors[g->floor];
    for(int i=0;i<HOTT_MAX_PROJECTILES;++i){
        Projectile *p=&g->projectiles[i]; if(!p->active)continue;
        p->step_cooldown_ms-=dt;
        if(p->step_cooldown_ms>0)continue;
        p->step_cooldown_ms=(p->kind==PROJ_LIGHTNING)?HOTT_LIGHTNING_STEP_MS:((p->kind==PROJ_FIREBALL)?HOTT_FIREBALL_STEP_MS:HOTT_ARROW_STEP_MS);
        if(--p->life<=0){p->active=false;continue;}

        p->prev_x=p->x; p->prev_y=p->y;
        if(p->kind==PROJ_FIREBALL) steer_fireball(g,p);
        int nx=p->x+p->dx,ny=p->y+p->dy;

        if(p->kind==PROJ_LIGHTNING && projectile_blocked(f,nx,ny)){
            bool block_x=projectile_blocked(f,p->x+p->dx,p->y);
            bool block_y=projectile_blocked(f,p->x,p->y+p->dy);
            if(p->dx!=0 && (block_x || p->dy==0)) p->dx=-p->dx;
            if(p->dy!=0 && (block_y || p->dx==0)) p->dy=-p->dy;
            nx=p->x+p->dx; ny=p->y+p->dy;
            if(projectile_blocked(f,nx,ny)){
                p->dx=-p->dx; p->dy=-p->dy;
                nx=p->x+p->dx; ny=p->y+p->dy;
            }
            if(projectile_blocked(f,nx,ny)){p->active=false;continue;}
        } else if(projectile_blocked(f,nx,ny)){
            p->active=false;continue;
        }

        Monster *m=monster_at(g,nx,ny);
        if(m){hurt_monster(g,m,p->damage);p->active=false;continue;}
        p->x=nx;p->y=ny;
    }
}

static bool monster_can_step(Game *g,Monster *self,int x,int y){
    if(!inside(x,y))return false;
    Tile t=g->floors[g->floor].tiles[y][x];
    if(t==TILE_DOOR_CLOSED){if(rr(g,8)==0)g->floors[g->floor].tiles[y][x]=TILE_DOOR_OPEN;return false;}
    if(!game_tile_walkable(t)||is_item(t)||t==TILE_GOLD_KEY)return false;
    for(int i=0;i<HOTT_MAX_MONSTERS;++i){Monster *m=&g->floors[g->floor].monsters[i];if(m!=self&&m->active&&m->x==x&&m->y==y)return false;}
    return true;
}

static void update_monsters(Game *g,int dt){
    FloorState *f=&g->floors[g->floor];
    for(int i=0;i<HOTT_MAX_MONSTERS;++i){
        Monster *m=&f->monsters[i];if(!m->active)continue;
        if(m->attack_cooldown_ms>0)m->attack_cooldown_ms-=dt;
        m->move_cooldown_ms-=dt;if(m->move_cooldown_ms>0)continue;
        m->move_cooldown_ms=120+rr(g,180);
        int md=abs(m->x-g->player_x)+abs(m->y-g->player_y);
        if(md==1){if(m->attack_cooldown_ms<=0){hurt_player(g,6+rr(g,10));m->attack_cooldown_ms=500;}continue;}
        int choices[4],n=0;
        int hx=(g->player_x>m->x)?DIR_RIGHT:(g->player_x<m->x?DIR_LEFT:-1);
        int hy=(g->player_y>m->y)?DIR_DOWN:(g->player_y<m->y?DIR_UP:-1);
        if(md<18){if(abs(g->player_x-m->x)>abs(g->player_y-m->y)){if(hx>=0)choices[n++]=hx;if(hy>=0)choices[n++]=hy;}
                   else{if(hy>=0)choices[n++]=hy;if(hx>=0)choices[n++]=hx;}}
        for(int d=0;d<4;++d)choices[n++]=rr(g,4);
        for(int c=0;c<n;++c){int d=choices[c],nx=m->x+DX[d],ny=m->y+DY[d];
            if(nx==g->player_x&&ny==g->player_y){if(m->attack_cooldown_ms<=0){hurt_player(g,6+rr(g,10));m->attack_cooldown_ms=500;}break;}
            if(monster_can_step(g,m,nx,ny)){m->x=nx;m->y=ny;break;}}
    }
}

static void auto_sword(Game *g){
    if(g->sword_cooldown_ms>0)return;
    int x,y;front_cell(g,&x,&y);if(monster_at(g,x,y))game_sword(g);
}

void game_update(Game *g,int dt_ms){
    if(dt_ms<0) dt_ms=0;
    if(dt_ms>100) dt_ms=100;
    if(g->message_ms>0){g->message_ms-=dt_ms;if(g->message_ms<0)g->message_ms=0;}
    if(g->sword_cooldown_ms>0)g->sword_cooldown_ms-=dt_ms;
    if(g->dead||g->won)return;
    auto_pickup_current(g);
    if(g->won)return;
    auto_sword(g);
    update_projectiles(g,dt_ms);
    update_monsters(g,dt_ms);
}

int game_debug_count_tile(const Game *g,Tile t){
    int n=0;for(int f=0;f<HOTT_FLOORS;++f)for(int y=0;y<HOTT_MAP_H;++y)for(int x=0;x<HOTT_MAP_W;++x)if(g->floors[f].tiles[y][x]==t)n++;return n;
}
