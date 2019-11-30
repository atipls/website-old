#pragma once
#include "sandbird/sandbird.h"

char* prologue();
char* epilogue();
void render_page(sb_Stream* s, const char* path);
void render_index(sb_Stream* s);