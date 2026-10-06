# adm.database.sqlite

SQLite for `std.data.db`. The engine (SQLite 3.53.4) is compiled into your program from the C
source that ships with the library, so there is nothing else to install and no server to run.

```bash
adm get admlang:adm.database.sqlite
```

The library contains C code. ADM normally asks before installing a package that brings foreign
code; official `admlang` packages are accepted without the question.

## Connecting

```adm
use (
	std.data.db
	adm.database.sqlite
)

let conn = try db.connect("sqlite:app.db")
```

| URL | Database |
|---|---|
| `sqlite:app.db` | a file, relative to the working directory |
| `sqlite:///var/data/app.db` | a file by absolute path |
| `sqlite::memory:` | a private database in memory, gone when the connection closes |

Options go in the query, `sqlite:app.db?mode=ro&foreign_keys=on`:

| Option | Values | Default |
|---|---|---|
| `mode` | `ro` read only, `rw` read and write, `rwc` also create the file, `memory` | `rwc` |
| `busy_timeout` | milliseconds to wait for another connection's lock | `5000` |
| `foreign_keys`, `journal_mode`, `synchronous`, `cache_size`, `temp_store`, `locking_mode`, `secure_delete`, `recursive_triggers`, `query_only` | the pragma's value | SQLite's own |

`sqlite.open(path, mode = Mode.ReadWriteCreate, busyTimeout = 5s)` opens a file without a URL
and returns the driver's own `Connection`, which adds `interrupt()` (stops the statements
running on it, from any task). `sqlite.version()` is the engine's version.

## Statements

- Placeholders are `?`; SQLite also takes `?3`, `:name`, `@name` and `$name`. A named one takes
  the entry of the `named` map under its name, with or without the prefix.
- `exec` may hold several statements separated by `;`. Each takes its share of the positional
  arguments in turn. `query` and `prepare` take one statement.
- `Result.lastInsertId` is the rowid of the last inserted row.
- A transaction that may write starts with `BEGIN IMMEDIATE`, so it takes the write lock at
  once and cannot fail halfway for it; `beginTx(readOnly = true)` refuses writes. SQLite
  transactions are serializable whatever isolation level is asked for.

## Values

| ADM argument | Stored as |
|---|---|
| `none` | NULL |
| `bool` | INTEGER 0 or 1 |
| integers | INTEGER |
| floats | REAL |
| `string` | TEXT |
| `byte[]` | BLOB |
| `time.Time` | TEXT, RFC 3339 in UTC (`2026-10-06T12:30:00Z`) |
| `time.Date`, `time.TimeOfDay`, `time.DateTime` | TEXT in ISO 8601 |
| `math.Decimal`, `bigint` | TEXT in decimal digits |

`Rows.value(i)` returns what the column holds by storage class: `int`, `float`, `string`,
`byte[]` or `none`. The typed readers convert only without loss: `int` and `bool` read
INTEGER, `float` reads REAL and INTEGER, `string` reads anything but NULL, `bytes` reads BLOB
and TEXT. `db.read<T>` turns TEXT and INTEGER columns into the times, dates and decimals a
struct's fields declare.

## Errors

`DbError.native` is SQLite's extended result code (2067 for a UNIQUE violation). SQLite has no
SQLSTATE.

| SQLite result | `ErrorKind` |
|---|---|
| `SQLITE_CONSTRAINT` | `Constraint` |
| `SQLITE_BUSY`, `SQLITE_LOCKED` | `Conflict` |
| `SQLITE_READONLY`, `SQLITE_PERM`, `SQLITE_AUTH` | `Denied` |
| `SQLITE_CANTOPEN`, `SQLITE_NOTADB` | `Connection` |
| `SQLITE_ERROR` (syntax, unknown table or column) | `Syntax` |
| `SQLITE_MISMATCH`, `SQLITE_RANGE`, `SQLITE_TOOBIG` | `Type` |
| anything else | `Other` |

## The engine

`sqlite/sqlite3.inc` is the amalgamation's `sqlite3.c`, unmodified (`sqlite/UPSTREAM` has the
source and hashes). `adm_sqlite.c` includes it after setting the compile-time options:
serialized threading, FTS5, R-Tree, Geopoly, the math functions, column metadata, STAT4, the
`dbstat` table, URI file names, double-quoted string literals off, and no loading of extensions
from shared libraries. To move to a new SQLite release, replace `sqlite3.inc` with the new
`sqlite3.c` and update `UPSTREAM`.

A statement runs with the task's processor released while it works or waits for a lock, so
other tasks keep running. A connection serves one task at a time.

`adm_sqlite.c` hands text back to ADM as SQLite's own memory. A foreign function's `string`
result is borrowed, not copied, so the driver copies what it keeps (`owned` in `session.adm`).
