#include "file.h"
#include <stdio.h>

#include "debugalloc.h"

char* _strndup(const char* s, size_t n) {
	char* result;
	size_t len = strnlen(s, n);
	result = (char*)malloc(len + 1);
	if (!result)
		return 0;
	result[len] = '\0';
	return (char*)memcpy(result, s, len);
}

char valid_file(const char* path, const char* extension) {
	size_t plen = strlen(path);
	size_t elen = strlen(extension);
	if (elen > plen)
		return 0;
	return strncmp(path + plen - elen, extension, elen) == 0;
}

char* read_file(const char* path) {
	char* source = NULL;
	FILE* fp = fopen(path, "r");
	if (!fp) goto _exit;
	fseek(fp, 0L, SEEK_END);
	long bufsize = ftell(fp);
	if (bufsize == -1) goto _exit;
	source = (char*)malloc(sizeof(char) * (bufsize + 1));
	if (!source) goto _exit;
	fseek(fp, 0L, SEEK_SET);
	size_t len = fread(source, sizeof(char), bufsize, fp);
	source[len++] = '\0';
_exit:
	if (fp) fclose(fp);
	return source;
}

char* title_from_markdown(const char* file) {
	char* p = (char*)file;
	size_t len = 0;
	while (*p == '#' || *p == ' ') p++;
	while (p[len] != '\n' && p[len] != '\0') len++;
	return _strndup(p, len);
}