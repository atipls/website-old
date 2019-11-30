#pragma once
#include <stdio.h>
#include <stdlib.h>

inline void* debugalloc(size_t size, char* func, char* file, int line) {
	void* mem = malloc(size);
	fprintf(stderr, "[ALLOC] %p %s (%s:%d)\n", mem, func, file, line);
	return mem;
}
inline void debugfree(void* data, char* func, char* file, int line) {
	free(data);
	fprintf(stderr, "[FREE ] %p %s (%s:%d)\n", data, func, file, line);
}
#ifdef _DEBUG
#define malloc(x) debugalloc((x), __FUNCTION__, __FILE__, __LINE__)
#define free(x) debugfree((x), __FUNCTION__, __FILE__, __LINE__)

#endif