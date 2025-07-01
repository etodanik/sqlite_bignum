cmake_minimum_required(VERSION 3.31)
project(sqlite_bignum C)

set(CMAKE_C_STANDARD 99)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SQLite3)
include(CMocka)

add_library(sqlite_bignum SHARED src/sqlite_bignum.c)
target_include_directories(sqlite_bignum PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sqlite_bignum PRIVATE SQLite3)

if (WIN32)
    set_target_properties(sqlite_bignum PROPERTIES PREFIX "")
endif ()

set_target_properties(sqlite_bignum PROPERTIES OUTPUT_NAME "sqlite_bignum")


enable_testing()
add_executable(test_sqlite_bignum test/test_sqlite_bignum.c)
target_include_directories(test_sqlite_bignum PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_sqlite_bignum PRIVATE sqlite_bignum cmocka::cmocka SQLite3)
add_test(NAME bignum_test COMMAND test_sqlite_bignum)
