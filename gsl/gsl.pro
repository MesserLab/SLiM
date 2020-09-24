#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T22:38:01
#
#-------------------------------------------------

QT       -= core gui

TEMPLATE = lib
CONFIG += staticlib


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# You may also want to set ASAN_OPTIONS, in the Run Settings section of the Project tab in Qt Creator, to
# strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
# This also enables undefined behavior sanitizing, in conjunction with ASAN, because why not.
#CONFIG += sanitizer sanitize_address sanitize_undefined


CONFIG -= qt
CONFIG += object_parallel_to_source
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3

# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CFLAGS += -Wno-unknown-pragmas -Wno-attributes -Wno-unused-parameter -Wno-unused-but-set-parameter
}

INCLUDEPATH = . ./blas ./block ./cblas ./cdf ./complex ./err ./linalg ./matrix ./randist ./rng ./specfunc ./sys ./vector


# prevent link dependency cycles
QMAKE_LFLAGS += $$QMAKE_LFLAGS_NOUNDEF


SOURCES += \
    blas/blas.c \
    block/init.c \
    cblas/ddot.c \
    cblas/dgemv.c \
    cblas/dtrmv.c \
    cblas/dtrsv.c \
    cblas/xerbla.c \
    cdf/gauss.c \
    cdf/gaussinv.c \
    cdf/tdist.c \
    complex/inline.c \
    complex/math.c \
    err/error.c \
    err/message.c \
    err/stream.c \
    linalg/cholesky.c \
    matrix/copy.c \
    matrix/init.c \
    matrix/matrix.c \
    matrix/rowcol.c \
    matrix/submatrix.c \
    matrix/swap.c \
    randist/beta.c \
    randist/binomial_tpe.c \
    randist/cauchy.c \
    randist/discrete.c \
    randist/exponential.c \
    randist/gamma.c \
    randist/gauss.c \
    randist/gausszig.c \
    randist/geometric.c \
    randist/lognormal.c \
    randist/multinomial.c \
    randist/mvgauss.c \
    randist/poisson.c \
    randist/shuffle.c \
    randist/weibull.c \
    rng/inline.c \
    rng/mt.c \
    rng/rng.c \
    rng/taus.c \
    specfunc/beta.c \
    specfunc/elementary.c \
    specfunc/erfc.c \
    specfunc/exp.c \
    specfunc/expint.c \
    specfunc/gamma_inc.c \
    specfunc/gamma.c \
    specfunc/log.c \
    specfunc/pow_int.c \
    specfunc/psi.c \
    specfunc/trig.c \
    specfunc/zeta.c \
    sys/coerce.c \
    sys/fdiv.c \
    sys/infnan.c \
    sys/minmax.c \
    sys/pow_int.c \
    vector/init.c \
    vector/oper.c \
    vector/vector.c

HEADERS += \
    build.h \
    config.h \
    gsl_errno.h \
    gsl_inline.h \
    gsl_machine.h \
    gsl_math.h \
    gsl_minmax.h \
    gsl_nan.h \
    gsl_pow_int.h \
    gsl_precision.h \
    gsl_types.h \
    gsl_version.h \
    blas/gsl_blas_types.h \
    blas/gsl_blas.h \
    block/gsl_block_double.h \
    block/gsl_block.h \
    block/gsl_check_range.h \
    cblas/cblas.h \
    cblas/error_cblas_l2.h \
    cblas/error_cblas.h \
    cblas/gsl_cblas.h \
    cblas/source_dot_r.h \
    cblas/source_gemv_r.h \
    cblas/source_trmv_r.h \
    cblas/source_trsv_r.h \
    cdf/gsl_cdf.h \
    cdf/rat_eval.h \
    complex/gsl_complex_math.h \
    complex/gsl_complex.h \
    err/gsl_message.h \
    linalg/gsl_linalg.h \
    matrix/gsl_matrix_double.h \
    matrix/gsl_matrix.h \
    matrix/view.h \
    randist/gsl_randist.h \
    rng/gsl_rng.h \
    specfunc/chebyshev.h \
    specfunc/check.h \
    specfunc/error.h \
    specfunc/eval.h \
    specfunc/gsl_sf_elementary.h \
    specfunc/gsl_sf_erf.h \
    specfunc/gsl_sf_exp.h \
    specfunc/gsl_sf_expint.h \
    specfunc/gsl_sf_gamma.h \
    specfunc/gsl_sf_log.h \
    specfunc/gsl_sf_pow_int.h \
    specfunc/gsl_sf_psi.h \
    specfunc/gsl_sf_result.h \
    specfunc/gsl_sf_trig.h \
    specfunc/gsl_sf_zeta.h \
    sys/gsl_sys.h \
    vector/gsl_vector_double.h \
    vector/gsl_vector.h


