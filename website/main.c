#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>

#include "sandbird/sandbird.h"
#include "tinydir.h"

#include "file.h"
#include "html.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

static int sandbird_handler(sb_Event* e) {
	if (e->type == SB_EV_REQUEST) {
		sb_send_status(e->stream, 200, "Hello there");
		if (valid_file(e->path, ".md")) {
			sb_send_header(e->stream, "Content-Type", "text/html");
			render_page(e->stream, e->path);
			return SB_RES_OK;
		}
		if (valid_file(e->path, ".css")) {
			sb_send_header(e->stream, "Content-Type", "text/css");
			chdir("data");
			char* file = read_file(e->path + 1);
			chdir("..");
			if (file) {
				sb_writef(e->stream, file);
				free(file);
			} else sb_writef(e->stream, "");
			return SB_RES_OK;
		}
		if (!strcmp(e->path, "/")) {
			render_index(e->stream);
			return SB_RES_OK;
		}
		sb_writef(e->stream, "<h1>404. how did we get here?</h1>");
	}
	return SB_RES_OK;
}

int main() {
	setlocale(LC_ALL, "");

	sb_Options opt;
	memset(&opt, 0, sizeof(opt));
	opt.port = "80";
	opt.handler = sandbird_handler;

	sb_Server* server = sb_new_server(&opt);

	if (!server) {
		fprintf(stderr, "failed to initialize server\n");
		exit(EXIT_FAILURE);
	}
	for (;;) sb_poll_server(server, 1000);
	sb_close_server(server);
	return EXIT_SUCCESS;
															}