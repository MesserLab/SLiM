#ifndef GSL_GLOBAL_H
#define GSL_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(GSL_LIBRARY)
#  define GSLSHARED_EXPORT Q_DECL_EXPORT
#else
#  define GSLSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // GSL_GLOBAL_H
