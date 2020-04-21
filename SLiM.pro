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
