#include <setjmp.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cmokca needs to be explicitly included last
#include <cmocka.h>

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

static int open_db(void **state) {
  sqlite3 *db = NULL;
  int rc = sqlite3_open(":memory:", &db);
  assert_int_equal(rc, SQLITE_OK);
  *state = db;
  rc = sqlite3_enable_load_extension(db, 1);
  assert_int_equal(rc, SQLITE_OK);
  exec_ok(db, "SELECT load_extension('sqlite_bignum.dll')");
  return 0;
}

static sqlite3_stmt *prepare(sqlite3 *db, const char *sql) {
  sqlite3_stmt *stmt = NULL;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  assert_int_equal(rc, SQLITE_OK);
  assert_non_null(stmt);
  return stmt;
}

/**
 * @brief tests that the default REAL affinity used by sqlite3 fails to properly order uint64 values due to loss of precision at high values
 *
 * This test checks how SQLite's dynamic typing (INTEGER/REAL affinity) produces *incorrect sorting* and *incorrect representation* for true uint64
 * values, especially those exceeding INT64_MAX.
 *
 * Key facts:
 *   - Values <= 2^53 ("9007199254740992") can be stored/extracted precisely.
 *   - Values > 2^53 but <= INT64_MAX are still precisely stored as INTEGER.
 *   - Values > INT64_MAX (e.g. 18446744073709551615, which is UINT64_MAX)
 *     are stored as REAL (IEEE 754 double), which loses precision, so some
 *     distinct integers collapse to the same double value, and sorting/rounding
 *     becomes *wrong*.
 *
 * What actually happens on default SQLite when we ORDER BY n:
 *   [ "9007199254740992",
 *     "9007199254740993",      // May NOT be distinguishable in output!
 *     "9223372036854775807",
 *     "9223372036854775808",
 *     "18446744073709551615"   // Appears as "18446744073709551616" (!)
 *   ]
 *
 * But the *true* uint64 numeric order should be:
 *   [ 9007199254740992,
 *     9007199254740993,
 *     9223372036854775807,
 *     9223372036854775808,
 *     18446744073709551615
 *   ]
 *
 * The crucial bugs exposed:
 *   - UINT64_MAX can't be represented as double, is rounded, and sorts *as if it was* 18446744073709551616.
 *   - 9007199254740993 ("2^53+1") cannot be represented in double at all; it sorts as if it were 9007199254740992.
 *   - Thus, both ordering and textual representation are broken for high uint64s.
 *
 * The test asserts that "demangled" output (as double-printed integer) does not match
 * the original uint64 decimal input for at least one row, highlighting precision loss.
 **/
static void test_wrong_ordering_on_u64(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  exec_ok(db, "CREATE TABLE t(n INTEGER)");

  exec_ok(db,
          "INSERT INTO t(n) VALUES "
          "(9223372036854775807),"   // INT64_MAX, stored as INTEGER
          "(9223372036854775808),"   // INT64_MAX+1, stored as REAL (lossless)
          "(18446744073709551615),"  // UINT64_MAX, stored as REAL (loses precision)
          "(9007199254740992),"      // 2^53, precisely stored as REAL (last exact double-int)
          "(9007199254740993)"       // 2^53 + 1, NOT exact in double
  );

  sqlite3_stmt *stmt = prepare(db, "SELECT n FROM t ORDER BY n");

  char results[5][32] = {{0}};
  int i = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    snprintf(results[i], sizeof(results[i]), "%s", sqlite3_column_text(stmt, 0));
    i++;
  }
  sqlite3_finalize(stmt);
  assert_int_equal(i, 5);

  static const char *expected[] = {"9007199254740992", "9007199254740993", "9223372036854775807", "9223372036854775808", "18446744073709551615"};

  printf("Returned row order from SQLite and decimal interpretation:\n");
  int problems = 0;
  for (int j = 0; j < 5; ++j) {
    const char *sqlite_out = results[j];
    const char *true_val = expected[j];

    // Get the "de-scientified" version by parsing as double then printing full integer
    char demangled[32] = {0};
    char *endptr = NULL;
    double dval = strtod(sqlite_out, &endptr);
    if (endptr && *endptr == '\0') {
      // this will round for values beyond double's exact span!
      snprintf(demangled, sizeof(demangled), "%.0f", dval);
    } else {
      snprintf(demangled, sizeof(demangled), "<not a number>");
    }

    printf("[%d]: %-24s | as uint64: %-22s | expected: %s\n", j, sqlite_out, demangled, true_val);

    if (strcmp(demangled, true_val) != 0) {
      printf("  !!! MISMATCH at row %d: got '%s', as uint64 '%s', expected '%s'\n", j, sqlite_out, demangled, true_val);
      problems++;
    }
  }

  assert_true(problems > 0 && "Expected precision loss with high uint64 values in SQLite");
}

static void test_text_ordering_fails_without_collation(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  exec_ok(db, "CREATE TABLE t(val TEXT)");
  exec_ok(db,
          "INSERT INTO t(val) VALUES "
          "('18446744073709551615'),"  // u64 max
          "('9223372036854775808'),"
          "('100'),"
          "('2')");

  // Natural text ordering (should not match numeric)
  sqlite3_stmt *stmt = prepare(db, "SELECT val FROM t ORDER BY val");
  const char *expected_wrong_order[] = {"100", "18446744073709551615", "2", "9223372036854775808"};
  int i = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *txt = (const char *) sqlite3_column_text(stmt, 0);
    assert_string_equal(txt, expected_wrong_order[i++]);
  }
  sqlite3_finalize(stmt);
  assert_int_equal(i, 4);
}

static void test_u64_edgecases(void **state) {
  sqlite3 *db = *(sqlite3 **) state;
  exec_ok(db, "CREATE TABLE t(n TEXT COLLATE U64TEXT);");

  exec_ok(db,
          "INSERT INTO t(n) VALUES "
          "(u64_to_text('9223372036854775807')),"   // INT64_MAX
          "(u64_to_text('9223372036854775808')),"   // INT64_MAX + 1
          "(u64_to_text('18446744073709551615')),"  // UINT64_MAX
          "(u64_to_text('9007199254740992')),"      // 2^53
          "(u64_to_text('9007199254740993'))"       // 2^53 + 1
  );

  // Test correct order using your U64TEXT collation
  sqlite3_stmt *stmt = prepare(db, "SELECT n FROM t ORDER BY n");

  const char *expected[] = {
      "00009007199254740992",  // 9007199254740992
      "00009007199254740993",  // 9007199254740993
      "09223372036854775807",  // 9223372036854775807
      "09223372036854775808",  // 9223372036854775808
      "18446744073709551615",  // 18446744073709551615
  };

  for (int i = 0; i < 5; ++i) {
    assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
    const char *val = (const char *) sqlite3_column_text(stmt, 0);
    assert_string_equal(val, expected[i]);
  }
  assert_int_equal(sqlite3_step(stmt), SQLITE_DONE);

  sqlite3_finalize(stmt);
}

static void test_u64text_display_function(void **state) {
  sqlite3 *db = *(sqlite3 **) state;

  struct {
    const char *input;
    const char *expected;
    const char *description;
  } test_cases[] = {
      // Basic zero removal
      {"00000000000000000042", "42", "Simple number with leading zeros"},
      {"00000000000000000001", "1", "Single digit with leading zeros"},
      {"00000000000000000000", "0", "All zeros should display as single zero"},

      // Edge cases
      {"18446744073709551615", "18446744073709551615", "UINT64_MAX (no leading zeros)"},
      {"09223372036854775807", "9223372036854775807", "INT64_MAX with one leading zero"},
      {"00009007199254740992", "9007199254740992", "2^53 with leading zeros"},

      // Maximum leading zeros
      {"00000000000000000123", "123", "Many leading zeros"},
      {"01234567890123456789", "1234567890123456789", "19 digits with one leading zero"},

      // All digits from 1-19 length results
      {"00000000000000000001", "1", "1 digit result"},
      {"00000000000000000012", "12", "2 digit result"},
      {"00000000000000000123", "123", "3 digit result"},
      {"00000000000001234567", "1234567", "7 digit result"},
      {"00000012345678901234", "12345678901234", "14 digit result"},
      {"12345678901234567890", "12345678901234567890", "20 digit result (no leading zeros)"},
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT u64text_display('%s')", test_cases[i].input);

    sqlite3_stmt *stmt = prepare(db, sql);
    assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);

    const char *result = (const char *) sqlite3_column_text(stmt, 0);
    printf("Test %zu: %s\n", i + 1, test_cases[i].description);
    printf("  Input: '%s'\n", test_cases[i].input);
    printf("  Expected: '%s'\n", test_cases[i].expected);
    printf("  Got: '%s'\n", result);

    assert_string_equal(result, test_cases[i].expected);
    sqlite3_finalize(stmt);
  }

  printf("\nTesting error/edge cases:\n");

  // NULL input
  sqlite3_stmt *stmt = prepare(db, "SELECT u64text_display(NULL)");
  assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
  assert_int_equal(sqlite3_column_type(stmt, 0), SQLITE_NULL);
  sqlite3_finalize(stmt);
  printf("  NULL input -> NULL result: ✓\n");

  // non-TEXT input (should return NULL)
  stmt = prepare(db, "SELECT u64text_display(42)");
  assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
  assert_int_equal(sqlite3_column_type(stmt, 0), SQLITE_NULL);
  sqlite3_finalize(stmt);
  printf("  INTEGER input -> NULL result: ✓\n");

  // invalid u64text format (should return original string)
  stmt = prepare(db, "SELECT u64text_display('invalid_format')");
  assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
  const char *result = (const char *) sqlite3_column_text(stmt, 0);
  assert_string_equal(result, "invalid_format");
  sqlite3_finalize(stmt);
  printf("  Invalid format -> original string: ✓\n");

  // wrong length (should return original string)
  stmt = prepare(db, "SELECT u64text_display('123456789')");
  assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
  result = (const char *) sqlite3_column_text(stmt, 0);
  assert_string_equal(result, "123456789");
  sqlite3_finalize(stmt);
  printf("  Wrong length -> original string: ✓\n");

  // non-numeric characters (should return original string)
  stmt = prepare(db, "SELECT u64text_display('1234567890abcdef1234')");
  assert_int_equal(sqlite3_step(stmt), SQLITE_ROW);
  result = (const char *) sqlite3_column_text(stmt, 0);
  assert_string_equal(result, "1234567890abcdef1234");
  sqlite3_finalize(stmt);
  printf("  Non-numeric characters -> original string: ✓\n");
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test_setup_teardown(test_wrong_ordering_on_u64, open_db, close_db),
                                     cmocka_unit_test_setup_teardown(test_text_ordering_fails_without_collation, open_db, close_db),
                                     cmocka_unit_test_setup_teardown(test_u64_edgecases, open_db, close_db),
                                     cmocka_unit_test_setup_teardown(test_u64text_display_function, open_db, close_db)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}