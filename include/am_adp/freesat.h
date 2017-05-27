#ifndef FREESAT_H
#define FREESAT_H

#include <sys/types.h>

char *freesat_huffman_decode(const unsigned char *compressed, size_t size);

#endif