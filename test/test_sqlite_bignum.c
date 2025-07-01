#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
// cmocka requires setjmp (gc) and stddef, stdarg (clang) to be included before it
#include <cmocka.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>

typedef struct {
  sqlite3* database;
} test_data_t;

static void execute_sql_statement(sqlite3* database, const char* sql_query) {
  char* error_message = NULL;
  int result_code = sqlite3_exec(database, sql_query, 0, 0, &error_message);
  if (result_code != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", error_message);
    sqlite3_free(error_message);
    assert_int_equal(result_code, SQLITE_OK);
  }
}

static int print_query_results(void* unused, int column_count, char** column_values, char** column_names) {
  for (int i = 0; i < column_count; i++) { printf("%s = %s\n", column_names[i], column_values[i] ? column_values[i] : "NULL"); }
  printf("\n");
  return 0;
}

static int group_setup(void** state) {
  static test_data_t test_data;

  int result_code = sqlite3_open(":memory:", &test_data.database);
  if (result_code != SQLITE_OK) { return -1; }

  execute_sql_statement(test_data.database, "CREATE TABLE big_numbers (id INTEGER PRIMARY KEY, big_num UNSIGNED BIG INT)");
  execute_sql_statement(test_data.database, "INSERT INTO big_numbers VALUES (1, 1000)");
  // Max int64
  execute_sql_statement(test_data.database, "INSERT INTO big_numbers VALUES (2, 9223372036854775807)");
  // Max int64 + 1
  execute_sql_statement(test_data.database, "INSERT INTO big_numbers VALUES (3, 9223372036854775808)");
  // Max uint64
  execute_sql_statement(test_data.database, "INSERT INTO big_numbers VALUES (4, 18446744073709551615)");

  *state = &test_data;
  return 0;
}

static int group_teardown(void** state) {
  test_data_t* test_data = (test_data_t*) *state;
  sqlite3_close(test_data->database);
  return 0;
}

static void test_order_by_query(void** state) {
  test_data_t* test_data = (test_data_t*) *state;
  sqlite3* database = test_data->database;

  printf("Query with ORDER BY (should show incorrect ordering):\n");

  sqlite3_stmt* statement = NULL;
  const char* order_query = "SELECT id, big_num, typeof(big_num) FROM big_numbers ORDER BY big_num";
  int result_code = sqlite3_prepare_v2(database, order_query, -1, &statement, NULL);
  assert_int_equal(result_code, SQLITE_OK);

  const int expected_order[] = {1, 2, 3, 4};
  int current_index = 0;

  while ((result_code = sqlite3_step(statement)) == SQLITE_ROW) {
    const int id = sqlite3_column_int(statement, 0);
    const sqlite_uint64 big_num = sqlite3_column_int64(statement, 1);
    const unsigned char* typeof_big_num = sqlite3_column_text(statement, 2);

    printf("ID: %d, big_num: %" PRIu64 ", typeof_big_num: %s\n", id, big_num, typeof_big_num);

    if (current_index < 4) {
      if (id != expected_order[current_index]) {
        printf("ORDERING ERROR: Expected ID %d at position %d, got %d\n", expected_order[current_index], current_index, id);
      }
      current_index++;
    }
  }
  assert_int_equal(result_code, SQLITE_DONE);
  sqlite3_finalize(statement);
}

// Second test - test comparison query
static void test_comparison_query(void** state) {
  test_data_t* test_data = (test_data_t*) *state;
  sqlite3* database = test_data->database;

  printf("\nTesting comparison (should be incorrect):\n");
  const char* comparison_query =
      "SELECT a.id as id_a, a.big_num as num_a, b.id as id_b, b.big_num as "
      "num_b, "
      "CASE WHEN a.big_num > b.big_num THEN '>' "
      "WHEN a.big_num < b.big_num THEN '<' "
      "ELSE '=' END as comparison "
      "FROM big_numbers a, big_numbers b "
      "WHERE a.id = 3 AND b.id = 4";

  sqlite3_stmt* statement = NULL;
  int result_code = sqlite3_prepare_v2(database, comparison_query, -1, &statement, NULL);
  assert_int_equal(result_code, SQLITE_OK);

  result_code = sqlite3_step(statement);
  assert_int_equal(result_code, SQLITE_ROW);

  const int id_a = sqlite3_column_int(statement, 0);
  sqlite_uint64 num_a = sqlite3_column_int64(statement, 1);
  const int id_b = sqlite3_column_int(statement, 2);
  sqlite_uint64 num_b = sqlite3_column_int64(statement, 3);
  const char* comparison = (const char*) sqlite3_column_text(statement, 4);

  printf("ID %d (%" PRIu64 ") %s ID %d (%" PRIu64 ")\n", id_a, num_a, comparison, id_b, num_b);

  // Assert that the comparison is incorrect
  // Max int64 + 1 should be less than Max uint64, so the comparison should be '<'
  assert_string_equal(comparison, "<");
  // But SQLite will likely show '>' because of signed int64 comparison

  sqlite3_finalize(statement);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_order_by_query),
      cmocka_unit_test(test_comparison_query),
  };

  return cmocka_run_group_tests(tests, group_setup, group_teardown);
}