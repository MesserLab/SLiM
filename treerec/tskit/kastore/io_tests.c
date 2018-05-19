/* These tests simulate failures in IO by wrapping the original definitions
 * of the fwrite, etc using the linker and failing after a predefined number
 * of calls */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <float.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "kastore.h"
#include <CUnit/Basic.h>

char * _tmp_file_name;

/* Wrappers used to check for correct error handling. Must be linked with the
 * link option, e.g., -Wl,--wrap=fwrite. See 'ld' manpage for details.
 */

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t __real_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int _fwrite_fail_at = -1;
int _fwrite_count = 0;
size_t
__wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (_fwrite_fail_at == _fwrite_count) {
        return 0;
    }
    _fwrite_count++;
    return __real_fwrite(ptr, size, nmemb, stream);
}

size_t __wrap_fread(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t __real_fread(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int _fread_fail_at = -1;
int _fread_count = 0;
size_t
__wrap_fread(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (_fread_fail_at == _fread_count) {
        return 0;
    }
    _fread_count++;
    return __real_fread(ptr, size, nmemb, stream);
}

int __wrap_fseek(FILE *stream, long offset, int whence);
int __real_fseek(FILE *stream, long offset, int whence);
int _fseek_fail_at = -1;
int _fseek_count = 0;
int
__wrap_fseek(FILE *stream, long offset, int whence)
{
    if (_fseek_fail_at == _fseek_count) {
        return -1;
    }
    _fseek_count++;
    return __real_fseek(stream, offset, whence);
}

int __wrap_fclose(FILE *stream);
int __real_fclose(FILE *stream);
int _fclose_fail_at = -1;
int _fclose_count = 0;
int
__wrap_fclose(FILE *stream)
{
    if (_fclose_fail_at == _fclose_count) {
        /* Close the file anyway to avoid a memory leak, but return failure. */
        __real_fclose(stream);
        return EOF;
    }
    _fclose_count++;
    return __real_fclose(stream);
}

void *__wrap_mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *__real_mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int _mmap_fail_at = -1;
int _mmap_count = 0;
void *
__wrap_mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (_mmap_fail_at == _mmap_count) {
        return MAP_FAILED;
    }
    _mmap_count++;
    return __real_mmap64(addr, length, prot, flags, fd, offset);
}

void *__wrap_stat64(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
void *__real_stat64(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int _stat_fail_at = -1;
int _stat_count = 0;
void *
__wrap_stat64(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (_stat_fail_at == _stat_count) {
        return MAP_FAILED;
    }
    _stat_count++;
    return __real_stat64(addr, length, prot, flags, fd, offset);
}

/* Tests */

static void
test_write_empty(void)
{
    kastore_t store;
    int ret = 0;

    _fwrite_fail_at = 0;
    _fwrite_count = 0;
    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);

   /* Make sure we reset this for other functions */
    _fwrite_fail_at = -1;
}

static void
test_write(void)
{
    kastore_t store;
    int ret = 0;
    int8_t array[] = {1};
    bool done;

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    _fwrite_fail_at = 0;
    _fwrite_count = 0;
    done = false;
    while (! done) {
        ret = kastore_open(&store, _tmp_file_name, "w", 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        _fwrite_count = 0;
        ret = kastore_puts_int8(&store, "a", array, 1, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = kastore_close(&store);
        if (ret == 0) {
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
        }
        _fwrite_fail_at++;
    }
    CU_ASSERT_TRUE(_fwrite_fail_at > 1);
    /* Make sure we reset this for other functions */
    _fwrite_fail_at = -1;
}

static void
test_write_fclose(void)
{
    kastore_t store;
    int ret = 0;
    int8_t array[] = {1};
    bool done;

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    _fclose_fail_at = 0;
    _fclose_count = 0;
    done = false;
    while (! done) {
        ret = kastore_open(&store, _tmp_file_name, "w", 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        _fclose_count = 0;
        ret = kastore_puts_int8(&store, "a", array, 1, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = kastore_close(&store);
        if (ret == 0) {
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
        }
        _fclose_fail_at++;
    }
    CU_ASSERT_TRUE(_fclose_fail_at > 1);
    /* Make sure we reset this for other functions */
    _fclose_fail_at = -1;
}

static void
test_read_fclose(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";

    _fclose_fail_at = 0;
    _fclose_count = 0;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);

   _fclose_fail_at = -1;
}

static void
test_append_fclose(void)
{
    kastore_t store;
    int ret = 0;
    int8_t array[] = {1};
    bool done;
    size_t index = 0;
    const char * keys[] = {"b", "c"};

    ret = kastore_open(&store, _tmp_file_name, "w", 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_puts_int8(&store, "a", array, 1, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    _fclose_fail_at = 0;
    _fclose_count = 0;
    done = false;
    while (! done) {
        ret = kastore_open(&store, _tmp_file_name, "a", 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        _fclose_count = 0;
        CU_ASSERT_FATAL(index < sizeof(keys) / (sizeof(*keys)));
        ret = kastore_puts_int8(&store, keys[index], array, 1, 0);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        ret = kastore_close(&store);
        if (ret == 0) {
            done = true;
        } else {
            CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
        }
        _fclose_fail_at++;
        index++;
    }
    CU_ASSERT_TRUE(_fclose_fail_at > 1);
    /* Make sure we reset this for other functions */
    _fclose_fail_at = -1;
}

static void
test_open_read_fread(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";
    bool done;
    size_t f;
    int flags[] = {0, KAS_NO_MMAP};

    _fread_fail_at = 0;
    /* Make sure the failing fread setup works first */
    _fread_count = 0;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_FILE_FORMAT);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* If we set errno before calling, we should get an IO error back */
    _fread_count = 0;
    errno = ENOENT;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    errno = 0;
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* keep increasing fail_at until we pass. This should catch all possible
     * places at which we can fail. */
    for (f = 0; f < sizeof(flags) / sizeof(*flags); f++) {
        _fread_fail_at = 0;
        done = false;
        while (! done) {
            _fread_count = 0;
            ret = kastore_open(&store, filename, "r", flags[f]);
            if (ret == 0) {
                done = true;
            } else {
                CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_BAD_FILE_FORMAT);
            }
            ret = kastore_close(&store);
            CU_ASSERT_EQUAL_FATAL(ret, 0);
            _fread_fail_at++;
        }
    }

    /* Make sure we reset this for other functions */
    _fread_fail_at = -1;
}

static void
test_open_read_fseek(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";

    /* fseek is only called in the non mmap code path */
    _fseek_fail_at = 0;
    _fseek_count = 0;
    ret = kastore_open(&store, filename, "r", KAS_NO_MMAP);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    _fseek_fail_at = -1;
}

static void
test_open_read_mmap(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";

    _mmap_fail_at = 0;
    _mmap_count = 0;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    _mmap_fail_at = -1;
}

static void
test_open_read_stat(void)
{
    kastore_t store;
    int ret = 0;
    const char *filename = "test-data/v1/all_types_1_elements.kas";

    _stat_fail_at = 0;
    _stat_count = 0;
    ret = kastore_open(&store, filename, "r", 0);
    CU_ASSERT_EQUAL_FATAL(ret, KAS_ERR_IO);
    ret = kastore_close(&store);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    _stat_fail_at = -1;
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
        {"test_write_empty", test_write_empty},
        {"test_write", test_write},
        {"test_write_fclose", test_write_fclose},
        {"test_read_fclose", test_read_fclose},
        {"test_append_fclose", test_append_fclose},
        {"test_open_read_fread", test_open_read_fread},
        {"test_open_read_fseek", test_open_read_fseek},
        {"test_open_read_mmap", test_open_read_mmap},
        {"test_open_read_stat", test_open_read_stat},
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
        printf("usage: ./io_tests <test_name>\n");
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
