/**
 * @file kastore.h
 * @brief Public API for kastore.
 *
 * This is the API documentation for kastore.
 */
#ifndef KASTORE_H
#define KASTORE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define KAS_WARN_UNUSED __attribute__((warn_unused_result))
#define KAS_UNUSED(x) KAS_UNUSED_##x __attribute__((__unused__))
#else
#define KAS_WARN_UNUSED
#define KAS_UNUSED(x) KAS_UNUSED_##x
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/** @} */

/**
@defgroup ERROR_GROUP Error return values.
@{
*/
// clang-format off
/**
Generic error thrown when no other message can be generated.
*/
#define KAS_ERR_GENERIC                               -1
/**
An error occured during IO.
*/
#define KAS_ERR_IO                                    -2
/**
An unrecognised mode string was passed to open().
*/
#define KAS_ERR_BAD_MODE                              -3
/**
Out-of-memory condition.
*/
#define KAS_ERR_NO_MEMORY                             -4
/**
Attempt to read an unknown file format.
*/
#define KAS_ERR_BAD_FILE_FORMAT                       -5
/**
The file is in kastore format, but the version is too old for this
version of the library to read.
*/
#define KAS_ERR_VERSION_TOO_OLD                       -6
/**
The file is in kastore format, but the version is too new for this
version of the library to read.
*/
#define KAS_ERR_VERSION_TOO_NEW                       -7
/**
An unknown type key was specified.
*/
#define KAS_ERR_BAD_TYPE                              -8
/**
A zero-length key was specified.
*/
#define KAS_ERR_EMPTY_KEY                             -9
/**
A duplicate key was specified.
*/
#define KAS_ERR_DUPLICATE_KEY                         -10
/**
The requested key does not exist in the store.
*/
#define KAS_ERR_KEY_NOT_FOUND                         -11
/**
The requestion function cannot be called in the current mode.
*/
#define KAS_ERR_ILLEGAL_OPERATION                     -12
/**
The requested type does not match the type of the stored values.
*/
#define KAS_ERR_TYPE_MISMATCH                         -13
/**
End of file was reached while reading data.
*/
#define KAS_ERR_EOF                                   -14
/**
Unknown flags were provided to open.
*/
#define KAS_ERR_BAD_FLAGS                             -15
/** @} */

/* Flags for open */
#define KAS_READ_ALL            1


/**
@defgroup TYPE_GROUP Data types.
@{
*/
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
/** @} */

#define KAS_NUM_TYPES           10

#define KAS_READ                1
#define KAS_WRITE               2

/**
@defgroup FILE_VERSION_GROUP File version macros.
@{
*/
/**
The file version major number. Incremented when any breaking changes are made
to the file format.
*/
#define KAS_FILE_VERSION_MAJOR  1
/**
The file version minor number. Incremented when non-breaking backward-compatible
changes are made to the file format.
*/
#define KAS_FILE_VERSION_MINOR  0
/** @} */

/**
@defgroup API_VERSION_GROUP API version macros.
@{
*/
/**
The library major version. Incremented when breaking changes to the API or ABI are
introduced. This includes any changes to the signatures of functions and the
sizes and types of externally visible structs.
*/
#define KAS_VERSION_MAJOR   2
/**
The library minor version. Incremented when non-breaking backward-compatible changes
to the API or ABI are introduced, i.e., the addition of a new function.
*/
#define KAS_VERSION_MINOR   0
/**
The library patch version. Incremented when any changes not relevant to the
to the API or ABI are introduced, i.e., internal refactors of bugfixes.
*/
#define KAS_VERSION_PATCH   0
/** @} */

#define KAS_HEADER_SIZE             64
#define KAS_ITEM_DESCRIPTOR_SIZE    64
#define KAS_MAGIC                   "\211KAS\r\n\032\n"
#define KAS_ARRAY_ALIGN             8
// clang-format on

typedef struct {
    int type;
    size_t key_len;
    size_t array_len;
    char *key;
    void *array;
    size_t key_start;
    size_t array_start;
} kaitem_t;

/**
@brief A file-backed store of key-array values.
*/
typedef struct {
    int flags;
    int mode;
    int file_version[2];
    size_t num_items;
    kaitem_t *items;
    FILE *file;
    size_t file_size;
    long file_offset;
    char *read_buffer;
} kastore_t;

/**
@brief Library version information.
*/
typedef struct {
    /** @brief The major version number. */
    int major;
    /** @brief The minor version number. */
    int minor;
    /** @brief The patch version number. */
    int patch;
} kas_version_t;

/**
@brief Open a store from a given file in read ("r"), write ("w") or
append ("a") mode.

@rst
In read mode, a store can be queried using the :ref:`get functions
<sec_c_api_get>` and any attempts to write to the store will return an error.
In write and append mode, the store can written to using the :ref:`put
functions <sec_c_api_put>` and any attempt to read will return an error.

After :c:func:`kastore_open` has been called on a particular store,
:c:func:`kastore_close` must be called to avoid leaking memory. This must also
be done when :c:func:`kastore_open` returns an error.

When opened in read-mode, the default is to read key/array values from file
on demand. This is useful when a subset of the data is required and we don't
wish to read the entire file. If the entire file is to be read, the
``KAS_READ_ALL`` flag may be specified to improve performance.

**Flags**

KAS_READ_ALL
    If this option is specified, read the entire file at
    open time. This will give slightly better performance as the file can
    be read sequentially in a single pass.

@endrst

@param self A pointer to a kastore object.
@param filename The file path to open.
@param mode The open mode: can be read ("r"), write ("w") or append ("a").
@param flags The open flags.
@return Return 0 on success or a negative value on failure.
*/
int kastore_open(kastore_t *self, const char *filename, const char *mode, int flags);

/**
@brief Open a store from a given FILE pointer.

@rst
Behaviour, mode and flags follow that of :c:func:`kastore_open`,
except append mode is not supported.
The ``file`` argument must be opened in an appropriate mode (e.g. "r"
for a kastore in "r" mode).  Files open with other modes will result
in KAS_ERR_IO being returned when read/write operations are attempted.

The FILE will not be closed when :c:func:`kastore_close` is called.
If the KAS_READ_ALL flag is supplied, no ``seek`` operations will be
performed on the FILE and so streams such as stdin, FIFOs etc are
supported. The FILE pointer will be positioned exactly at the end
of the kastore encoded bytes once reading is completed, and reading
multiple stores from the same FILE sequentially is fully supported.
@endrst

@param self A pointer to a kastore object.
@param file The FILE* to read/write the store from/to.
@param mode The open mode: can be read ("r") or write ("w").
@param flags The open flags.
@return Return 0 on success or a negative value on failure.
*/
int kastore_openf(kastore_t *self, FILE *file, const char *mode, int flags);

/**
@brief Close an opened store, freeing all resources.

Any store that has been opened must be closed to avoid memory leaks
(including cases in which errors have occured). It is not an error to
call ``kastore_close`` multiple times on the same object, but
``kastore_open`` must be called before ``kastore_close``.

@param self A pointer to a kastore object.
@return Return 0 on success or a negative value on failure.
*/
int kastore_close(kastore_t *self);

/**
@brief Return 1 if the store contains the specified key and 0 if it does not.

@rst
Queries the store for the specified key and returns 1 if it exists. If the
key does not exist, 0 is returned. If an error occurs (for example, if querying
the store while it is in write-mode), a negative value is returned.

For keys that are standard NULL terminated strings, the :c:func:`kastore_containss`
function may be more convenient.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param key_len The length of the key.
@return Return 1 if the key is present and 0 if it does not. If an error occurs,
    return a negative value.
*/
int kastore_contains(kastore_t *self, const char *key, size_t key_len);

/**
@brief Return 1 if the store contains the specified NULL terminated key
and 0 if it does not.

@rst
Queries the store for the specified key, which must be a NULL terminated string,
and returns 1 if it exists. If the
key does not exist, 0 is returned. If an error occurs (for example, if querying
the store while it is in write-mode), a negative value is returned.
the array in the specified destination pointers.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@return Return 1 if the key is present and 0 if it does not. If an error occurs,
    return a negative value.
*/
int kastore_containss(kastore_t *self, const char *key);

/**
@brief Get the array for the specified key.

@rst
Queries the store for the specified key and stores pointers to the memory for
the corresponding array, the number of elements in this array and the type of
the array in the specified destination pointers. This is the most general form
of ``get`` query in kastore, as non NULL-terminated strings can be used as
keys and the resulting array is returned in a generic pointer. When standard C
strings are used as keys and the type of the array is known, it is more
convenient to use the :ref:`typed variants <sec_c_api_typed_get>` of this function.

The returned array points to memory that is internally managed by the store
and must not be freed or modified. The pointer is guaranteed to be valid
until :c:func:`kastore_close` is called.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param key_len The length of the key.
@param array The destination pointer for the array.
@param array_len The destination pointer for the number of elements
in the array.
@param type The destination pointer for the type code of the array.
@return Return 0 on success or a negative value on failure.
*/
int kastore_get(kastore_t *self, const char *key, size_t key_len, void **array,
    size_t *array_len, int *type);

/**
@brief Get the array for the specified NULL-terminated key.

@rst
As for :c:func:`kastore_get()` except the key is a NULL-terminated string.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param array The destination pointer for the array.
@param array_len The destination pointer for the number of elements
in the array.
@param type The destination pointer for the type code of the array.
@return Return 0 on success or a negative value on failure.
*/
int kastore_gets(
    kastore_t *self, const char *key, void **array, size_t *array_len, int *type);

/**
@defgroup TYPED_GETS_GROUP Typed get functions.
@{
*/

int kastore_gets_int8(
    kastore_t *self, const char *key, int8_t **array, size_t *array_len);
int kastore_gets_uint8(
    kastore_t *self, const char *key, uint8_t **array, size_t *array_len);
int kastore_gets_int16(
    kastore_t *self, const char *key, int16_t **array, size_t *array_len);
int kastore_gets_uint16(
    kastore_t *self, const char *key, uint16_t **array, size_t *array_len);
int kastore_gets_int32(
    kastore_t *self, const char *key, int32_t **array, size_t *array_len);
int kastore_gets_uint32(
    kastore_t *self, const char *key, uint32_t **array, size_t *array_len);
int kastore_gets_int64(
    kastore_t *self, const char *key, int64_t **array, size_t *array_len);
int kastore_gets_uint64(
    kastore_t *self, const char *key, uint64_t **array, size_t *array_len);
int kastore_gets_float32(
    kastore_t *self, const char *key, float **array, size_t *array_len);
int kastore_gets_float64(
    kastore_t *self, const char *key, double **array, size_t *array_len);

/** @} */

/**
@brief Insert the specified key-array pair into the store.

@rst
A key with the specified length is inserted into the store and associated with
an array of the specified type and number of elements. The contents of the
specified key and array are copied. Keys can be any sequence of bytes but must
be at least one byte long and be unique. There is no restriction on the
contents of arrays. This is the most general form of ``put`` operation in
kastore; when the type of the array is known and the keys are standard C
strings, it is usually more convenient to use the :ref:`typed variants
<sec_c_api_typed_put>` of this function.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param key_len The length of the key.
@param array The array.
@param array_len The number of elements in the array.
@param type The type of the array.
@param flags The insertion flags. Currently unused.
@return Return 0 on success or a negative value on failure.
*/
int kastore_put(kastore_t *self, const char *key, size_t key_len, const void *array,
    size_t array_len, int type, int flags);
/**
@brief Insert the specified NULL terminated key and array pair into the store.

@rst
As for :c:func:`kastore_put` except the key must be NULL-terminated C string.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param array The array.
@param array_len The number of elements in the array.
@param type The type of the array.
@param flags The insertion flags. Currently unused.
@return Return 0 on success or a negative value on failure.
*/
int kastore_puts(kastore_t *self, const char *key, const void *array, size_t array_len,
    int type, int flags);

/**
 @defgroup TYPED_PUTS_GROUP Typed put functions.
 @{
 */

int kastore_puts_int8(
    kastore_t *self, const char *key, const int8_t *array, size_t array_len, int flags);
int kastore_puts_uint8(
    kastore_t *self, const char *key, const uint8_t *array, size_t array_len, int flags);
int kastore_puts_int16(
    kastore_t *self, const char *key, const int16_t *array, size_t array_len, int flags);
int kastore_puts_uint16(kastore_t *self, const char *key, const uint16_t *array,
    size_t array_len, int flags);
int kastore_puts_int32(
    kastore_t *self, const char *key, const int32_t *array, size_t array_len, int flags);
int kastore_puts_uint32(kastore_t *self, const char *key, const uint32_t *array,
    size_t array_len, int flags);
int kastore_puts_int64(
    kastore_t *self, const char *key, const int64_t *array, size_t array_len, int flags);
int kastore_puts_uint64(kastore_t *self, const char *key, const uint64_t *array,
    size_t array_len, int flags);
int kastore_puts_float32(
    kastore_t *self, const char *key, const float *array, size_t array_len, int flags);
int kastore_puts_float64(
    kastore_t *self, const char *key, const double *array, size_t array_len, int flags);

/** @} */

/**
@brief Insert the specified key-array pair into the store, transferring ownership
of the malloced array buffer to the store (own-put).

@rst
A key with the specified length is inserted into the store and associated with
an array of the specified type and number of elements. The contents of the
specified key is copied, but the array buffer is taken directly and freed when
the store is closed. The array buffer must be a pointer returned by ``malloc``
or ``calloc``. Ownership of the buffer is not taken unless the function returns
successfully.

Apart from taking ownership of the array buffer, the semantics of this
function are identical to :c:func:`kastore_put`.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param key_len The length of the key.
@param array The array. Must be a pointer returned by malloc/calloc.
@param array_len The number of elements in the array.
@param type The type of the array.
@param flags The insertion flags. Currently unused.
@return Return 0 on success or a negative value on failure.
*/
int kastore_oput(kastore_t *self, const char *key, size_t key_len, void *array,
    size_t array_len, int type, int flags);
/**
@brief Insert the specified NULL terminated key and array pair into the store,
transferring ownership of the malloced array buffer to the store (own-put).

@rst
As for :c:func:`kastore_oput` except the key must be NULL-terminated C string.
@endrst

@param self A pointer to a kastore object.
@param key The key.
@param array The array. Must be a pointer returned by malloc/calloc.
@param array_len The number of elements in the array.
@param type The type of the array.
@param flags The insertion flags. Currently unused.
@return Return 0 on success or a negative value on failure.
*/
int kastore_oputs(kastore_t *self, const char *key, void *array, size_t array_len,
    int type, int flags);

/**
 @defgroup TYPED_OPUTS_GROUP Typed own-and-put functions.
 @{
 */

int kastore_oputs_int8(
    kastore_t *self, const char *key, int8_t *array, size_t array_len, int flags);
int kastore_oputs_uint8(
    kastore_t *self, const char *key, uint8_t *array, size_t array_len, int flags);
int kastore_oputs_int16(
    kastore_t *self, const char *key, int16_t *array, size_t array_len, int flags);
int kastore_oputs_uint16(
    kastore_t *self, const char *key, uint16_t *array, size_t array_len, int flags);
int kastore_oputs_int32(
    kastore_t *self, const char *key, int32_t *array, size_t array_len, int flags);
int kastore_oputs_uint32(
    kastore_t *self, const char *key, uint32_t *array, size_t array_len, int flags);
int kastore_oputs_int64(
    kastore_t *self, const char *key, int64_t *array, size_t array_len, int flags);
int kastore_oputs_uint64(
    kastore_t *self, const char *key, uint64_t *array, size_t array_len, int flags);
int kastore_oputs_float32(
    kastore_t *self, const char *key, float *array, size_t array_len, int flags);
int kastore_oputs_float64(
    kastore_t *self, const char *key, double *array, size_t array_len, int flags);

/** @} */

void kastore_print_state(kastore_t *self, FILE *out);

/**
@brief Returns a description of the specified error code.

@param err The error code.
@return String describing the error code.
*/
const char *kas_strerror(int err);

/**
@brief Returns the API version.

@rst
The API follows the `semver convention <https://semver.org/>`_, where the
major, minor and patch numbers have specific meanings. The versioning
scheme here also takes into account ABI compatability.
@endrst
*/
kas_version_t kas_version(void);

#define kas_safe_free(pointer)                                                          \
    do {                                                                                \
        if (pointer != NULL) {                                                          \
            free(pointer);                                                              \
            pointer = NULL;                                                             \
        }                                                                               \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
