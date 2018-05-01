#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "kastore.h"

#include <CUnit/Basic.h>

char * _tmp_file_name;
FILE * _devnull;

static void
test_bad_open_mode(void)
{
    int ret;
    kastore_t store;
    const char *bad_modes[] = {"", "R", "W", "read", "rw", "write"};
    size_t j;

    for (j = 0; j < sizeof(bad_modes) / sizeof(*bad_modes); j++) {
        ret = kastore_open(&store, "", bad_modes[j], 0);
        CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_MODE);
        ret = kastore_close(&store);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

static void
test_open_io_errors(void)
{
    int ret;
    kastore_t store;
    const char *msg1, *msg2;

    /* Read a non existent file */
    ret = kastore_open(&store, "", "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    CU_ASSERT_EQUAL_FATAL(errno, ENOENT);
    msg1 = kas_strerror(KAS_ERR_IO);
    msg2 = strerror(errno);
    CU_ASSERT_STRING_EQUAL(msg1, msg2);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Read a directory */
    ret = kastore_open(&store, "/", "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    CU_ASSERT_EQUAL_FATAL(errno, EISDIR)
    msg1 = kas_strerror(KAS_ERR_IO);
    msg2 = strerror(errno);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Write a directory */
    ret = kastore_open(&store, "./", "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    CU_ASSERT_EQUAL_FATAL(errno, EISDIR)
    msg1 = kas_strerror(KAS_ERR_IO);
    msg2 = strerror(errno);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);


    /* Write without permissions */
    ret = kastore_open(&store, "/noway.kas", "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    msg1 = kas_strerror(KAS_ERR_IO);
    msg2 = strerror(errno);
    CU_ASSERT_EQUAL_FATAL(errno, EACCES);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Reading /dev/null returns 0 bytes */
    ret = kastore_open(&store, "/dev/null", "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_FILE_FORMAT);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

}

static void
test_write_errors(void)
{
    int ret;
    kastore_t store;
    int64_t a[4] = {1, 2, 3, 4};

    /* Write /dev/null should be fine */
    ret = kastore_open(&store, "/dev/random", "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "a", a, 4, KAS_INT64, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "b", a, 4, KAS_INT64, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* TODO find some way to make it so we get IO errors when we flush */
}

static void
test_strerror(void)
{
    const int max_err = 100;  /* arbitrary */
    int err;
    const char *str;

    /* Make sure the errno=0 codepath for IO errors is exercised */
    errno = 0;
    for (err = 1; err < max_err ; err++) {
        str = kas_strerror(-err);
        CU_ASSERT_NOT_EQUAL_FATAL(str, NULL);
        CU_ASSERT(strlen(str) > 0);
    }
}

static void
test_bad_types(void)
{
    int ret;
    kastore_t store;
    uint32_t array[] = {1};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_puts(&store, "a", array, 1, -1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_TYPE);
    ret = kastore_puts(&store, "a", array, 1, -2, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_TYPE);
    ret = kastore_puts(&store, "a", array, 1, KAS_NUM_TYPES, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_TYPE);
    ret = kastore_puts(&store, "a", array, 1, KAS_NUM_TYPES + 1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_TYPE);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
verify_key_round_trip(const char **keys, size_t num_keys)
{
    int ret;
    kastore_t store;
    size_t j;
    uint32_t array[] = {1};
    uint32_t *a;
    size_t array_len;
    int type;

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    for (j = 0; j < num_keys; j++) {
        ret = kastore_put(&store, keys[j], strlen(keys[j]), array, 1, KAS_UINT32, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_open(&store, _tmp_file_name, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    kastore_print_state(&store, _devnull);

    CU_ASSERT_EQUAL(store.num_items, num_keys);
    for (j = 0; j < num_keys; j++) {
        ret = kastore_get(&store, keys[j], strlen(keys[j]),
                (const void **) &a, &array_len, &type);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(type, KAS_UINT32);
        CU_ASSERT_EQUAL(array_len, 1);
        CU_ASSERT_EQUAL(a[0], 1);
    }
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
test_different_key_length(void)
{
    const char *keys[] = {"a", "aa", "aaa", "aaaa", "aaaaa"};
    verify_key_round_trip(keys, sizeof(keys) / sizeof(*keys));
}

static void
test_different_key_length_reverse(void)
{
    const char *keys[] = {"aaaaaa", "aaaa", "aaa", "aa", "a"};
    verify_key_round_trip(keys, sizeof(keys) / sizeof(*keys));
}

static void
test_mixed_keys(void)
{
    const char *keys[] = {"x", "aabs", "pqrastuvw", "st", "12345", "67^%"};
    verify_key_round_trip(keys, sizeof(keys) / sizeof(*keys));
}

static void
test_duplicate_key(void)
{
    int ret;
    kastore_t store;
    uint32_t array[] = {1};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_put(&store, "a", 1, array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_put(&store, "b", 1, array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_put(&store, "a", 1, array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_DUPLICATE_KEY);
    CU_ASSERT_EQUAL_FATAL(store.num_items, 2);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_open(&store, _tmp_file_name, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    CU_ASSERT_EQUAL(store.num_items, 2);
    ret = kastore_close(&store);
}

static void
test_empty_key(void)
{
    int ret;
    kastore_t store;
    uint32_t array[] = {1};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_put(&store, "", 0, array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_EMPTY_KEY);
    ret = kastore_put(&store, "b", 0, array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_EMPTY_KEY);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
test_put_read_mode(void)
{
    int ret;
    kastore_t store;
    uint32_t array[] = {1};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_open(&store, _tmp_file_name, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "a", array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_ILLEGAL_OPERATION);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
test_get_write_mode(void)
{
    int ret;
    kastore_t store;
    uint32_t *a;
    size_t array_len;
    int type;

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_gets(&store, "xyz", (const void **) &a, &array_len, &type);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_ILLEGAL_OPERATION);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

static void
test_missing_key(void)
{
    int ret;
    kastore_t store;
    const uint32_t array[] = {1, 2, 3, 4};
    uint32_t *a;
    size_t array_len;
    int type;

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "abc", array, 4, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "defg", array, 2, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "hijkl", array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_open(&store, _tmp_file_name, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_gets(&store, "xyz", (const void **) &a, &array_len, &type);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_KEY_NOT_FOUND);
    ret = kastore_gets(&store, "a", (const void **) &a, &array_len, &type);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_KEY_NOT_FOUND);
    ret = kastore_gets(&store, "defgh", (const void **) &a, &array_len, &type);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_KEY_NOT_FOUND);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}


static void
test_simple_round_trip(void)
{
    int ret;
    kastore_t store;
    const uint32_t array[] = {1, 2, 3, 4};
    uint32_t *a;
    size_t j, array_len;
    int type;
    int flags[] = {0, 1};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_puts(&store, "c", array, 4, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "b", array, 2, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts(&store, "a", array, 1, KAS_UINT32, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    for (j = 0; j < sizeof(flags) / sizeof(*flags); j++) {
        ret = kastore_open(&store, _tmp_file_name, "r", flags[j]);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        CU_ASSERT_EQUAL(store.num_items, 3);
        ret = kastore_gets(&store, "a", (const void **) &a, &array_len, &type);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(type, KAS_UINT32);
        CU_ASSERT_EQUAL(array_len, 1);
        CU_ASSERT_EQUAL(a[0], 1);

        ret = kastore_gets(&store, "b", (const void **) &a, &array_len, &type);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(type, KAS_UINT32);
        CU_ASSERT_EQUAL(array_len, 2);
        CU_ASSERT_EQUAL(a[0], 1);
        CU_ASSERT_EQUAL(a[1], 2);

        ret = kastore_gets(&store, "c", (const void **) &a, &array_len, &type);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL(type, KAS_UINT32);
        CU_ASSERT_EQUAL(array_len, 4);
        CU_ASSERT_EQUAL(a[0], 1);
        CU_ASSERT_EQUAL(a[1], 2);
        CU_ASSERT_EQUAL(a[2], 3);
        CU_ASSERT_EQUAL(a[3], 4);

        ret = kastore_close(&store);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
}

static void
verify_bad_file(const char *filename, int err)
{
    int ret;
    kastore_t store;

    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, err);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = kastore_open(&store, filename, "r", KAS_NO_MMAP);
    CU_ASSERT_EQUAL_FATAL(ret, err);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

}

static void
test_empty_file(void)
{
    verify_bad_file("test-data/malformed/empty_file.kas", KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_bad_type(void)
{
    verify_bad_file("test-data/malformed/bad_type_9.kas", KAS_ERR_BAD_TYPE);
    verify_bad_file("test-data/malformed/bad_type_16.kas", KAS_ERR_BAD_TYPE);
}

static void
test_bad_filesizes(void)
{
    verify_bad_file("test-data/malformed/bad_filesize_0_-1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_filesize_0_1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_filesize_0_1024.kas",
            KAS_ERR_BAD_FILE_FORMAT);

    verify_bad_file("test-data/malformed/bad_filesize_10_-1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_filesize_10_1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_filesize_10_1024.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_bad_magic_number(void)
{
    verify_bad_file("test-data/malformed/bad_magic_number.kas", KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_version_0(void)
{
    verify_bad_file("test-data/malformed/version_0.kas", KAS_ERR_VERSION_TOO_OLD);
}

static void
test_version_100(void)
{
    verify_bad_file("test-data/malformed/version_100.kas", KAS_ERR_VERSION_TOO_NEW);
}

static void
test_truncated_file(void)
{
    verify_bad_file("test-data/malformed/truncated_file.kas", KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_key_offset_outside_file(void)
{
    verify_bad_file("test-data/malformed/key_offset_outside_file.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_array_offset_outside_file(void)
{
    verify_bad_file("test-data/malformed/array_offset_outside_file.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_key_len_outside_file(void)
{
    verify_bad_file("test-data/malformed/key_len_outside_file.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_array_len_outside_file(void)
{
    verify_bad_file("test-data/malformed/array_len_outside_file.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_bad_key_start(void)
{
    verify_bad_file("test-data/malformed/bad_key_start_-1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_key_start_1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_bad_array_start(void)
{
    verify_bad_file("test-data/malformed/bad_array_start_-8.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_array_start_-1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_array_start_1.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/bad_array_start_8.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
test_truncated_file_correct_size(void)
{
    verify_bad_file("test-data/malformed/truncated_file_correct_size_100.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/truncated_file_correct_size_128.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/truncated_file_correct_size_129.kas",
            KAS_ERR_BAD_FILE_FORMAT);
    verify_bad_file("test-data/malformed/truncated_file_correct_size_200.kas",
            KAS_ERR_BAD_FILE_FORMAT);
}

static void
verify_all_types_n_elements(size_t n)
{
    int ret;
    kastore_t store;
    const char *filename_pattern = "test-data/v1/all_types_%d_elements.kas";
    const char *keys[] = {
        "uint8", "int8", "uint32", "int32", "uint64", "int64", "float32",
        "float64"};
    const int types[] = {
        KAS_UINT8, KAS_INT8, KAS_UINT32, KAS_INT32,
        KAS_UINT64, KAS_INT64, KAS_FLOAT32, KAS_FLOAT64};
    size_t j, k, array_len;
    const void *a;
    int type;
    char filename[1024];

    sprintf(filename, filename_pattern, n);

    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    for (j = 0; j < sizeof(keys) / sizeof(*keys); j++) {
        ret = kastore_gets(&store, keys[j], &a, &array_len, &type);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        CU_ASSERT_EQUAL_FATAL(array_len, n);
        CU_ASSERT_FATAL(a != NULL);
        CU_ASSERT_EQUAL_FATAL(type, types[j]);
        for (k = 0; k < array_len; k++) {
            switch (type) {
                case KAS_UINT8:
                    CU_ASSERT_EQUAL_FATAL(((const uint8_t *) a)[k], (uint8_t) k);
                    break;
                case KAS_INT8:
                    CU_ASSERT_EQUAL_FATAL(((const int8_t *) a)[k], (int8_t) k);
                    break;
                case KAS_UINT32:
                    CU_ASSERT_EQUAL_FATAL(((const uint32_t *) a)[k], (uint32_t) k);
                    break;
                case KAS_INT32:
                    CU_ASSERT_EQUAL_FATAL(((const int32_t *) a)[k], (int32_t) k);
                    break;
                case KAS_UINT64:
                    CU_ASSERT_EQUAL_FATAL(((const uint64_t *) a)[k], (uint64_t) k);
                    break;
                case KAS_INT64:
                    CU_ASSERT_EQUAL_FATAL(((const int64_t *) a)[k], (int64_t) k);
                    break;
                case KAS_FLOAT32:
                    CU_ASSERT_EQUAL_FATAL(((const float *) a)[k], (float) k);
                    break;
                case KAS_FLOAT64:
                    CU_ASSERT_EQUAL_FATAL(((const double *) a)[k], (double) k);
                    break;
            }
        }
    }
    kastore_close(&store);
}

static void
test_all_types_n_elements(void)
{
    size_t j;

    for (j = 0; j < 10; j++) {
        verify_all_types_n_elements(j);
    }
}

static int
kastore_suite_init(void)
{
    int fd;
    static char template[] = "/tmp/kas_c_test_XXXXXX";

    _tmp_file_name = NULL;
    _devnull = NULL;

    _tmp_file_name = malloc(sizeof(template));
    if (_tmp_file_name == NULL) {
        return CUE_NOMEMORY;
    }
    strcpy(_tmp_file_name, template);
    fd = mkstemp(_tmp_file_name);
    if (fd == -1) {
        return CUE_SINIT_FAILED;
    }
    close(fd);
    _devnull = fopen("/dev/null", "w");
    if (_devnull == NULL) {
        return CUE_SINIT_FAILED;
    }
    return CUE_SUCCESS;
}

static int
kastore_suite_cleanup(void)
{
    if (_tmp_file_name != NULL) {
        unlink(_tmp_file_name);
        free(_tmp_file_name);
    }
    if (_devnull != NULL) {
        fclose(_devnull);
    }
    return CUE_SUCCESS;
}

static void
handle_cunit_error(void)
{
    fprintf(stderr, "CUnit error occured: %d: %s\n",
            CU_get_error(), CU_get_error_msg());
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    int ret;
    CU_pTest test;
    CU_pSuite suite;
    CU_TestInfo tests[] = {
        {"test_bad_open_mode", test_bad_open_mode},
        {"test_open_io_errors", test_open_io_errors},
        {"test_write_errors", test_write_errors},
        {"test_strerror", test_strerror},
        {"test_empty_key", test_empty_key},
        {"test_get_write_mode", test_get_write_mode},
        {"test_put_read_mode", test_put_read_mode},
        {"test_different_key_length", test_different_key_length},
        {"test_different_key_length_reverse", test_different_key_length_reverse},
        {"test_mixed_keys", test_mixed_keys},
        {"test_duplicate_key", test_duplicate_key},
        {"test_missing_key", test_missing_key},
        {"test_bad_types", test_bad_types},
        {"test_simple_round_trip", test_simple_round_trip},
        {"test_empty_file", test_empty_file},
        {"test_bad_types", test_bad_type},
        {"test_bad_filesizes", test_bad_filesizes},
        {"test_bad_magic_number", test_bad_magic_number},
        {"test_version_0", test_version_0},
        {"test_version_100", test_version_100},
        {"test_truncated_file", test_truncated_file},
        {"test_key_offset_outside_file", test_key_offset_outside_file},
        {"test_array_offset_outside_file", test_array_offset_outside_file},
        {"test_key_len_outside_file", test_key_len_outside_file},
        {"test_array_len_outside_file", test_array_len_outside_file},
        {"test_bad_key_start", test_bad_key_start},
        {"test_bad_array_start", test_bad_array_start},
        {"test_truncated_file_correct_size", test_truncated_file_correct_size},
        {"test_all_types_n_elements", test_all_types_n_elements},
        CU_TEST_INFO_NULL,
    };

    /* We use initialisers here as the struct definitions change between
     * versions of CUnit */
    CU_SuiteInfo suites[] = {
        {
            .pName = "kastore",
            .pInitFunc = kastore_suite_init,
            .pCleanupFunc = kastore_suite_cleanup,
            .pTests = tests
        },
        CU_SUITE_INFO_NULL,
    };
    if (CUE_SUCCESS != CU_initialize_registry()) {
        handle_cunit_error();
    }
    if (CUE_SUCCESS != CU_register_suites(suites)) {
        handle_cunit_error();
    }
    CU_basic_set_mode(CU_BRM_VERBOSE);

    if (argc == 1) {
        CU_basic_run_tests();
    } else if (argc == 2) {
        suite = CU_get_suite_by_name("kastore", CU_get_registry());
        if (suite == NULL) {
            printf("Suite not found\n");
            return EXIT_FAILURE;
        }
        test = CU_get_test_by_name(argv[1], suite);
        if (test == NULL) {
            printf("Test '%s' not found\n", argv[1]);
            return EXIT_FAILURE;
        }
        CU_basic_run_test(suite, test);
    } else {
        printf("usage: ./tests <test_name>\n");
        return EXIT_FAILURE;
    }

    ret = EXIT_SUCCESS;
    if (CU_get_number_of_tests_failed() != 0) {
        printf("Test failed!\n");
        ret = EXIT_FAILURE;
    }
    CU_cleanup_registry();
    return ret;
}
