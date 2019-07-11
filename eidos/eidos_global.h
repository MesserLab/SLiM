#ifndef EIDOS_GLOBAL_H
#define EIDOS_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(EIDOS_LIBRARY)
#  define EIDOSSHARED_EXPORT Q_DECL_EXPORT
#else
#  define EIDOSSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // EIDOS_GLOBAL_H
