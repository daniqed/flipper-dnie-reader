#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define FURI_LOG_E(...)
#define FURI_LOG_D(...)
#define FURI_LOG_I(...)
#define FURI_LOG_W(...)
#define furi_assert(x) ((void)(x))
typedef struct { uint8_t d[512]; size_t n; } BitBuffer;
BitBuffer* bit_buffer_alloc(size_t cap);
void bit_buffer_free(BitBuffer* b);
void bit_buffer_reset(BitBuffer* b);
void bit_buffer_append_bytes(BitBuffer* b, const uint8_t* d, size_t n);
size_t bit_buffer_get_size_bytes(const BitBuffer* b);
uint8_t bit_buffer_get_byte(const BitBuffer* b, size_t i);
const uint8_t* bit_buffer_get_data(const BitBuffer* b);
