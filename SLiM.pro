TEMPLATE = subdirs

SUBDIRS += \
    eidos \
    core \
    QtSLiM \
    gsl \
    treerec/tskit

eidos.depends = gsl
core.depends = gsl eidos treerec/tskit
QTSLiM.depends = gsl eidos treerec/tskit core
