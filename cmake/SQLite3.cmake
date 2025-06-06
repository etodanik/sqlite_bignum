include(FetchContent)

FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2025/sqlite-amalgamation-3500000.zip
)
FetchContent_MakeAvailable(sqlite3)

add_library(SQLite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(SQLite3 PUBLIC ${sqlite3_SOURCE_DIR})