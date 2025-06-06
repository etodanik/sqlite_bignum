#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static void u64_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 1) {
    sqlite3_result_null(ctx);
    return;
  }

  switch (sqlite3_value_type(argv[0])) {
  case SQLITE_INTEGER:
    sqlite3_result_int64(ctx, (sqlite3_int64)((uint64_t)sqlite3_value_int64(argv[0])));
    break;
  case SQLITE_TEXT: {
    const char *text = (const char *)sqlite3_value_text(argv[0]);
    char *end = NULL;
    uint64_t val = strtoull(text, &end, 10);
    if (end && *end == '\0') {
      sqlite3_result_int64(ctx, (sqlite3_int64)val); // lossy if > 2^63-1
    } else {
      sqlite3_result_error(ctx, "Invalid u64 string", -1);
    }
    break;
  }
  default:
    sqlite3_result_null(ctx);
  }
}

static void u64_cmp_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc != 2) {
    sqlite3_result_null(ctx);
    return;
  }

  uint64_t a = strtoull((const char *)sqlite3_value_text(argv[0]), NULL, 10);
  uint64_t b = strtoull((const char *)sqlite3_value_text(argv[1]), NULL, 10);

  int result = (a < b) ? -1 : (a > b ? 1 : 0);
  sqlite3_result_int(ctx, result);
}

int sqlite3_u64ext_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
  SQLITE_EXTENSION_INIT2(pApi);
  sqlite3_create_function(db, "u64", 1, SQLITE_UTF8, 0, u64_func, 0, 0);
  sqlite3_create_function(db, "u64_cmp", 2, SQLITE_UTF8, 0, u64_cmp_func, 0, 0);
  return SQLITE_OK;
}
