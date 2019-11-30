#pragma once
#include <string.h>

char valid_file(const char* path, const char* extension);
char* read_file(const char* path);
char* title_from_markdown(const char* file);