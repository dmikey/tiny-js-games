#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include "duktape/duktape.h"


/* game config  */
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


/* javascript bridge logic */
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
static duk_ret_t my_setter(duk_context *ctx) {
    printf("setter called\n");
}

static void setup_context(duk_context *ctx) {
  
    duk_push_object(ctx);
    duk_push_c_function(ctx, my_setter, 1);
    duk_push_string(ctx, "_title");
    duk_put_prop_string(ctx, -2, "title");

    // Define console object
    duk_push_object(ctx);
    duk_push_c_function(ctx, native_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "log");
    duk_put_global_string(ctx, "console");
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

 /* Standard Routines and Interfaces. */
static int handle_key_event_(playerContext *ctx, SDL_Scancode key_code)
{
    switch (key_code) {
    /* Quit. */
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_Q:
        return SDL_APP_SUCCESS;
    /* Restart the game as if the program was launched. */
    case SDL_SCANCODE_R:
        // player_initialize(ctx);
        break;
    /* Decide new direction of the player. */
    case SDL_SCANCODE_RIGHT:
        // player_redir(ctx, player_DIR_RIGHT);
        break;
    case SDL_SCANCODE_UP:
        // player_redir(ctx, player_DIR_UP);
        break;
    case SDL_SCANCODE_LEFT:
        // player_redir(ctx, player_DIR_LEFT);
        break;
    case SDL_SCANCODE_DOWN:
        // player_redir(ctx, player_DIR_DOWN);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as = (AppState *)appstate;
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
