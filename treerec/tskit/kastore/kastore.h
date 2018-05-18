#ifndef KASTORE_H
#define KASTORE_H

#ifdef __GNUC__
    #define KAS_WARN_UNUSED __attribute__ ((warn_unused_result))
    #define KAS_UNUSED(x) KAS_UNUSED_ ## x __attribute__((__unused__))
#else
    #define KAS_WARN_UNUSED
    #define KAS_UNUSED(x) KAS_UNUSED_ ## x
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define KAS_ERR_GENERIC                               -1
#define KAS_ERR_IO                                    -2
#define KAS_ERR_BAD_MODE                              -3
#define KAS_ERR_NO_MEMORY                             -4
#define KAS_ERR_BAD_FILE_FORMAT                       -5
#define KAS_ERR_VERSION_TOO_OLD                       -6
#define KAS_ERR_VERSION_TOO_NEW                       -7
#define KAS_ERR_BAD_TYPE                              -8
#define KAS_ERR_EMPTY_KEY                             -9
#define KAS_ERR_DUPLICATE_KEY                         -10
#define KAS_ERR_KEY_NOT_FOUND                         -11
#define KAS_ERR_ILLEGAL_OPERATION                     -12
#define KAS_ERR_TYPE_MISMATCH                         -13

/* Flags for open */
#define KAS_NO_MMAP             1

#define KAS_FILE_VERSION_MAJOR  1
#define KAS_FILE_VERSION_MINOR  0

#define KAS_INT8                0
#define KAS_UINT8               1
#define KAS_INT16               2
#define KAS_UINT16              3
#define KAS_INT32               4
#define KAS_UINT32              5
#define KAS_INT64               6
#define KAS_UINT64              7
#define KAS_FLOAT32             8
#define KAS_FLOAT64             9
#define KAS_NUM_TYPES           10

#define KAS_READ                1
#define KAS_WRITE               2

#define KAS_HEADER_SIZE             64
#define KAS_ITEM_DESCRIPTOR_SIZE    64
#define KAS_MAGIC                   "\211KAS\r\n\032\n"
#define KAS_ARRAY_ALIGN             8

typedef struct {
    int type;
    size_t key_len;
    size_t array_len;
    char *key;
    void *array;
    size_t key_start;
    size_t array_start;
} kaitem_t;

typedef struct {
    int flags;
    int mode;
    int file_version[2];
    size_t num_items;
    kaitem_t *items;
    FILE *file;
    const char *filename;
    size_t file_size;
    char *read_buffer;
} kastore_t;

int kastore_open(kastore_t *self, const char *filename, const char *mode, int flags);
int kastore_close(kastore_t *self);
int kastore_get(kastore_t *self, const char *key, size_t key_len,
       void **array, size_t *array_len, int *type);
int kastore_gets(kastore_t *self, const char *key,
       void **array, size_t *array_len, int *type);
int kastore_gets_int8(kastore_t *self, const char *key, int8_t **array, size_t *array_len);
int kastore_gets_uint8(kastore_t *self, const char *key, uint8_t **array, size_t *array_len);
int kastore_gets_int16(kastore_t *self, const char *key, int16_t **array, size_t *array_len);
int kastore_gets_uint16(kastore_t *self, const char *key, uint16_t **array, size_t *array_len);
int kastore_gets_int32(kastore_t *self, const char *key, int32_t **array, size_t *array_len);
int kastore_gets_uint32(kastore_t *self, const char *key, uint32_t **array, size_t *array_len);
int kastore_gets_int64(kastore_t *self, const char *key, int64_t **array, size_t *array_len);
int kastore_gets_uint64(kastore_t *self, const char *key, uint64_t **array, size_t *array_len);
int kastore_gets_float32(kastore_t *self, const char *key, float **array, size_t *array_len);
int kastore_gets_float64(kastore_t *self, const char *key, double **array, size_t *array_len);

int kastore_put(kastore_t *self, const char *key, size_t key_len,
       const void *array, size_t array_len, int type, int flags);
int kastore_puts(kastore_t *self, const char *key,
       const void *array, size_t array_len, int type, int flags);
/* Typed puts for convenience */
int kastore_puts_uint8(kastore_t *self, const char *key, const uint8_t *array,
        size_t array_len, int flags);
int kastore_puts_int8(kastore_t *self, const char *key, const int8_t *array,
        size_t array_len, int flags);
int kastore_puts_int16(kastore_t *self, const char *key, const int16_t *array,
        size_t array_len, int flags);
int kastore_puts_uint16(kastore_t *self, const char *key, const uint16_t *array,
        size_t array_len, int flags);
int kastore_puts_int32(kastore_t *self, const char *key, const int32_t *array,
        size_t array_len, int flags);
int kastore_puts_uint32(kastore_t *self, const char *key, const uint32_t *array,
        size_t array_len, int flags);
int kastore_puts_int64(kastore_t *self, const char *key, const int64_t *array,
        size_t array_len, int flags);
int kastore_puts_uint64(kastore_t *self, const char *key, const uint64_t *array,
        size_t array_len, int flags);
int kastore_puts_float32(kastore_t *self, const char *key, const float *array,
        size_t array_len, int flags);
int kastore_puts_float64(kastore_t *self, const char *key, const double *array,
        size_t array_len, int flags);

const char *kas_strerror(int err);

/* Debugging */
void kastore_print_state(kastore_t *self, FILE *out);

#define kas_safe_free(pointer) \
do {\
    if (pointer != NULL) {\
        free(pointer);\
        pointer = NULL;\
    }\
} while (0)


#endif
