#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define U64TEXT_WIDTH 20

/**
 * @brief Check if the string is a valid zero-padded 20-digit u64
 * @param text the string to validate
 * @param len length of the string
 * @return
 */
static int is_valid_u64text(const char *text, int len) {
  if (len != U64TEXT_WIDTH) return 0;
  for (int i = 0; i < U64TEXT_WIDTH; ++i) {
    if (text[i] < '0' || text[i] > '9') return 0;
  }

  return 1;
}

/**
 * @brief Collation for zero-padded u64 text
 *
 * Example usage:
 *
 * CREATE TABLE tokens (balance TEXT COLLATE U64TEXT);
 *
 * @param unused
 * @param len1
 * @param str1
 * @param len2
 * @param str2
 * @return
 */
static int u64text_collation(void *unused, int len1, const void *str1, int len2, const void *str2) {
  // Require exact width 20, else fallback to length comparison
  if (len1 != U64TEXT_WIDTH || len2 != U64TEXT_WIDTH) { return len1 - len2; }
  return memcmp(str1, str2, U64TEXT_WIDTH);
}

/**
 * @brief Convert INTEGER or TEXT to zero-padded 20-digit TEXT
 *
 * @param ctx
 * @param argc
 * @param argv
 */
static void u64_to_text_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc < 1) {
    sqlite3_result_null(ctx);
    return;
  }
  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    sqlite3_result_null(ctx);
    return;
  }

  char buf[U64TEXT_WIDTH + 1];
  switch (sqlite3_value_type(argv[0])) {
    case SQLITE_INTEGER: {
      uint64_t u = (uint64_t) sqlite3_value_int64(argv[0]);
      snprintf(buf, sizeof(buf), "%020" PRIu64, u);
      sqlite3_result_text(ctx, buf, U64TEXT_WIDTH, SQLITE_TRANSIENT);
      break;
    }
    case SQLITE_TEXT: {
      const char *text = (const char *) sqlite3_value_text(argv[0]);
      char *end = NULL;
      uint64_t val = strtoull(text, &end, 10);
      if (end && *end == '\0') {
        snprintf(buf, sizeof(buf), "%020" PRIu64, val);
        sqlite3_result_text(ctx, buf, U64TEXT_WIDTH, SQLITE_TRANSIENT);
      } else {
        sqlite3_result_error(ctx, "Invalid u64 string", -1);
      }
      break;
    }
    default:
      sqlite3_result_error(ctx, "Unsupported type for u64_to_text", -1);
  }
}

/**
 * @brief Parse and output uint64 from zero-padded TEXT or INTEGER (for application use)
 *
 * @param ctx
 * @param argc
 * @param argv
 */
static void text_to_u64_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc < 1) {
    sqlite3_result_null(ctx);
    return;
  }

  switch (sqlite3_value_type(argv[0])) {
    case SQLITE_TEXT: {
      const unsigned char *text = sqlite3_value_text(argv[0]);
      int len = sqlite3_value_bytes(argv[0]);
      if (!is_valid_u64text((const char *) text, len)) {
        sqlite3_result_error(ctx, "Invalid u64 text format", -1);
        return;
      }
      char buf[U64TEXT_WIDTH + 1];
      memcpy(buf, text, U64TEXT_WIDTH);
      buf[U64TEXT_WIDTH] = 0;
      uint64_t val = strtoull(buf, NULL, 10);
      sqlite3_result_int64(ctx, (sqlite3_int64) val);
      break;
    }
    case SQLITE_INTEGER:
      sqlite3_result_int64(ctx, sqlite3_value_int64(argv[0]));
      break;
    case SQLITE_NULL:
      sqlite3_result_null(ctx);
      break;
    case SQLITE_FLOAT:
      sqlite3_result_error(ctx, "Values larger than double need to be passed as a string", -1);
      break;
    default:
      sqlite3_result_error(ctx, "Unsupported type for u64_to_text", -1);
  }
}

/**
 * @brief validation function for triggers or CHECK (bool)
 *
 * @param ctx
 * @param argc
 * @param argv
 */
static void is_u64text_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  if (argc < 1 || sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
    sqlite3_result_int(ctx, 0);
    return;
  }
  const char *text = (const char *) sqlite3_value_text(argv[0]);
  int len = sqlite3_value_bytes(argv[0]);
  sqlite3_result_int(ctx, is_valid_u64text(text, len));
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
  SQLITE_EXTENSION_INIT2(pApi);
  // Collation: for CREATE TABLE ... big_num TEXT COLLATE U64TEXT
  sqlite3_create_collation(db, "U64TEXT", SQLITE_UTF8, 0, u64text_collation);
  // Function: u64_to_text(u64) -> TEXT (padded)
  sqlite3_create_function(db, "u64_to_text", 1, SQLITE_UTF8, 0, u64_to_text_func, 0, 0);
  // Function: text_to_u64(TEXT) -> INTEGER (for extracting value)
  sqlite3_create_function(db, "text_to_u64", 1, SQLITE_UTF8, 0, text_to_u64_func, 0, 0);
  // Function: is_u64text(TEXT) -> bool
  sqlite3_create_function(db, "is_u64text", 1, SQLITE_UTF8, 0, is_u64text_func, 0, 0);
  return SQLITE_OK;
}
