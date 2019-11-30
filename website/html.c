#include <stdio.h>
#include "html.h"
#include "file.h"
#include "md4c/render_html.h"
#include "tinydir.h"

#include "debugalloc.h"

char* prologue() {
	char* ret = read_file("data/prologue.t");
	if (!ret)
		return "<h1>no prologue</h1>";
	return ret;
}

char* epilogue() {
	char* ret = read_file("data/epilogue.t");
	if (!ret)
		return "<h1>no epilogue</h1>";
	return ret;
}

static void process_html(const MD_CHAR* text, MD_SIZE size, void* userdata) { sb_write((sb_Stream*)userdata, text, size); }
void render_page(sb_Stream* s, const char* path) {
	char* blogpath = malloc(strlen(path) + 10);
	if (!blogpath)
		return;
	sprintf(blogpath, "./blog/%s", path + 1);
	char* file = read_file(blogpath);
	char* pro = prologue();
	char* epi = epilogue();
	if (file) {
		char* title = title_from_markdown(file);
		sb_writef(s, pro, title);
		free(title);

		md_render_html(file, strlen(file), process_html, s, MD_DIALECT_GITHUB | MD_FLAG_LATEXMATHSPANS | MD_FLAG_WIKILINKS, 0);
		free(file);
	} else {
		sb_writef(s, pro, "404 not found");
		sb_writef(s, "<h1>404. how did we get here?</h1>");
	}
	sb_writef(s, epi);
	free(blogpath);
	free(pro);
	free(epi);
}
void render_index(sb_Stream* s) {
	char* pro = prologue();
	char* epi = epilogue();
	sb_writef(s, pro, "index");
	sb_writef(s, "<ul>");
	tinydir_dir dir;
	tinydir_open(&dir, "./blog/");
	while (dir.has_next) {
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		if (!file.is_dir && strcmp(file.name, "about.md")) {
			sb_writef(s, "<li> <a href=\"/%s\">%s</a></li>", file.name, file.name);
		}
		tinydir_next(&dir);
	}
	tinydir_close(&dir);

	sb_writef(s, "</ul>");
	sb_writef(s, epi);
	free(pro);
	free(epi);
}