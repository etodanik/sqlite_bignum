# sqlite_bignum

**sqlite_bignum** is a cross-platform [SQLite](https://sqlite.org/) extension that provides support for working with
64-bit unsigned integers (big numbers) as text, including collation and conversion functions. This makes it possible to
safely and efficiently store, compare, and convert 64-bit values within SQLite databases, especially in contexts where
binary and standard integer types may not be suitable.

## Features

- Provides custom collation for 64-bit unsigned integers stored as text in SQLite.
- Includes utility functions for converting between numeric and text representations of unsigned 64-bit integers.
- Can be dynamically loaded as a shared library (`.so`, `.dll`, or `.dylib`) on Linux, Windows, or macOS.
- Includes a dedicated test suite built with [cmocka](https://cmocka.org/).

## Usage

### Loading the Extension

First, build the extension for your platform (see [Building](#building)).

Then, you can load it in SQLite:

```
sql .load './sqlite_bignum.dll' 
```

### Available Functions

- `u64_to_text(INT)`: Converts a 64-bit unsigned integer to its text representation.
- `text_to_u64(TEXT)`: Parses a text representing an unsigned 64-bit integer and returns its integer value.
- `is_u64text(TEXT)`: Returns 1 if the text is a valid 64-bit unsigned integer, 0 otherwise.
- `u64text_display(TEXT)`: Returns an unpadded representation of the 64-bit integer text value

### Custom Collation

The extension registers a collation sequence that allows correct sorting of 64-bit unsigned integers stored as text.

```sql
-- Example usage: 
CREATE TABLE data
(
    value TEXT COLLATE U64TEXT
);

INSERT INTO data
VALUES ('18446744073709551615'),
       ('1'),
       ('42');
SELECT *
FROM data
ORDER BY value COLLATE u64text;

```

## Building

### Requirements

- [CMake](https://cmake.org/) 3.31 or newer
- C compiler (GCC/Clang/MSVC)
- Git

### Build Steps

1. Clone the repository:

```sh
git clone <your-repo-url>
cd sqlite_bignum
```

2. Configure and build:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The shared library will be found in the `build/` directory as:

- Linux: `libsqlite_bignum.so`
- macOS: `libsqlite_bignum.dylib`
- Windows: `sqlite_bignum.dll`

### Running Tests

```shell
cmake --build build --target test
```

Or, if you want to run the tests directly:

```shell
ctest --test-dir build
```

## Integration

You can use this extension with any SQLite application that supports loading external extensions.

```sql
.load '<path-to-shared-library>'
SELECT u64_to_text(42), text_to_u64('18446744073709551615'), is_u64text('invalid_value');

```

## License

[MIT](LICENSE) or your license of choice.

## Contributing

Contributions, bug reports, and suggestions are welcome!

