#ifndef KASTORE_GLOBAL_H
#define KASTORE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(KASTORE_LIBRARY)
#  define KASTORESHARED_EXPORT Q_DECL_EXPORT
#else
#  define KASTORESHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // KASTORE_GLOBAL_H
