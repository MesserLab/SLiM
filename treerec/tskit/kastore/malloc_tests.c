/* These tests simulate failures in malloc by wrapping the original definitions
 * of malloc using the linker and failing after a predefined number of calls */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <float.h>
#include <stdbool.h>

#include "kastore.h"
#include <CUnit/Basic.h>

char * _tmp_file_name;

/* Wrappers used to check for correct error handling. Must be linked with the
 * link option, e.g., -Wl,--wrap=malloc. See 'ld' manpage for details.
 */

/* Consider malloc and realloc as the same thing in terms of the counters */
void *__wrap_malloc(size_t);
void *__real_malloc(size_t);
void *__wrap_realloc(void *, size_t);
void *__real_realloc(void *, size_t);
void *__wrap_calloc(size_t, size_t);
void *__real_calloc(size_t, size_t);

int _malloc_fail_at = -1;
int _malloc_count = 0;

void *
__wrap_malloc(size_t c)
{
    if (_malloc_count == _malloc_fail_at) {
        return NULL;
    }
    _malloc_count++;
    return __real_malloc(c);
}

void *
__wrap_realloc(void *p, size_t c)
{
    if (_malloc_count == _malloc_fail_at) {
        return NULL;
    }
    _malloc_count++;
    return __real_realloc(p, c);
}

void *
__wrap_calloc(size_t s, size_t c)
{
    if (_malloc_count == _malloc_fail_at) {
        return NULL;
    }
    _malloc_count++;
    return __real_calloc(s, c);
}

static void
test_write(void)
{
    kastore_t store;
    int ret = 0;
    int32_t array[] = {1, 2, 3, 4};
    bool done;

    /* Make sure the failing malloc setup works first */
    _malloc_fail_at = 0;
    _malloc_count = 0;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts_int32(&store, "array", array, 4, 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    done = false;
    while (! done) {
        ret = kastore_open(&store, _tmp_file_name, "w", 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        _malloc_count = 0;
        _malloc_fail_at++;
        ret = kastore_puts_int32(&store, "array", array, 4, 0);
        if (ret == 0) {
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
        }
        ret = kastore_close(&store);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
    }
    /* Make sure we reset this for other functions */
    _malloc_fail_at = -1;
}

static void
test_append(void)
{
    kastore_t store;
    int ret = 0;
    int32_t array[] = {1, 2, 3, 4};
    bool done;

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts_int32(&store, "array", array, 4, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    _malloc_fail_at = 0;
    done = false;
    while (! done) {
        _malloc_count = 0;
        ret = kastore_open(&store, _tmp_file_name, "a", 0);
        if (ret == 0) {
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
        }
        ret = kastore_close(&store);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        _malloc_fail_at++;
    }
    CU_ASSERT(_malloc_fail_at > 1);
    /* Make sure we reset this for other functions */
    _malloc_fail_at = -1;
}


static void
test_open_read(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";
    bool done;
    size_t f;
    int flags[] = {0, KAS_NO_MMAP};

    /* Make sure the failing malloc setup works first */
    _malloc_fail_at = 0;
    _malloc_count = 0;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    for (f = 0; f < sizeof(flags) / sizeof(*flags); f++) {
        _malloc_fail_at = 0;
        done = false;
        while (! done) {
            _malloc_count = 0;
            ret = kastore_open(&store, filename, "r", flags[f]);
            if (ret == 0) {
                done = true;
            } else {
                CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
            }
            ret = kastore_close(&store);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            _malloc_fail_at++;
        }
    }

    /* Make sure we reset this for other functions */
    _malloc_fail_at = -1;
}

static void
test_read(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";
    bool done;
    int8_t *array;
    size_t size;

    /* Make sure the failing malloc setup works first */
    _malloc_fail_at = 0;
    _malloc_count = 0;
    ret = kastore_open(&store, filename, "r", KAS_NO_MMAP);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    _malloc_fail_at = -1;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    _malloc_fail_at = 0;
    done = false;
    while (! done) {
        _malloc_count = 0;
        ret = kastore_gets_int8(&store, "int8", &array, &size);
        if (ret == 0) {
            CU_ASSERT_EQUAL_FATAL(size, 1);
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_NO_MEMORY);
        }
        _malloc_fail_at++;
    }
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Make sure we reset this for other functions */
    _malloc_fail_at = -1;
}

/*=================================================
  Test suite management
  =================================================
*/

static int
kastore_suite_init(void)
{
    int fd;
    static char template[] = "/tmp/kas_c_test_XXXXXX";

    _tmp_file_name = NULL;

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
    return CUE_SUCCESS;
}

static int
kastore_suite_cleanup(void)
{
    if (_tmp_file_name != NULL) {
        unlink(_tmp_file_name);
        free(_tmp_file_name);
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
        {"test_write", test_write},
        {"test_append", test_append},
        {"test_open_read", test_open_read},
        {"test_read", test_read},
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
        printf("usage: ./malloc_tests <test_name>\n");
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
