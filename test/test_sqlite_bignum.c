#include <setjmp.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cmokca needs to be explicitly included last
#include <cmocka.h>

static int open_db(void **state) {
  sqlite3 *db = NULL;
  int rc = sqlite3_open(":memory:", &db);
  assert_int_equal(rc, SQLITE_OK);
  *state = db;
  return 0;
}

static int close_db(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  sqlite3_close(db);
  return 0;
}

static void exec_ok(sqlite3 *db, const char *sql) {
  char *errmsg = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
  if (rc != SQLITE_OK) { fprintf(stderr, "SQL error: %s\n", errmsg); }
  assert_int_equal(rc, SQLITE_OK);
  sqlite3_free(errmsg);
}

static sqlite3_stmt *prepare(sqlite3 *db, const char *sql) {
  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  assert_int_equal(rc, SQLITE_OK);
  assert_non_null(stmt);
  return stmt;
}

static void test_baseline_failure(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  exec_ok(db, "CREATE TABLE t(n INTEGER)");
  // Only insert values within SQLite INTEGER range
  exec_ok(db,
          "INSERT INTO t VALUES (1000), "
          "(9223372036854775807), "   // INT64_MAX
          "(-9223372036854775808)");  // INT64_MIN

  sqlite3_stmt *stmt = prepare(db, "SELECT n FROM t ORDER BY n");
  int64_t expected[] = {INT64_MIN, 1000LL, INT64_MAX};
  int64_t found[3] = {0};
  int i = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) { found[i++] = sqlite3_column_int64(stmt, 0); }
  sqlite3_finalize(stmt);
  assert_int_equal(i, 3);
  for (int j = 0; j < 3; j++) assert_int_equal(found[j], expected[j]);
}

static void test_extension_and_features(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  // Enable extension loading before trying to load
  int rc = sqlite3_enable_load_extension(db, 1);
  assert_int_equal(rc, SQLITE_OK);
  exec_ok(db, "SELECT load_extension('sqlite_bignum.dll')");
  exec_ok(db, "CREATE TABLE bigu(u TEXT CHECK(is_u64text(u)))");
  exec_ok(db, "INSERT INTO bigu(u) VALUES (u64_to_text(9223372036854775807)), (u64_to_text(1000))");

  sqlite3_stmt *stmt = prepare(db, "SELECT u, text_to_u64(u) FROM bigu ORDER BY u");
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *text = (const char *) sqlite3_column_text(stmt, 0);
    sqlite3_int64 value = sqlite3_column_int64(stmt, 1);
    assert_int_equal(strlen(text), 20);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long) value);
    uint64_t parsed = strtoull(text, NULL, 10);
    assert_true(parsed == (uint64_t) value);
  }
  sqlite3_finalize(stmt);

  char *errmsg = NULL;
  rc = sqlite3_exec(db, "INSERT INTO bigu(u) VALUES ('42')", NULL, NULL, &errmsg);
  assert_int_not_equal(rc, SQLITE_OK);
  if (errmsg) {
    printf("Expected constraint error: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test_setup_teardown(test_baseline_failure, open_db, close_db),
                                     cmocka_unit_test_setup_teardown(test_extension_and_features, open_db, close_db)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}