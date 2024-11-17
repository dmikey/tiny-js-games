#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include "duktape/duktape.h"

#define STEP_RATE_IN_MILLISECONDS  125
#define player_BLOCK_SIZE_IN_PIXELS 24
#define player_GAME_WIDTH  24U
#define player_GAME_HEIGHT 18U

#define SDL_WINDOW_WIDTH           (player_BLOCK_SIZE_IN_PIXELS * player_GAME_WIDTH)
#define SDL_WINDOW_HEIGHT          (player_BLOCK_SIZE_IN_PIXELS * player_GAME_HEIGHT)


#define player_MATRIX_SIZE (player_GAME_WIDTH * player_GAME_HEIGHT)

#define THREE_BITS  0x7U /* ~CHAR_MAX >> (CHAR_BIT - player_CELL_MAX_BITS) */
#define SHIFT(x, y) (((x) + ((y) * player_GAME_WIDTH)) * player_CELL_MAX_BITS)

typedef enum
{
    player_CELL_NOTHING = 0U,
    player_CELL_SRIGHT = 1U,
    player_CELL_SUP = 2U,
    player_CELL_SLEFT = 3U,
    player_CELL_SDOWN = 4U,
    player_CELL_FOOD = 5U
} playerCell;

#define player_CELL_MAX_BITS 3U /* floor(log2(player_CELL_FOOD)) + 1 */

typedef enum
{
    player_DIR_RIGHT,
    player_DIR_UP,
    player_DIR_LEFT,
    player_DIR_DOWN
} playerDirection;

typedef struct
{
    unsigned char cells[(player_MATRIX_SIZE * player_CELL_MAX_BITS) / 8U];
    char head_xpos;
    char head_ypos;
    char tail_xpos;
    char tail_ypos;
    char next_dir;
    char inhibit_tail_step;
    unsigned occupied_cells;
} playerContext;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    playerContext player_ctx;
    Uint64 last_step;
} AppState;

static duk_ret_t native_print(duk_context *ctx) {
	int n = duk_get_top(ctx);  // #args
	for (int i = 0; i < n; i++) {
		if (i > 0) {
			printf(" ");
		}
		printf("%s", duk_safe_to_string(ctx, i));
	}
	printf("\n");
	return 0;
}

static char* read_file(const char *filename, long *file_size) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "Could not open file: %s\n", filename);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	*file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *script = (char *)malloc(*file_size + 1);
	if (!script) {
		fprintf(stderr, "Memory allocation failed\n");
		fclose(file);
		return NULL;
	}

	fread(script, 1, *file_size, file);
	script[*file_size] = '\0';
	fclose(file);

	return script;
}

static void setup_context(duk_context *ctx) {
    // Define console object
    duk_push_object(ctx);
    duk_push_c_function(ctx, native_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "log");
    duk_put_global_string(ctx, "console");
}

playerCell player_cell_at(const playerContext *ctx, char x, char y)
{
    const int shift = SHIFT(x, y);
    unsigned short range;
    SDL_memcpy(&range, ctx->cells + (shift / 8), sizeof(range));
    return (playerCell)((range >> (shift % 8)) & THREE_BITS);
}

static void set_rect_xy_(SDL_FRect *r, short x, short y)
{
    r->x = (float)(x * player_BLOCK_SIZE_IN_PIXELS);
    r->y = (float)(y * player_BLOCK_SIZE_IN_PIXELS);
}

static void put_cell_at_(playerContext *ctx, char x, char y, playerCell ct)
{
    const int shift = SHIFT(x, y);
    const int adjust = shift % 8;
    unsigned char *const pos = ctx->cells + (shift / 8);
    unsigned short range;
    SDL_memcpy(&range, pos, sizeof(range));
    range &= ~(THREE_BITS << adjust); /* clear bits */
    range |= (ct & THREE_BITS) << adjust;
    SDL_memcpy(pos, &range, sizeof(range));
}

static int are_cells_full_(playerContext *ctx)
{
    return ctx->occupied_cells == player_GAME_WIDTH * player_GAME_HEIGHT;
}

static void new_food_pos_(playerContext *ctx)
{
    while (true) {
        const char x = (char) SDL_rand(player_GAME_WIDTH);
        const char y = (char) SDL_rand(player_GAME_HEIGHT);
        if (player_cell_at(ctx, x, y) == player_CELL_NOTHING) {
            put_cell_at_(ctx, x, y, player_CELL_FOOD);
            break;
        }
    }
}

void player_initialize(playerContext *ctx)
{
    int i;
    SDL_zeroa(ctx->cells);
    ctx->head_xpos = ctx->tail_xpos = player_GAME_WIDTH / 2;
    ctx->head_ypos = ctx->tail_ypos = player_GAME_HEIGHT / 2;
    ctx->next_dir = player_DIR_RIGHT;
    ctx->inhibit_tail_step = ctx->occupied_cells = 4;
    --ctx->occupied_cells;
    put_cell_at_(ctx, ctx->tail_xpos, ctx->tail_ypos, player_CELL_SRIGHT);
    for (i = 0; i < 4; i++) {
        new_food_pos_(ctx);
        ++ctx->occupied_cells;
    }
}

void player_redir(playerContext *ctx, playerDirection dir)
{
    playerCell ct = player_cell_at(ctx, ctx->head_xpos, ctx->head_ypos);
    if ((dir == player_DIR_RIGHT && ct != player_CELL_SLEFT) ||
        (dir == player_DIR_UP && ct != player_CELL_SDOWN) ||
        (dir == player_DIR_LEFT && ct != player_CELL_SRIGHT) ||
        (dir == player_DIR_DOWN && ct != player_CELL_SUP)) {
        ctx->next_dir = dir;
    }
}

static void wrap_around_(char *val, char max)
{
    if (*val < 0) {
        *val = max - 1;
    } else if (*val > max - 1) {
        *val = 0;
    }
}

void player_step(playerContext *ctx)
{
    const playerCell dir_as_cell = (playerCell)(ctx->next_dir + 1);
    playerCell ct;
    char prev_xpos;
    char prev_ypos;
    /* Move tail forward */
    if (--ctx->inhibit_tail_step == 0) {
        ++ctx->inhibit_tail_step;
        ct = player_cell_at(ctx, ctx->tail_xpos, ctx->tail_ypos);
        put_cell_at_(ctx, ctx->tail_xpos, ctx->tail_ypos, player_CELL_NOTHING);
        switch (ct) {
        case player_CELL_SRIGHT:
            ctx->tail_xpos++;
            break;
        case player_CELL_SUP:
            ctx->tail_ypos--;
            break;
        case player_CELL_SLEFT:
            ctx->tail_xpos--;
            break;
        case player_CELL_SDOWN:
            ctx->tail_ypos++;
            break;
        default:
            break;
        }
        wrap_around_(&ctx->tail_xpos, player_GAME_WIDTH);
        wrap_around_(&ctx->tail_ypos, player_GAME_HEIGHT);
    }
    /* Move head forward */
    prev_xpos = ctx->head_xpos;
    prev_ypos = ctx->head_ypos;
    switch (ctx->next_dir) {
    case player_DIR_RIGHT:
        ++ctx->head_xpos;
        break;
    case player_DIR_UP:
        --ctx->head_ypos;
        break;
    case player_DIR_LEFT:
        --ctx->head_xpos;
        break;
    case player_DIR_DOWN:
        ++ctx->head_ypos;
        break;
    }
    wrap_around_(&ctx->head_xpos, player_GAME_WIDTH);
    wrap_around_(&ctx->head_ypos, player_GAME_HEIGHT);
    /* Collisions */
    ct = player_cell_at(ctx, ctx->head_xpos, ctx->head_ypos);
    if (ct != player_CELL_NOTHING && ct != player_CELL_FOOD) {
        player_initialize(ctx);
        return;
    }
    put_cell_at_(ctx, prev_xpos, prev_ypos, dir_as_cell);
    put_cell_at_(ctx, ctx->head_xpos, ctx->head_ypos, dir_as_cell);
    if (ct == player_CELL_FOOD) {
        if (are_cells_full_(ctx)) {
            player_initialize(ctx);
            return;
        }
        new_food_pos_(ctx);
        ++ctx->inhibit_tail_step;
        ++ctx->occupied_cells;
    }
}

static int handle_key_event_(playerContext *ctx, SDL_Scancode key_code)
{
    switch (key_code) {
    /* Quit. */
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_Q:
        return SDL_APP_SUCCESS;
    /* Restart the game as if the program was launched. */
    case SDL_SCANCODE_R:
        player_initialize(ctx);
        break;
    /* Decide new direction of the player. */
    case SDL_SCANCODE_RIGHT:
        player_redir(ctx, player_DIR_RIGHT);
        break;
    case SDL_SCANCODE_UP:
        player_redir(ctx, player_DIR_UP);
        break;
    case SDL_SCANCODE_LEFT:
        player_redir(ctx, player_DIR_LEFT);
        break;
    case SDL_SCANCODE_DOWN:
        player_redir(ctx, player_DIR_DOWN);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as = (AppState *)appstate;
    playerContext *ctx = &as->player_ctx;
    const Uint64 now = SDL_GetTicks();
    SDL_FRect r;
    unsigned i;
    unsigned j;
    int ct;

    // run game logic if we're at or past the time to run it.
    // if we're _really_ behind the time to run it, run it
    // several times.
    while ((now - as->last_step) >= STEP_RATE_IN_MILLISECONDS) {
        player_step(ctx);
        as->last_step += STEP_RATE_IN_MILLISECONDS;
    }

    r.w = r.h = player_BLOCK_SIZE_IN_PIXELS;
    SDL_SetRenderDrawColor(as->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(as->renderer);
    for (i = 0; i < player_GAME_WIDTH; i++) {
        for (j = 0; j < player_GAME_HEIGHT; j++) {
            ct = player_cell_at(ctx, i, j);
            if (ct == player_CELL_NOTHING)
                continue;
            set_rect_xy_(&r, i, j);
            if (ct == player_CELL_FOOD)
                SDL_SetRenderDrawColor(as->renderer, 80, 80, 255, SDL_ALPHA_OPAQUE);
            else /* body */
                SDL_SetRenderDrawColor(as->renderer, 0, 128, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(as->renderer, &r);
        }
    }
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 0, SDL_ALPHA_OPAQUE); /*head*/
    set_rect_xy_(&r, ctx->head_xpos, ctx->head_ypos);
    SDL_RenderFillRect(as->renderer, &r);
    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}

static const struct
{
    const char *key;
    const char *value;
} extended_metadata[] =
{
    { SDL_PROP_APP_METADATA_URL_STRING, "https://examples.libsdl.org/SDL3/game/01-player/" },
    { SDL_PROP_APP_METADATA_CREATOR_STRING, "SDL team" },
    { SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Placed in the public domain" },
    { SDL_PROP_APP_METADATA_TYPE_STRING, "game" }
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    size_t i;

    if (!SDL_SetAppMetadata("Example player game", "1.0", "com.example.player")) {
        return SDL_APP_FAILURE;
    }

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script.js>\n", argv[0]);
		return 1;
	}

	long file_size;
	char *script = read_file(argv[1], &file_size);
	if (!script) {
		return 1;
	}

	duk_context *ctx = duk_create_heap_default();
	setup_context(ctx);

	if (duk_peval_string(ctx, script) != 0) {
		fprintf(stderr, "Error: %s\n", duk_safe_to_string(ctx, -1));
	}

	duk_pop(ctx);  /* pop eval result */
	duk_destroy_heap(ctx);
	free(script);
	
    for (i = 0; i < SDL_arraysize(extended_metadata); i++) {
        if (!SDL_SetAppMetadataProperty(extended_metadata[i].key, extended_metadata[i].value)) {
            return SDL_APP_FAILURE;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    AppState *as = SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer("examples/game/player", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, 0, &as->window, &as->renderer)) {
        return SDL_APP_FAILURE;
    }

    player_initialize(&as->player_ctx);

    as->last_step = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    playerContext *ctx = &((AppState *)appstate)->player_ctx;
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        return handle_key_event_(ctx, event->key.scancode);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate != NULL) {
        AppState *as = (AppState *)appstate;
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
    }
}
