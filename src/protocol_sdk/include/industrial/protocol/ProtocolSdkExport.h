#pragma once

#include <QtCore/qglobal.h>

#if defined(PROTOCOL_SDK_LIBRARY)
#  define PROTOCOL_SDK_EXPORT Q_DECL_EXPORT
#else
#  define PROTOCOL_SDK_EXPORT Q_DECL_IMPORT
#endif
