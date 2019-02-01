#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "text_renderer.h"

typedef enum e_GameState {
    NotStarted,
    Playing,
    Paused,
    GameOver
} GameState;

typedef enum e_FieldType { /* indicates what is inside game board field */
    Empty,
    Snake,
    Food,
    Wall
} FieldType;

typedef struct BoardField {
    FieldType type; /* type of this field */
    int p; /* paremeter(kind of character in snake body, kind of food, kind of wall ) */
    int ndx; /* delta x to next piece of snake. */
    int ndy; /* delta y to next piece of snake. */
    int pdx; /* delta x to previous piece of snake. */
    int pdy; /* delta y to previous piece of snake. */
} BoardField;

#define SCR_W 1024
#define SCR_H 768
#define TILE_SIZE 16
#define GAME_SPEED 200
#define GROUND_TILES 8
#define WALL_TILES 4
#define FOOD_TILES 6
#define TOTAL_CHAR 12
#define CHAR_ANIM_FRAMES 4
SDL_Window *screen = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *txt_game_board = NULL;
SDL_Surface *srf_env_tileset = NULL;
SDL_Texture *txt_env_tileset = NULL;
SDL_Surface *srf_food_tileset = NULL;
SDL_Texture *txt_food_tileset = NULL;
SDL_Surface *srf_char_tileset = NULL;
SDL_Texture *txt_char_tileset = NULL;
SDL_Surface *srf_font = NULL;
SDL_Texture *txt_font = NULL;
SDL_TimerID game_timer = 0;

SDL_Rect *ground_tile = NULL;
SDL_Rect *wall_tile = NULL;
SDL_Rect *food_tile = NULL;
BoardField *game_board = NULL;
GameState game_state = NotStarted;

int game_board_w = (SCR_W/TILE_SIZE); /* game board width(in full(16 pixels) tiles)*/
int game_board_h = (SCR_H/TILE_SIZE-1); /* game board width(in full(16 pixels) tiles)*/
int dhx=0, dhy=0;     /* head movement direction */
int hx=0, hy=0;       /* head coordinates */
int tx=0, ty=0;       /* tail coordinates */
int score=0;          /* player's score, number of food eaten */
int hi_score=0;       /* highest score from previous gameplays */
int expand_counter=0; /* counter of snake segments to add and walls to seed */
int ck_press=0;       /* control key pressed indicator */
int frame=0;          /* animation frame */


int allocate_arrays(void) {
    game_board = calloc(game_board_w*game_board_h, sizeof(BoardField));
    ground_tile = calloc(GROUND_TILES, sizeof(SDL_Rect));
    wall_tile = calloc(WALL_TILES, sizeof(SDL_Rect));
    food_tile = calloc(FOOD_TILES, sizeof(SDL_Rect));
    return 0;
}

void free_arrays(void) {
    free(food_tile);
    free(wall_tile);
    free(ground_tile);
    free(game_board);
}

void init_tiles(void) {
    ground_tile[0].x = 0;   ground_tile[0].y = 0;
    ground_tile[1].x = 1;   ground_tile[1].y = 0;
    ground_tile[2].x = 2;   ground_tile[2].y = 0;
    ground_tile[3].x = 3;   ground_tile[3].y = 0;
    ground_tile[4].x = 4;   ground_tile[4].y = 0;
    ground_tile[5].x = 0;   ground_tile[5].y = 1;
    ground_tile[6].x = 0;   ground_tile[6].y = 2;
    ground_tile[7].x = 0;   ground_tile[7].y = 3;
    for (int i=0; i<GROUND_TILES; i++) {
        ground_tile[i].x *= TILE_SIZE;   ground_tile[i].y *= TILE_SIZE;
        ground_tile[i].w = ground_tile[i].h = TILE_SIZE;
    }

    wall_tile[0].x = 0;  wall_tile[0].y = 17;
    wall_tile[1].x = 1;  wall_tile[1].y = 17;
    wall_tile[2].x = 2;  wall_tile[2].y = 17;
    wall_tile[3].x = 3;  wall_tile[3].y = 17;
    for (int i=0; i<WALL_TILES; i++) {
        wall_tile[i].x *= TILE_SIZE;   wall_tile[i].y *= TILE_SIZE;
        wall_tile[i].w = wall_tile[i].h = TILE_SIZE;
    }

    food_tile[0].x = 4;  food_tile[0].y = 1;
    food_tile[1].x = 0;  food_tile[1].y = 4;
    food_tile[2].x = 1;  food_tile[2].y = 4;
    food_tile[3].x = 2;  food_tile[3].y = 4;
    food_tile[4].x = 0;  food_tile[4].y = 6;
    food_tile[5].x = 3;  food_tile[5].y = 6;
    for (int i=0; i<FOOD_TILES; i++) {
        food_tile[i].x *= TILE_SIZE;   food_tile[i].y *= TILE_SIZE;
        food_tile[i].w = food_tile[i].h = TILE_SIZE;
    }
}

/* Clears game board and fills the background texture with random grass pattern.
   Executed at the start of the game.
 */
void init_game_board(void) {
    /* clear the game board array */
    for (int i=0; i<game_board_w*game_board_h; i++) {
        game_board[i].type = Empty;
        game_board[i].ndx = 0;
        game_board[i].ndy = 0;
        game_board[i].pdx = 0;
        game_board[i].pdy = 0;
    }

    /* Redirect rendering to the game board texture */
    SDL_SetRenderTarget(renderer, txt_game_board);
    /* Fill render target with black. Later status bar at the bottom of the screen
       will have that color. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Fill background with random pattern of ground tiles */
    int x, y;
    SDL_Rect DstR = { 0, 0, TILE_SIZE, TILE_SIZE };
    for (y=0; y<game_board_h; y++)
        for (x=0; x<game_board_w; x++) {
            DstR.x = x*TILE_SIZE; DstR.y = y*TILE_SIZE;
            SDL_RenderCopy(renderer, txt_env_tileset, &ground_tile[rand()%GROUND_TILES], &DstR);
        }

    /* Detach the game board texture from renderer */
    SDL_SetRenderTarget(renderer, NULL);
}

/* Adds obstacle or food at random location in game board and renders it to background texture. */
void seed_item(FieldType item_type)
{
    int x = 0, y = 0; /* coordinates */
    while(1) {
        x = rand() % game_board_w;
        y = rand() % game_board_h;
        if (game_board[game_board_w*y+x].type == Empty) {
            game_board[game_board_w*y+x].type = item_type;
            /* render seeded wall to game board texture */
            if (item_type == Wall) {
                SDL_Rect DstR = { x*TILE_SIZE, y*TILE_SIZE, TILE_SIZE, TILE_SIZE };
                /* Redirect rendering to the game board texture */
                SDL_SetRenderTarget(renderer, txt_game_board);
                /* Copy wall tile */
                SDL_RenderCopy(renderer, txt_env_tileset, &wall_tile[rand()%WALL_TILES], &DstR);
                /* Detach the game board texture from renderer */
                SDL_SetRenderTarget(renderer, NULL);
            }
            else if (item_type == Food) {
                game_board[game_board_w*y+x].p = rand()%FOOD_TILES;
            }
            break;
        }
    }
}

/* renders whole game state and blits everything to screen */
void render_screen(void)
{
    /* each frame everything is rendered from scratch */
    int x, y;
    int foff; /* game board field offset */
    SDL_Rect DstR = { 0, 0, TILE_SIZE, TILE_SIZE };
    SDL_Rect SrcR = { 0, 0, TILE_SIZE, TILE_SIZE };
    char score_string[15];
    /* render game backround with walls */
    SDL_RenderCopy(renderer, txt_game_board, NULL, NULL);

    /* render snake body and food*/
    for (y=0; y<game_board_h; y++) {
        for (x=0; x<game_board_w; x++) {
            foff = game_board_w*y+x;
            switch(game_board[foff].type) {
                case Snake:
                    /* select direction of character based on snake piece direction */
                    if (-game_board[foff].pdx == 0 && -game_board[foff].pdy == 1) {
                        SrcR.x = 0;
                    }
                    else if (-game_board[foff].pdx == 1 && -game_board[foff].pdy == 0) {
                        SrcR.x = TILE_SIZE;
                    }
                    else if (-game_board[foff].pdx == 0 && -game_board[foff].pdy == -1) {
                        SrcR.x = 2*TILE_SIZE;
                    }
                    else if (-game_board[foff].pdx == -1 && -game_board[foff].pdy == 0) {
                        SrcR.x = 3*TILE_SIZE;
                    }
                    /* select type of character based on game board field parameter */
                    SrcR.y = game_board[foff].p*3*(TILE_SIZE+1) + 1;
                    /* select character animation frame based on frame counter */
                    if (frame == 1) {
                        SrcR.y += (TILE_SIZE+1);
                    }
                    else if (frame == 3) {
                        SrcR.y += 2*(TILE_SIZE+1);
                    }
                    /* select character position on screen */
                    DstR.x = x*TILE_SIZE;
                    DstR.y = y*TILE_SIZE;
                    DstR.x += (CHAR_ANIM_FRAMES-frame)*game_board[foff].pdx*TILE_SIZE/CHAR_ANIM_FRAMES;
                    DstR.y += (CHAR_ANIM_FRAMES-frame)*game_board[foff].pdy*TILE_SIZE/CHAR_ANIM_FRAMES;
                    /* render character to screen buffer */
                    SDL_RenderCopy(renderer, txt_char_tileset, &SrcR, &DstR);
                    break;
                case Food:
                    DstR.x = x*TILE_SIZE;
                    DstR.y = y*TILE_SIZE;
                    SDL_RenderCopy(renderer, txt_food_tileset,
                                   &food_tile[game_board[foff].p], &DstR);
                    break;
                default:
                    break;
            }
        }
    }

    /* Extra elements dependent on current game state. */
    /* TODO: Currently these are just placeholders - implement "real" messages and UI */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    switch (game_state) {
        case NotStarted:
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
            DstR.w = SCR_W/4;          DstR.h = 8*TILE_SIZE;
            DstR.x = (SCR_W-DstR.w)/2; DstR.y = (SCR_H-TILE_SIZE-DstR.h)/2;
            SDL_RenderFillRect(renderer, &DstR);
            render_text(SCR_W/2,    SCR_H/2-3.5*TILE_SIZE, Center, "Vonsh");
            render_text(SCR_W/2-32, SCR_H/2-2*TILE_SIZE,   Right, "Arrows:");
            render_text(SCR_W/2-32, SCR_H/2-2*TILE_SIZE,   Left, " snake control");
            render_text(SCR_W/2-32, SCR_H/2-TILE_SIZE,     Right, "SPACE:");
            render_text(SCR_W/2-32, SCR_H/2-TILE_SIZE,     Left, " Pause/Resume");
            render_text(SCR_W/2-32, SCR_H/2,               Right, "ESC:");
            render_text(SCR_W/2-32, SCR_H/2,               Left, " Quit");
            render_text(SCR_W/2,    SCR_H/2+1.5*TILE_SIZE, Center, "Press SPACE to play");
            break;
        case GameOver:
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
            DstR.w = SCR_W/4;          DstR.h = 4*TILE_SIZE;
            DstR.x = (SCR_W-DstR.w)/2; DstR.y = (SCR_H-TILE_SIZE-DstR.h)/2;
            SDL_RenderFillRect(renderer, &DstR);
            render_text(SCR_W/2, SCR_H/2-1.5*TILE_SIZE, Center, "Game Over");
            render_text(SCR_W/2, SCR_H/2-0.25*TILE_SIZE, Center, "Press SPACE to play again");
            break;
        case Playing:
            break;
        case Paused:
            render_text(SCR_W/2, SCR_H/2-TILE_SIZE, Center, "Pause");
            break;
        default:
            break;
    }
    /* print score and hi score */
    sprintf(score_string, "SCORE: %d", score);
    render_text(TILE_SIZE, SCR_H-TILE_SIZE, Left, score_string);
    sprintf(score_string, "HI SCORE: %d", hi_score);
    render_text(SCR_W-TILE_SIZE, SCR_H-TILE_SIZE, Right, score_string);
    SDL_RenderPresent(renderer);
}

/* one iteration of game logic
    retval:
    0 - game continues
    1 - game over
 */
int progress_game(void)
{
    /* verify all kinds of collisions */
    if (hx+dhx < 0             || hy+dhy < 0 ||
        hx+dhx >= game_board_w || hy+dhy >= game_board_h) {
        /* collision with game board edge */
        return 1; /* GAME OVER */
    }
    else if (game_board[game_board_w*(hy+dhy)+hx+dhx].type == Food) {
        /* food hit - increase score, start expanding snake, seed new food */
        score++;
        hi_score = score > hi_score ? score : hi_score;
        expand_counter += score;
        seed_item(Food);
    }
    else if (game_board[game_board_w*(hy+dhy)+hx+dhx].type != Empty) {
        /* collision with snake body or wall */
        return 1; /* GAME OVER */
    }
    int foff; /* game board field offset */
    /* move snake head */
    foff = game_board_w*hy+hx;
    game_board[foff].ndx = dhx; /* update "neck" direction towards new head position */
    game_board[foff].ndy = dhy;
    hx += dhx;    hy += dhy; /* update head position */
    foff = game_board_w*hy+hx;
    game_board[foff].type = Snake;
    game_board[foff].ndx = dhx; /* update head direction */
    game_board[foff].ndy = dhy;
    game_board[foff].pdx = -dhx;
    game_board[foff].pdy = -dhy;

    int ix = tx, iy = ty; /* indexes used for traversing snake body */
    int pb1, pb2; /* buffers for shifting character kinds from tail to head */
    /* shift character kinds from tail to head */
    pb1 = game_board[game_board_w*iy+ix].p;
    do {
        /* move character kind from previous position to next. */
        foff = game_board_w*iy+ix;
        ix = ix + game_board[foff].ndx;
        iy = iy + game_board[foff].ndy;
        foff = game_board_w*iy+ix;
        pb2 = game_board[foff].p; /* buffer current character kind... */
        game_board[foff].p = pb1; /* and overwrite it with previous kind... */
        pb1 = pb2; /* and then exchange buffers */
    } while(ix!=hx || iy!=hy);

    if (expand_counter==0) { /* move snake tail if not expanding */
        foff = game_board_w*ty+tx; /* tail offset on game board */
        tx = tx + game_board[foff].ndx;
        ty = ty + game_board[foff].ndy;
        game_board[foff].type = Empty;
        game_board[foff].p = 0;
        game_board[foff].ndx = game_board[foff].pdx = 0;
        game_board[foff].ndy = game_board[foff].pdy = 0;
    }
    else { /* expand tail and seed new obstacle */
        game_board[game_board_w*ty+tx].p = rand()%TOTAL_CHAR;
        seed_item(Wall);
        expand_counter--;
    }
    return 0;
}

/* Callback for frame timer.
   Generates SDL_USEREVENT when main game loop shall render next frame. */
uint32_t tick_callback(uint32_t interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;
    userevent.type = SDL_USEREVENT;  userevent.code = 0;
    userevent.data1 = NULL;  userevent.data2 = NULL;
    event.type = SDL_USEREVENT;  event.user = userevent;
    SDL_PushEvent(&event);
    return interval;
}

void start_new_game(void) {
    /* initialize movement direction and position */
    init_game_board();

    score = expand_counter = 0;
    dhx = 0;   dhy = -1;
    hx = tx = game_board_w/2;   hy = ty = game_board_h/2;
    game_board[game_board_w*hy+hx].type = Snake;
    game_board[game_board_w*hy+hx].p = rand()%TOTAL_CHAR;
    game_board[game_board_w*hy+hx].ndx = dhx;
    game_board[game_board_w*hy+hx].ndy = dhy;
    game_board[game_board_w*hy+hx].pdx = -dhx;
    game_board[game_board_w*hy+hx].pdy = -dhy;
    seed_item(Food);
    ck_press = 0;
    frame = 0;
    game_state = Playing;

    render_screen();
}

void pause_game() {
    game_state = Paused;
    render_screen(); /* PAUSE screen */
}

void resume_game() {
    ck_press = 0;
    game_state = Playing;
}

int main(int argc, char ** argv)
{
    int quit = 0;
    SDL_Event event;

    /* init SDL library and load game resources */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    IMG_Init(IMG_INIT_PNG);
    screen = SDL_CreateWindow("Vonsh", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCR_W, SCR_H, 0);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    /* texture for rendering game background with obstacles */
    txt_game_board = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCR_W, SCR_H);

    /* textures with game assets */
    srf_env_tileset = IMG_Load("resources/board_tiles.png");
    txt_env_tileset = SDL_CreateTextureFromSurface(renderer, srf_env_tileset);
    srf_food_tileset = IMG_Load("resources/food_tiles.png");
    txt_food_tileset = SDL_CreateTextureFromSurface(renderer, srf_food_tileset);
    srf_char_tileset = IMG_Load("resources/character_tiles.png");
    txt_char_tileset = SDL_CreateTextureFromSurface(renderer, srf_char_tileset);
    srf_font = IMG_Load("resources/good_neighbors.png");
    txt_font = SDL_CreateTextureFromSurface(renderer, srf_font);

    allocate_arrays();

    init_text_renderer(renderer, txt_font);

    /* initialize environment tileset info */
    init_tiles();

    /* Draw splash screen. */
    game_state = NotStarted;
    hi_score = 0;
    init_game_board();
    render_screen();

    /* Start frame timer. */
    game_timer = SDL_AddTimer(GAME_SPEED/CHAR_ANIM_FRAMES, tick_callback, 0);
    while (!quit)
    {
        SDL_WaitEvent(&event);
        switch (event.type)
        {
            case SDL_USEREVENT: /* game tick event */
                if (game_state == Playing) {
                    frame = (frame+1) % CHAR_ANIM_FRAMES;
                    if (frame % CHAR_ANIM_FRAMES == 0) { /* updated game state frame */
                        if (progress_game()) { /* update game state and check if game is over */
                            game_state = GameOver; /* GAME OVER */
                            frame = CHAR_ANIM_FRAMES; /* hack allowing to render final position correctly */
                        }
                        else { /* game continues */
                            ck_press = 0; /* allow new direction key press */
                        }
                    }
                    /* render each frame - both animations and "full" game state updates */
                    render_screen();
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = 1;
                }
                else if (event.key.keysym.sym == SDLK_SPACE) {
                    switch (game_state) {
                        case NotStarted:
                        case GameOver:
                            /* start new game */
                            start_new_game();
                            break;
                        case Playing:
                            /* pause game */
                            pause_game();
                            break;
                        case Paused:
                            /* resume game */
                            resume_game();
                            break;
                        default:
                            break;
                    }
                }
                else if (game_state == Playing && event.key.repeat == 0 && ck_press == 0) {
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:  if (dhx == 0) { dhx = -1; dhy = 0; ck_press = 1; } break;
                        case SDLK_RIGHT: if (dhx == 0) { dhx = 1; dhy = 0; ck_press = 1; } break;
                        case SDLK_UP:    if (dhy == 0) { dhx = 0; dhy = -1; ck_press = 1; } break;
                        case SDLK_DOWN:  if (dhy == 0) { dhx = 0; dhy = 1; ck_press = 1; } break;
                    }
                }
                break;
            case SDL_QUIT:
                quit = 1;
                break;
        }
    }

    SDL_RemoveTimer(game_timer);

    SDL_DestroyTexture(txt_font);
    SDL_DestroyTexture(txt_char_tileset);
    SDL_DestroyTexture(txt_env_tileset);
    SDL_DestroyTexture(txt_food_tileset);
    SDL_DestroyTexture(txt_game_board);

    SDL_FreeSurface(srf_font);
    SDL_FreeSurface(srf_char_tileset);
    SDL_FreeSurface(srf_env_tileset);
    SDL_FreeSurface(srf_food_tileset);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    IMG_Quit();
    SDL_Quit();

    free_arrays();

    return 0;
}
