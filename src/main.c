#include <stdio.h>
#include <stdlib.h>
#include "duktape/duktape.h"

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

int main(int argc, char *argv[]) {
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

	return 0;
}
