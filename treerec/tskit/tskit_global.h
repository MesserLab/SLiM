#ifndef TSKIT_GLOBAL_H
#define TSKIT_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(TSKIT_LIBRARY)
#  define TSKITSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TSKITSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // TSKIT_GLOBAL_H
