#include <stdlib.h>
#include <string.h>

#include <rasta/rmemory.h>

void *rmalloc(unsigned int size) {
    return malloc(size);
}

void *rrealloc(void *element, unsigned int size) {
    return realloc(element, size);
}

void rfree(void *element) {
    free(element);
}

void *rmemcpy(void *dest, const void *src, unsigned int n) {
    return memcpy(dest, src, n);
}

void *rmemset(void *dest, int ch, unsigned int n) {
    return memset(dest, ch, n);
}

void rstrcpy(char *dest, const char *src) {
    strcpy(dest, src);
}

void rstrcat(char *dest, const char *src) {
    strcat(dest, src);
}

int rmemcmp(const void *a, const void *b, unsigned int len) {
    return memcmp(a, b, len);
}
