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
# CONFIG += sanitizer sanitize_address
