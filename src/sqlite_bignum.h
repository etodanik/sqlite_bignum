#ifndef SQLITE_BIGNUM_H
#define SQLITE_BIGNUM_H

#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define SQLITE_BIGNUM_API __declspec(dllexport)
#else
#define SQLITE_BIGNUM_API
#endif

SQLITE_BIGNUM_API int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

#ifdef __cplusplus
}
#endif

#endif // SQLITE_BIGNUM_H
