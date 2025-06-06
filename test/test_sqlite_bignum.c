#include <setjmp.h>
// cmocka requires setjmp.h to be included before it
#include <cmocka.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>

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

static void test_uint64_comparison_failure(void** state) {
  sqlite3* database;
  int result_code = sqlite3_open(":memory:", &database);
  assert_int_equal(result_code, SQLITE_OK);

  execute_sql_statement(database, "CREATE TABLE big_numbers (id INTEGER PRIMARY KEY, big_num UNSIGNED BIG INT)");
  execute_sql_statement(database, "INSERT INTO big_numbers VALUES (1, 1000)");
  // Max int64
  execute_sql_statement(database, "INSERT INTO big_numbers VALUES (2, 9223372036854775807)");
  // Max int64 + 1
  execute_sql_statement(database, "INSERT INTO big_numbers VALUES (3, 9223372036854775808)");
  // Max uint64
  execute_sql_statement(database, "INSERT INTO big_numbers VALUES (4, 18446744073709551615)");

  // Test ordering of large numbers
  printf("Query with ORDER BY (should show incorrect ordering):\n");
  char* error_message = NULL;
  result_code = sqlite3_exec(database, "SELECT id, big_num FROM big_numbers ORDER BY big_num", print_query_results, 0, &error_message);

  if (result_code != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", error_message);
    sqlite3_free(error_message);
  }

  printf("Testing comparison (should be incorrect):\n");
  const char* comparison_query =
      "SELECT a.id as id_a, a.big_num as num_a, b.id as id_b, b.big_num as "
      "num_b, "
      "CASE WHEN a.big_num > b.big_num THEN '>' "
      "WHEN a.big_num < b.big_num THEN '<' "
      "ELSE '=' END as comparison "
      "FROM big_numbers a, big_numbers b "
      "WHERE a.id = 3 AND b.id = 4";

  result_code = sqlite3_exec(database, comparison_query, print_query_results, 0, &error_message);

  if (result_code != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", error_message);
    sqlite3_free(error_message);
  }

  sqlite3_close(database);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_uint64_comparison_failure),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}