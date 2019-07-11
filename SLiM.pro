TEMPLATE = subdirs

SUBDIRS += \
    eidos \
    core \
    QtSLiM \
    gsl \
    treerec/tskit/kastore \
    treerec/tskit

eidos.depends = gsl
treerec/tskit.depends = treerec/tskit/kastore
core.depends = eidos treerec/tskit
QTSLiM.depends = core
