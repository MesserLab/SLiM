TEMPLATE = subdirs

SUBDIRS += \
    eidos \
    core \
    QtSLiM \
    gsl \
    treerec/tskit \
    eidos_zlib

eidos.depends = gsl
core.depends = gsl eidos treerec/tskit
QtSLiM.depends = gsl eidos core treerec/tskit
