TEMPLATE = subdirs

SUBDIRS += \
    eidos \
    core \
    QtSLiM \
    gsl \
    treerec/tskit \
    eidos_zlib

eidos.depends = gsl eidos_zlib
core.depends = gsl eidos_zlib eidos treerec/tskit
QtSLiM.depends = gsl eidos_zlib eidos core treerec/tskit


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# You may also want to set ASAN_OPTIONS, in the Run Settings section of the Project tab in Qt Creator, to
# strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
# This also enables undefined behavior sanitizing, in conjunction with ASAN, because why not.
#CONFIG += sanitizer sanitize_address sanitize_undefined
