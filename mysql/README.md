# adm.database.mysql

MySQL and MariaDB for `std.data.db`. The driver speaks the client/server protocol itself, in
ADM, over `std.net`: there is no C library to install.

```bash
adm get admlang:adm.database.mysql
```

## Connecting

```adm
use (
	std.data.db
	adm.database.mysql
)

let conn = try db.connect("mysql://app:secret@db.internal:3306/shop?sslmode=verify-full")
```

The scheme is `mysql:` or `mariadb:`. The port is 3306 when left out; the database may be left
out. Text is exchanged as `utf8mb4`. Options go in the query:

| Option | Values | Default |
|---|---|---|
| `sslmode` | `disable` (no TLS); `prefer` (TLS when the server offers it); `require` (TLS, certificate not checked); `verify-ca`, `verify-full` (TLS, certificate checked against the system's authorities and the host name) | `prefer` |
| `connect_timeout` | seconds to wait for the connection | `30` |
| `read_timeout` | seconds a statement may take to answer; past it the statement fails with `ErrorKind.TimedOut` and the connection closes | `0`, no limit |
| `multi_statements` | `true` lets `exec` and `query` without arguments run several statements separated by `;` | `false` |
| `found_rows` | `true` makes an UPDATE count the rows it matched, not only those it changed | `false` |

The password is checked with `caching_sha2_password` (under the server's RSA key when the
connection has no TLS), `mysql_native_password`, `sha256_password` or `mysql_clear_password`
(over TLS only), whichever the server asks for.

`mysql.open(url)` returns the driver's own `Connection`, which adds:

- `interrupt()`: stops the statement running on the connection, from any task (`KILL QUERY`
  on a session of its own); the statement fails with `ErrorKind.TimedOut`.
- `serverVersion()`: the server's version text (`8.4.3`, `11.4.4-MariaDB`).
- `threadId()`: the session's thread, as `CONNECTION_ID()` gives it.

## Statements

- Placeholders are `?`. The overloads that take a `named` map rewrite `:name` placeholders to
  `?` when no positional arguments are given.
- A statement with arguments is prepared, run and dropped on the server, so its values never
  become part of the text. One without arguments is sent as text.
- Rows are read from the server as `next` asks for them. Running another statement on the
  connection ends the rows still open on it; `next` on them then fails with
  `ErrorKind.Closed`.
- `Result.lastInsertId` is the last `AUTO_INCREMENT` key the statement generated.
- `LOAD DATA LOCAL INFILE` is refused: the driver sends the server no files.

## Values

| ADM argument | Sent as |
|---|---|
| `none` | NULL |
| `bool` | TINYINT 0 or 1 |
| integers | BIGINT (DECIMAL for a value above the 64-bit range) |
| floats | DOUBLE |
| `string` | text |
| `byte[]` | BLOB |
| `time.Time` | `2026-10-06 12:30:00.25`, in UTC |
| `time.Date`, `time.TimeOfDay`, `time.DateTime` | ISO 8601 with a space between date and time |
| `math.Decimal`, `bigint` | DECIMAL |

| Column type | `Rows.value(i)` |
|---|---|
| `tinyint`, `smallint`, `mediumint`, `int`, `bigint`, `year` | `int` (`bigint` for an unsigned value above the largest `int`) |
| `float`, `double` | `float` |
| `decimal` | `math.Decimal` |
| `date` | `time.Date` |
| `datetime`, `timestamp` | `time.DateTime` (MySQL sends neither with a zone) |
| `time` | `time.TimeOfDay` |
| `binary`, `varbinary`, `blob`, `bit`, geometry | `byte[]` |
| `char`, `varchar`, `text`, `json`, `enum`, `set` | `string` |

A value its type's reader does not take (a zero date, a `time` above a day or below zero) comes
back as its text. The typed readers: `int` and `bool` read the integer types; `float` reads
`float`, `double` and the integer types; `string` reads any column as MySQL prints it; `bytes`
reads text and binary columns. Rows read the same whether they came in the text protocol (no
arguments) or the binary one (with arguments or from a prepared statement).

## Errors

`DbError.native` is the server's error number (1062 for a duplicate key) and `sqlState` its
SQLSTATE.

| Error | `ErrorKind` |
|---|---|
| 1062, 1048, 1451, 1452, 3819, SQLSTATE class `23` | `Constraint` |
| SQLSTATE class `42` | `Syntax` |
| 1044, 1045, 1142, 1227, 1792, SQLSTATE class `28`, `25006` | `Denied` |
| 1205, 1213, SQLSTATE class `40` | `Conflict` |
| 1317, 3024 | `TimedOut` |
| SQLSTATE class `22` | `Type` |
| 1040, SQLSTATE class `08` | `Connection` |
| anything else | `Other` |

A refusal during the login (wrong password, unknown database) is `Denied`.

## Tests

The suite passes without running unless `ADM_TEST_MYSQL` holds `host:port` of a MySQL or
MariaDB server with a database `shop`, a user `app` (password `secret`) with all rights on it,
and `root` with password `rootpw` reachable from the test machine. On MySQL 8.4 start the
server with `--mysql-native-password=ON`, so the test can make a user of that method.

## Not done

Unix sockets, client certificates, compression, MariaDB's `client_ed25519` login, sending long
values in pieces, and session time zones (times are sent and read without a zone).
