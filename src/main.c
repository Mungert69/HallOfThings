#include <hall_of_things/game.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sprites.h"

/* High-resolution logical canvas. SDL scales this to the actual window. */
#define LOGICAL_W 1280
#define LOGICAL_H 720
#define WINDOW_W 1280
#define WINDOW_H 720

/*
 * Exact ZX Spectrum world field of view: 32 x 24 character cells.
 * The original machine displayed 256 x 192 pixels, i.e. 32 x 24 cells at
 * 8 x 8 pixels each.  We keep exactly that amount of world visible while
 * drawing each logical cell at high resolution and retaining smooth camera
 * interpolation.  The compact HUD lives outside the clipped playfield.
 */
#define TILE_PX 28
#define HUD_H 30
#define VIEW_W 32
#define VIEW_H 24
#define VIEW_PX_W (VIEW_W * TILE_PX)
#define VIEW_PX_H (VIEW_H * TILE_PX)
#define VIEW_X ((LOGICAL_W - VIEW_PX_W) / 2)
#define VIEW_Y (HUD_H + ((LOGICAL_H - HUD_H - VIEW_PX_H) / 2))
#define MOVE_STEP_MS 90

static const int VIEW_DX[8] = {0, 1, 0, -1, -1, 1, 1, -1};
static const int VIEW_DY[8] = {-1, 0, 1, 0, -1, -1, 1, 1};

typedef struct {
    float x, y;
    float from_x, from_y;
    float to_x, to_y;
    int elapsed_ms;
    int duration_ms;
    int floor;
    bool active;
} PlayerMotion;

static void player_motion_snap(PlayerMotion *m,const Game *g){
    m->x=(float)g->player_x; m->y=(float)g->player_y;
    m->from_x=m->to_x=m->x; m->from_y=m->to_y=m->y;
    m->elapsed_ms=0; m->duration_ms=MOVE_STEP_MS;
    m->floor=g->floor; m->active=false;
}

static void player_motion_begin(PlayerMotion *m,const Game *g,int old_x,int old_y,int old_floor){
    if(g->floor!=old_floor){ player_motion_snap(m,g); return; }
    if(g->player_x==old_x && g->player_y==old_y) return;
    m->from_x=m->x; m->from_y=m->y;
    m->to_x=(float)g->player_x; m->to_y=(float)g->player_y;
    m->elapsed_ms=0; m->duration_ms=MOVE_STEP_MS;
    m->floor=g->floor; m->active=true;
}

static void player_motion_update(PlayerMotion *m,const Game *g,int dt_ms){
    if(g->floor!=m->floor){ player_motion_snap(m,g); return; }
    if(!m->active){
        m->x=(float)g->player_x; m->y=(float)g->player_y;
        return;
    }
    m->elapsed_ms+=dt_ms;
    if(m->elapsed_ms>=m->duration_ms){
        m->x=m->to_x; m->y=m->to_y; m->active=false;
        return;
    }
    float t=(float)m->elapsed_ms/(float)m->duration_ms;
    m->x=m->from_x+(m->to_x-m->from_x)*t;
    m->y=m->from_y+(m->to_y-m->from_y)*t;
}

static int screen_tile_x(float map_x,float cam_x){
    return VIEW_X+(int)((map_x-cam_x)*(float)TILE_PX);
}

static int screen_tile_y(float map_y,float cam_y){
    return VIEW_Y+(int)((map_y-cam_y)*(float)TILE_PX);
}

typedef struct { char c; unsigned char r[7]; } Glyph;
static const Glyph FONT[] = {
{' ',{0,0,0,0,0,0,0}},{'!',{4,4,4,4,4,0,4}},{'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,6,6}},
{'/',{1,2,4,8,16,0,0}},{':',{0,6,6,0,6,6,0}},{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
{'6',{6,8,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,2,12}},
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
{'I',{14,4,4,4,4,4,14}},{'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}}
};

static const unsigned char *glyph(char c){
    if(c>='a' && c<='z') c=(char)(c-'a'+'A');
    for(size_t i=0;i<sizeof(FONT)/sizeof(FONT[0]);++i) if(FONT[i].c==c) return FONT[i].r;
    return FONT[0].r;
}

static void text(SDL_Renderer*r,int x,int y,int scale,const char*s,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    for(;*s;++s){
        const unsigned char*g=glyph(*s);
        for(int yy=0;yy<7;++yy) for(int xx=0;xx<5;++xx) if(g[yy]&(1<<(4-xx))){
            SDL_Rect q={x+xx*scale,y+yy*scale,scale,scale};
            SDL_RenderFillRect(r,&q);
        }
        x+=6*scale;
    }
}

static void rect(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect q={x,y,w,h};
    SDL_RenderFillRect(r,&q);
}

static void outline(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect q={x,y,w,h};
    SDL_RenderDrawRect(r,&q);
}

static void pixel(SDL_Renderer*r,int x,int y,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_RenderDrawPoint(r,x,y);
}

static void line(SDL_Renderer*r,int x1,int y1,int x2,int y2,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_RenderDrawLine(r,x1,y1,x2,y2);
}

static void thick_line(SDL_Renderer*r,int x1,int y1,int x2,int y2,int thickness,SDL_Color c){
    int half=thickness/2;
    if(y1==y2){
        for(int o=-half;o<=half;++o) line(r,x1,y1+o,x2,y2+o,c);
    }else if(x1==x2){
        for(int o=-half;o<=half;++o) line(r,x1+o,y1,x2+o,y2,c);
    }else{
        for(int o=-half;o<=half;++o) line(r,x1+o,y1,x2+o,y2,c);
    }
}

static bool map_in_bounds(int x,int y){ return x>=0 && y>=0 && x<HOTT_MAP_W && y<HOTT_MAP_H; }

static Tile map_tile(const Game*g,int x,int y){
    if(!map_in_bounds(x,y)) return TILE_WALL;
    return g->floors[g->floor].tiles[y][x];
}

static bool is_wall_tile(Tile t){ return t==TILE_WALL; }

/* Draw a wall tile as a centre-line only. The black background remains usable-looking
   everywhere; connected wall cells become one clean line rather than a filled strip. */
static void draw_wall_cell(SDL_Renderer*r,const Game*g,int px,int py,int mx,int my){
    SDL_Color green={0,245,32,255};
    const int cx=px+TILE_PX/2;
    const int cy=py+TILE_PX/2;
    bool l=is_wall_tile(map_tile(g,mx-1,my));
    bool rr=is_wall_tile(map_tile(g,mx+1,my));
    bool u=is_wall_tile(map_tile(g,mx,my-1));
    bool d=is_wall_tile(map_tile(g,mx,my+1));

    if(l)  thick_line(r,px,cy,cx,cy,2,green);
    if(rr) thick_line(r,cx,cy,px+TILE_PX-1,cy,2,green);
    if(u)  thick_line(r,cx,py,cx,cy,2,green);
    if(d)  thick_line(r,cx,cy,cx,py+TILE_PX-1,2,green);
    if(!l && !rr && !u && !d) rect(r,cx-1,cy-1,3,3,green);
}

static void draw_ring(SDL_Renderer*r,int cx,int cy,SDL_Color c){
    line(r,cx-4,cy-3,cx+4,cy-3,c); line(r,cx-4,cy+3,cx+4,cy+3,c);
    line(r,cx-5,cy-2,cx-5,cy+2,c); line(r,cx+5,cy-2,cx+5,cy+2,c);
}

static void draw_potion(SDL_Renderer*r,int cx,int cy,SDL_Color c){
    rect(r,cx-2,cy-6,4,3,c); rect(r,cx-1,cy-3,2,2,c);
    line(r,cx-4,cy+3,cx+4,cy+3,c); line(r,cx-4,cy+2,cx-2,cy-2,c);
    line(r,cx+4,cy+2,cx+2,cy-2,c); rect(r,cx-3,cy+1,7,3,c);
}

static void draw_arrow_icon(SDL_Renderer*r,int cx,int cy,SDL_Color c){
    line(r,cx-6,cy+4,cx+5,cy-5,c); line(r,cx+1,cy-5,cx+5,cy-5,c); line(r,cx+5,cy-5,cx+5,cy-1,c);
}

static void draw_heart(SDL_Renderer*r,int x,int y,bool full){
    SDL_Color red={255,28,40,255}, dim={100,35,40,255};
    SDL_Color c=full?red:dim;
    rect(r,x+1,y,3,2,c); rect(r,x+6,y,3,2,c);
    rect(r,x,y+2,10,4,c); rect(r,x+1,y+6,8,2,c); rect(r,x+3,y+8,4,2,c);
    if(!full){ rect(r,x+2,y+2,6,3,(SDL_Color){0,0,0,255}); }
}

static void draw_hud(SDL_Renderer*r,const Game*g){
    SDL_Color white={235,235,235,255};
    SDL_Color yellow={255,240,0,255};
    SDL_Color cyan={0,235,255,255};
    char b[32];

    rect(r,0,0,LOGICAL_W,HUD_H,(SDL_Color){0,0,0,255});

    int x=14;
    draw_ring(r,x+6,14,yellow); x+=18;
    snprintf(b,sizeof(b),"%d/7",g->rings); text(r,x,7,2,b,white); x+=64;

    draw_potion(r,x+7,14,cyan); x+=20;
    snprintf(b,sizeof(b),"%d",g->magic); text(r,x,7,2,b,white); x+=74;

    draw_arrow_icon(r,x+7,14,white); x+=20;
    snprintf(b,sizeof(b),"%d",g->arrows); text(r,x,7,2,b,white); x+=74;

    int health=127-g->wounds;
    int hearts=(health<=0)?0:((health+31)/32);
    if(hearts>4) hearts=4;
    for(int i=0;i<4;++i) draw_heart(r,x+i*15,8,i<hearts);

    snprintf(b,sizeof(b),"F%d",g->floor); text(r,LOGICAL_W-205,7,2,b,white);
    snprintf(b,sizeof(b),"%06d",g->score); text(r,LOGICAL_W-116,7,2,b,white);
}

static void draw_item(SDL_Renderer*r,Tile t,int px,int py){
    int x=px+(TILE_PX-8)/2, y=py+(TILE_PX-8)/2;
    SDL_Color yellow={255,240,0,255};
    SDL_Color cyan={0,235,255,255};
    SDL_Color white={240,240,240,255};
    SDL_Color magenta={235,80,255,255};

    const uint8_t *bitmap=NULL;
    SDL_Color c=white;
    if(t>=TILE_RING_1 && t<=TILE_RING_7){ bitmap=HOTT_RING_BITMAP; c=yellow; }
    else if(t==TILE_GOLD_KEY){ bitmap=HOTT_KEY_BITMAP; c=yellow; }
    else if(t==TILE_MAGIC){ bitmap=HOTT_MAGIC_BITMAP; c=cyan; }
    else if(t==TILE_ARROWS){ bitmap=HOTT_ARROW_BITMAP; c=white; }
    else if(t==TILE_TREASURE){ bitmap=HOTT_TREASURE_BITMAP; c=yellow; }
    else if(t==TILE_STAIRS_UP){ bitmap=HOTT_STAIRS_UP_BITMAP; c=magenta; }
    else if(t==TILE_STAIRS_DOWN){ bitmap=HOTT_STAIRS_DOWN_BITMAP; c=magenta; }

    if(bitmap){
        SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
        for(int yy=0; yy<8; ++yy){
            uint8_t bits=bitmap[yy];
            for(int xx=0; xx<8; ++xx)
                if(bits & (uint8_t)(0x80u >> xx)) SDL_RenderDrawPoint(r,x+xx,y+yy);
        }
    } else if(t==TILE_GATE_CLOSED){
        int cx=px+TILE_PX/2, cy=py+TILE_PX/2;
        for(int xx=cx-7;xx<=cx+7;xx+=5) thick_line(r,xx,cy-10,xx,cy+10,2,white);
    }
}

static void draw_bitmap(SDL_Renderer*r,int x,int y,const uint8_t *rows,int row_count,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    for(int yy=0;yy<row_count;++yy){
        uint8_t bits=rows[yy];
        for(int xx=0;xx<8;++xx){
            if(bits & (uint8_t)(0x80u >> xx)) SDL_RenderDrawPoint(r,x+xx,y+yy);
        }
    }
}

static void draw_player(SDL_Renderer*r,int px,int py,Direction facing){
    SDL_Color white={245,245,245,255};
    SDL_Color cyan={0,220,255,255};
    int x=px+(TILE_PX-8)/2;
    int y=py+(TILE_PX-16)/2;

    draw_bitmap(r,x,y,HOTT_PLAYER_BITMAP[(int)facing],16,white);

    /* A one-pixel aiming pip preserves the eight-way extension while keeping
       the sprite itself at the original 8x16 Spectrum footprint. */
    int cx=px+TILE_PX/2,cy=py+TILE_PX/2;
    int dx=VIEW_DX[facing],dy=VIEW_DY[facing];
    rect(r,cx+dx*10-1,cy+dy*10-1,3,3,cyan);
}

static void draw_sword(SDL_Renderer*r,int px,int py,Direction facing,int cooldown_ms){
    if(cooldown_ms<=0) return;
    static const int circle_dx[8]={0,1,1,1,0,-1,-1,-1};
    static const int circle_dy[8]={-1,-1,0,1,1,1,0,-1};
    static const int facing_to_circle[8]={0,2,4,6,7,1,3,5};
    SDL_Color white={245,245,245,255};
    SDL_Color cyan={0,220,255,255};
    int elapsed=180-cooldown_ms;
    if(elapsed<0) elapsed=0;
    if(elapsed>179) elapsed=179;
    int phase=(elapsed*5)/180;          /* five positions through the swing */
    int ci=(facing_to_circle[(int)facing]-2+phase+8)%8;
    int dx=circle_dx[ci],dy=circle_dy[ci];
    int cx=px+TILE_PX/2,cy=py+TILE_PX/2;
    int hx=cx+dx*7,hy=cy+dy*7;
    int tx=cx+dx*18,ty=cy+dy*18;
    thick_line(r,hx,hy,tx,ty,2,white);
    rect(r,tx-1,ty-1,3,3,cyan);
}

static void draw_monster(SDL_Renderer*r,const Monster*m,int px,int py){
    SDL_Color white={245,245,245,255};
    SDL_Color red={255,35,45,255};
    SDL_Color c=(m->move_type==3)?red:white;
    int x=px+(TILE_PX-8)/2;
    int y=py+(TILE_PX-8)/2;
    int frame=(m->x+m->y+(m->move_cooldown_ms/120))&1;
    int variant=(m->move_type*2+frame)&7;

    draw_bitmap(r,x,y,HOTT_THING_BITMAP[variant],8,c);
}

static void draw_lightning_bolt(SDL_Renderer*r,int cx,int cy,int dx,int dy){
    SDL_Color cyan={0,240,255,255};
    int px=-dy,py=dx;
    line(r,cx-dx*6,cy-dy*6,cx-dx*1+px*3,cy-dy*1+py*3,cyan);
    line(r,cx-dx*1+px*3,cy-dy*1+py*3,cx+dx*2-px*3,cy+dy*2-py*3,cyan);
    line(r,cx+dx*2-px*3,cy+dy*2-py*3,cx+dx*7,cy+dy*7,cyan);
}

static void draw_projectile(SDL_Renderer*r,const Projectile*p,int px,int py,int prev_px,int prev_py){
    SDL_Color white={245,245,245,255};
    SDL_Color yellow={255,230,0,255};
    SDL_Color red={255,50,35,255};
    SDL_Color cyan={0,240,255,255};
    int cx=px+TILE_PX/2,cy=py+TILE_PX/2;

    if(p->kind==PROJ_ARROW){
        line(r,cx-p->dx*7,cy-p->dy*7,cx+p->dx*7,cy+p->dy*7,white);
        int pxv=-p->dy,pyv=p->dx;
        line(r,cx+p->dx*7,cy+p->dy*7,cx+p->dx*3+pxv*3,cy+p->dy*3+pyv*3,white);
        line(r,cx+p->dx*7,cy+p->dy*7,cx+p->dx*3-pxv*3,cy+p->dy*3-pyv*3,white);
    }else if(p->kind==PROJ_FIREBALL){
        outline(r,cx-4,cy-4,9,9,yellow);
        pixel(r,cx,cy,red); pixel(r,cx-6,cy,red); pixel(r,cx+6,cy,red); pixel(r,cx,cy-6,red); pixel(r,cx,cy+6,red);
    }else{
        /* A short dotted trail makes the diagonal/reflection movement readable. */
        int x1=prev_px+TILE_PX/2,y1=prev_py+TILE_PX/2;
        for(int i=1;i<=5;++i){
            int tx=x1+(cx-x1)*i/6,ty=y1+(cy-y1)*i/6;
            rect(r,tx-1,ty-1,3,3,cyan);
        }
        draw_lightning_bolt(r,cx,cy,p->dx,p->dy);
    }
}

static void draw_status_overlay(SDL_Renderer*r,const Game*g){
    SDL_Color green={0,245,32,255};
    SDL_Color white={235,235,235,255};
    SDL_Color yellow={255,240,0,255};
    char b[64];

    const int w=330,h=220,x=(LOGICAL_W-w)/2,y=(LOGICAL_H-h)/2;
    rect(r,x,y,w,h,(SDL_Color){0,0,0,245});
    outline(r,x,y,w,h,green);
    text(r,x+20,y+18,2,"STATUS",yellow);
    snprintf(b,sizeof(b),"FLOOR %d   RINGS %d/7",g->floor,g->rings); text(r,x+20,y+52,2,b,white);
    snprintf(b,sizeof(b),"MAGIC %d   ARROWS %d",g->magic,g->arrows); text(r,x+20,y+78,2,b,white);
    snprintf(b,sizeof(b),"WOUNDS %d   SCORE %d",g->wounds,g->score); text(r,x+20,y+104,2,b,white);
    text(r,x+20,y+140,1,"ARROW KEYS MOVE/AIM - HOLD TWO FOR DIAGONAL",white);
    text(r,x+20,y+156,1,"SPACE SWORD  A ARROW  F FIREBALL  L LIGHT",white);
    text(r,x+20,y+172,1,"E HEAL   H KEYS   C CHEAT   D DROP   R REFILL",white);
    text(r,x+20,y+192,1,"PRESS 1 TO CLOSE",green);
}


static int find_gold_key_floor(const Game *g){
    if(g->has_key) return -2;
    for(int fl=0; fl<HOTT_FLOORS; ++fl){
        for(int y=0; y<HOTT_MAP_H; ++y){
            for(int x=0; x<HOTT_MAP_W; ++x){
                if(g->floors[fl].tiles[y][x] == TILE_GOLD_KEY) return fl;
            }
        }
    }
    return -1;
}

static int count_rings_on_floor(const Game *g,int floor){
    int count=0;
    for(int y=0; y<HOTT_MAP_H; ++y){
        for(int x=0; x<HOTT_MAP_W; ++x){
            Tile t=g->floors[floor].tiles[y][x];
            if(t>=TILE_RING_1 && t<=TILE_RING_7) ++count;
        }
    }
    return count;
}

static void draw_help_overlay(SDL_Renderer*r,const Game*g){
    (void)g;
    SDL_Color green={0,245,32,255};
    SDL_Color white={235,235,235,255};
    SDL_Color cyan={0,240,255,255};
    const int w=430,h=278,x=(LOGICAL_W-w)/2,y=(LOGICAL_H-h)/2;
    rect(r,x,y,w,h,(SDL_Color){0,0,0,248});
    outline(r,x,y,w,h,green);
    text(r,x+20,y+18,2,"CONTROLS",cyan);
    text(r,x+20,y+54,1,"ARROW KEYS   MOVE / AIM",white);
    text(r,x+20,y+72,1,"TWO ARROWS   DIAGONAL MOVE / AIM",white);
    text(r,x+20,y+90,1,"SPACE / S    SWORD",white);
    text(r,x+20,y+108,1,"A / ENTER    ARROW",white);
    text(r,x+20,y+126,1,"F            HOMING FIREBALL",white);
    text(r,x+20,y+144,1,"L            BOUNCING LIGHTNING",white);
    text(r,x+20,y+162,1,"E            HEAL",white);
    text(r,x+20,y+180,1,"D            DROP TREASURE",white);
    text(r,x+20,y+198,1,"R            TEST REFILL",white);
    text(r,x+20,y+216,1,"1            STATUS",white);
    text(r,x+20,y+234,1,"C            CHEAT / WIN HELP",white);
    text(r,x+20,y+254,1,"H TO CLOSE",green);
}

static void draw_cheat_overlay(SDL_Renderer*r,const Game*g){
    SDL_Color green={0,245,32,255};
    SDL_Color white={235,235,235,255};
    SDL_Color yellow={255,240,0,255};
    SDL_Color cyan={0,240,255,255};
    char b[96];
    int key_floor=find_gold_key_floor(g);
    const int w=500,h=360,x=(LOGICAL_W-w)/2,y=(LOGICAL_H-h)/2;
    rect(r,x,y,w,h,(SDL_Color){0,0,0,248});
    outline(r,x,y,w,h,green);
    text(r,x+20,y+18,2,"CHEAT",yellow);
    snprintf(b,sizeof(b),"RINGS COLLECTED %d/7",g->rings);
    text(r,x+20,y+56,1,b,white);

    text(r,x+20,y+82,1,"RINGS STILL ON EACH FLOOR",yellow);
    for(int fl=1; fl<HOTT_FLOORS; ++fl){
        int remaining=count_rings_on_floor(g,fl);
        snprintf(b,sizeof(b),"FLOOR %d    %d",fl,remaining);
        text(r,x+36,y+82+fl*18,1,b,remaining>0?cyan:white);
    }

    if(key_floor>=0){
        snprintf(b,sizeof(b),"GOLDEN KEY    FLOOR %d",key_floor);
        text(r,x+250,y+100,1,b,cyan);
    }else if(key_floor==-2){
        text(r,x+250,y+100,1,"GOLDEN KEY    COLLECTED",cyan);
    }else{
        text(r,x+250,y+100,1,"GOLDEN KEY    NOT FOUND",cyan);
    }

    text(r,x+250,y+136,1,"HOW TO WIN",yellow);
    text(r,x+250,y+158,1,"1 COLLECT ALL 7 RINGS",white);
    text(r,x+250,y+178,1,"2 RETURN TO FLOOR 0",white);
    text(r,x+250,y+198,1,"3 ENTER OPEN SANCTUARY",white);
    text(r,x+250,y+218,1,"4 TOUCH GOLDEN KEY",white);
    text(r,x+20,y+330,1,"C TO CLOSE",green);
}

static void render(SDL_Renderer*r,const Game*g,const PlayerMotion *motion){
    SDL_Color black={0,0,0,255};
    SDL_Color white={235,235,235,255};
    SDL_Color yellow={255,240,0,255};
    SDL_Color red={255,35,45,255};

    rect(r,0,0,LOGICAL_W,LOGICAL_H,black);
    draw_hud(r,g);

    /* Camera tracks the interpolated player position, not the integer grid cell. */
    float camx=motion->x-(float)VIEW_W/2.0f;
    float camy=motion->y-(float)VIEW_H/2.0f;
    float max_camx=(float)(HOTT_MAP_W-VIEW_W);
    float max_camy=(float)(HOTT_MAP_H-VIEW_H);
    if(camx<0.0f) camx=0.0f;
    if(camy<0.0f) camy=0.0f;
    if(camx>max_camx) camx=max_camx;
    if(camy>max_camy) camy=max_camy;

    int first_x=(int)camx;
    int first_y=(int)camy;
    SDL_Rect play_clip={VIEW_X,VIEW_Y,VIEW_PX_W,VIEW_PX_H};
    SDL_RenderSetClipRect(r,&play_clip);

    /* Draw an extra row/column because a fractional camera can expose them. */
    for(int iy=-1;iy<=VIEW_H+1;++iy){
        for(int ix=-1;ix<=VIEW_W+1;++ix){
            int mx=first_x+ix,my=first_y+iy;
            if(!map_in_bounds(mx,my)) continue;
            Tile t=map_tile(g,mx,my);
            if(t!=TILE_WALL) continue;
            draw_wall_cell(r,g,screen_tile_x((float)mx,camx),screen_tile_y((float)my,camy),mx,my);
        }
    }

    for(int iy=-1;iy<=VIEW_H+1;++iy){
        for(int ix=-1;ix<=VIEW_W+1;++ix){
            int mx=first_x+ix,my=first_y+iy;
            if(!map_in_bounds(mx,my)) continue;
            Tile t=map_tile(g,mx,my);
            if(t==TILE_FLOOR || t==TILE_WALL || t==TILE_DOOR_OPEN || t==TILE_DOOR_CLOSED) continue;
            draw_item(r,t,screen_tile_x((float)mx,camx),screen_tile_y((float)my,camy));
        }
    }

    for(int i=0;i<HOTT_MAX_MONSTERS;++i){
        const Monster*m=&g->floors[g->floor].monsters[i];
        if(!m->active) continue;

        int interval=m->move_interval_ms>0?m->move_interval_ms:1;
        float alpha=1.0f-(float)m->move_cooldown_ms/(float)interval;
        if(alpha<0.0f) alpha=0.0f;
        if(alpha>1.0f) alpha=1.0f;
        float rx=(float)m->prev_x+((float)m->x-(float)m->prev_x)*alpha;
        float ry=(float)m->prev_y+((float)m->y-(float)m->prev_y)*alpha;

        float sx=rx-camx,sy=ry-camy;
        if(sx<-1.0f || sy<-1.0f || sx>(float)VIEW_W+1.0f || sy>(float)VIEW_H+1.0f) continue;
        draw_monster(r,m,screen_tile_x(rx,camx),screen_tile_y(ry,camy));
    }

    for(int i=0;i<HOTT_MAX_PROJECTILES;++i){
        const Projectile*p=&g->projectiles[i];
        if(!p->active) continue;

        int interval=(p->kind==PROJ_LIGHTNING)?HOTT_LIGHTNING_STEP_MS:((p->kind==PROJ_FIREBALL)?HOTT_FIREBALL_STEP_MS:HOTT_ARROW_STEP_MS);
        float alpha=1.0f-(float)p->step_cooldown_ms/(float)interval;
        if(alpha<0.0f) alpha=0.0f;
        if(alpha>1.0f) alpha=1.0f;
        float rx=(float)p->prev_x+((float)p->x-(float)p->prev_x)*alpha;
        float ry=(float)p->prev_y+((float)p->y-(float)p->prev_y)*alpha;

        float sx=rx-camx,sy=ry-camy;
        if(sx<-1.0f || sy<-1.0f || sx>(float)VIEW_W+1.0f || sy>(float)VIEW_H+1.0f) continue;
        int px=screen_tile_x(rx,camx),py=screen_tile_y(ry,camy);
        int prev_px=screen_tile_x((float)p->prev_x,camx),prev_py=screen_tile_y((float)p->prev_y,camy);
        draw_projectile(r,p,px,py,prev_px,prev_py);
    }

    int player_px=screen_tile_x(motion->x,camx);
    int player_py=screen_tile_y(motion->y,camy);
    draw_player(r,player_px,player_py,g->facing);
    draw_sword(r,player_px,player_py,g->facing,g->sword_cooldown_ms);
    SDL_RenderSetClipRect(r,NULL);

    if(g->message_ms>0){
        int tw=(int)strlen(g->message)*6;
        int x=(LOGICAL_W-tw)/2;
        rect(r,x-8,LOGICAL_H-18,tw+16,16,(SDL_Color){0,0,0,220});
        text(r,x,LOGICAL_H-15,1,g->message,g->won?yellow:(g->dead?red:white));
    }

    if(g->show_status) draw_status_overlay(r,g);
    if(g->show_help) draw_help_overlay(r,g);
    if(g->show_cheat) draw_cheat_overlay(r,g);

    if(g->dead || g->won){
        const char*msg=g->won?"YOU ARE VICTORIOUS!":"YOU HAVE BEEN KILLED";
        int w=(int)strlen(msg)*12+36;
        int x=(LOGICAL_W-w)/2;
        rect(r,x,LOGICAL_H/2-28,w,56,(SDL_Color){0,0,0,245});
        outline(r,x,LOGICAL_H/2-28,w,56,g->won?yellow:red);
        text(r,x+18,LOGICAL_H/2-8,2,msg,g->won?yellow:red);
    }
}

static void handle_key(Game*g,SDL_Keycode k){
    switch(k){
        case SDLK_k: game_keep(g); break;
        case SDLK_d: game_drop(g); break;
        case SDLK_s: case SDLK_SPACE: game_sword(g); break;
        case SDLK_a: case SDLK_RETURN: game_fire_arrow(g); break;
        case SDLK_f: game_fireball(g); break;
        case SDLK_l: game_lightning(g); break;
        case SDLK_h:
            g->show_help=!g->show_help;
            if(g->show_help){ g->show_cheat=false; g->show_status=false; }
            break;
        case SDLK_c:
            g->show_cheat=!g->show_cheat;
            if(g->show_cheat){ g->show_help=false; g->show_status=false; }
            break;
        case SDLK_e: game_heal(g); break;
        case SDLK_r: game_test_refill(g); break;
        case SDLK_1:
            game_toggle_status(g);
            if(g->show_status){ g->show_help=false; g->show_cheat=false; }
            break;
        default: break;
    }
}

static bool held_direction(const Uint8 *keys,Direction *dir){
    int x=(keys[SDL_SCANCODE_RIGHT]?1:0)-(keys[SDL_SCANCODE_LEFT]?1:0);
    int y=(keys[SDL_SCANCODE_DOWN]?1:0)-(keys[SDL_SCANCODE_UP]?1:0);
    if(x==0 && y==0) return false;
    if(x==0) *dir=(y<0)?DIR_UP:DIR_DOWN;
    else if(y==0) *dir=(x<0)?DIR_LEFT:DIR_RIGHT;
    else if(x<0 && y<0) *dir=DIR_UP_LEFT;
    else if(x>0 && y<0) *dir=DIR_UP_RIGHT;
    else if(x>0 && y>0) *dir=DIR_DOWN_RIGHT;
    else *dir=DIR_DOWN_LEFT;
    return true;
}

int main(int argc,char**argv){
    uint32_t seed=(uint32_t)time(NULL);
    int start_floor=1; /* Experimental maze build: start where the room maze is visible. */
    if(argc>1) seed=(uint32_t)strtoul(argv[1],NULL,0);
    if(argc>2){
        start_floor=atoi(argv[2]);
        if(start_floor<0 || start_floor>=HOTT_FLOORS){
            fprintf(stderr,"start floor must be 0..7\n");
            return 2;
        }
    }

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){
        fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());
        return 1;
    }

    SDL_Window*w=SDL_CreateWindow("Halls of the Things",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        WINDOW_W,WINDOW_H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!w){
        fprintf(stderr,"window: %s\n",SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer*r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!r) r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
    if(!r){
        fprintf(stderr,"renderer: %s\n",SDL_GetError());
        SDL_DestroyWindow(w); SDL_Quit(); return 1;
    }

    SDL_RenderSetLogicalSize(r,LOGICAL_W,LOGICAL_H);

    Game g;
    game_init(&g,seed);

    /* Floor 0 is intentionally the original special open-plan level.  The
       experimental build defaults to floor 1 so the 16x16 room topology can
       be inspected immediately.  A second CLI argument selects any floor. */
    g.floor=start_floor;
    g.player_x=4;
    g.player_y=(start_floor==0)?4:5; /* floor 1..7: start beside the down stair */
    g.facing=DIR_RIGHT;
    memset(g.projectiles,0,sizeof(g.projectiles));

    PlayerMotion motion;
    player_motion_snap(&motion,&g);
    Uint32 prev=SDL_GetTicks();
    bool running=true;
    int move_repeat_ms=0;
    int last_move_dir=-1;

    while(running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=false;
            else if(e.type==SDL_KEYDOWN && !e.key.repeat){
                if(e.key.keysym.sym==SDLK_ESCAPE) running=false;
                else handle_key(&g,e.key.keysym.sym);
            }
        }

        Uint32 now=SDL_GetTicks();
        int dt=(int)(now-prev); prev=now;
        if(dt>100) dt=100;
        move_repeat_ms-=dt;
        player_motion_update(&motion,&g,dt);

        Direction move_dir;
        const Uint8 *keys=SDL_GetKeyboardState(NULL);
        if(held_direction(keys,&move_dir)){
            if(!motion.active && (last_move_dir!=(int)move_dir || move_repeat_ms<=0)){
                int old_x=g.player_x,old_y=g.player_y,old_floor=g.floor;
                game_move(&g,move_dir);
                player_motion_begin(&motion,&g,old_x,old_y,old_floor);
                last_move_dir=(int)move_dir;
                move_repeat_ms=MOVE_STEP_MS;
            }
        } else {
            last_move_dir=-1;
            move_repeat_ms=0;
        }

        game_update(&g,dt);
        render(r,&g,&motion);
        SDL_RenderPresent(r);
    }

    SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
    return 0;
}
