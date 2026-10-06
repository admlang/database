# adm.database.postgres

PostgreSQL for `std.data.db`. The driver speaks the server's wire protocol (version 3.0)
itself, in ADM, over `std.net`: there is no C library to install.

```bash
adm get admlang:adm.database.postgres
```

## Connecting

```adm
use (
	std.data.db
	adm.database.postgres
)

let conn = try db.connect("postgres://app:secret@db.internal:5432/shop?sslmode=verify-full")
```

The scheme is `postgres:` or `postgresql:`. The port is 5432 when left out and the database is
the user's name when left out. Options go in the query:

| Option | Values | Default |
|---|---|---|
| `sslmode` | `disable`, `allow` (no TLS); `prefer` (TLS when the server offers it); `require` (TLS, certificate not checked); `verify-ca`, `verify-full` (TLS, certificate checked against the system's authorities and the host name) | `prefer` |
| `connect_timeout` | seconds to wait for the connection | `30` |
| `read_timeout` | seconds a statement may take to answer; past it the statement fails with `ErrorKind.TimedOut` and the connection closes | `0`, no limit |
| anything else | sent to the server as a session setting: `application_name`, `search_path`, `statement_timeout`, ... | |

The password is checked with SCRAM-SHA-256 (bound to the TLS channel when the server offers
`SCRAM-SHA-256-PLUS`), MD5, or sent as text, whichever the server asks for.

`postgres.open(url)` returns the driver's own `Connection`, which adds:

- `interrupt()`: asks the server to stop the statement running on the connection, from any
  task (a cancel request on a connection of its own); the statement fails with
  `ErrorKind.TimedOut`.
- `setting(name)`: what the server reported for the session (`server_version`, `TimeZone`).
- `processId()`: the server process, as `pg_backend_pid()` gives it.

## Statements

- Placeholders are `$1`, `$2`, ... The overloads that take a `named` map rewrite `:name`
  placeholders to `$n` when no positional arguments are given.
- `exec` without arguments may hold several statements separated by `;`. With arguments, and
  for `query` and `prepare`, the text is one statement.
- Rows are read from the server as `next` asks for them. Running another statement on the
  connection ends the rows still open on it; `next` on them then fails with
  `ErrorKind.Closed`.
- `Result.lastInsertId` is always 0: use `INSERT ... RETURNING` and read the row.
- `commitTx` of a transaction in which a statement failed fails too (SQLSTATE `25P02`): the
  server rolled it back.
- `COPY ... FROM STDIN` is refused; `LISTEN` notifications are read and dropped.

## Values

Arguments are sent as text and the server decides their type from the statement, as with
`psql`; `byte[]` is sent as bytes.

| ADM argument | Sent as |
|---|---|
| `none` | NULL |
| `bool` | `true` / `false` |
| integers, floats | decimal digits (`NaN`, `Infinity` for those floats) |
| `string` | text |
| `byte[]` | bytes (for `bytea`) |
| `time.Time` | RFC 3339 in UTC |
| `time.Date`, `time.TimeOfDay`, `time.DateTime` | ISO 8601 |
| `math.Decimal`, `bigint` | decimal digits |

| Column type | `Rows.value(i)` |
|---|---|
| `bool` | `bool` |
| `int2`, `int4`, `int8`, `oid` | `int` |
| `float4`, `float8` | `float` |
| `numeric` | `math.Decimal` |
| `bytea` | `byte[]` |
| `date` | `time.Date` |
| `time` | `time.TimeOfDay` |
| `timestamp` | `time.DateTime` |
| `timestamptz` | `time.Time` |
| everything else (`text`, `varchar`, `json`, `jsonb`, `uuid`, arrays, ...) | `string`, the server's text form |

A value its type's reader does not take (`infinity`, `NaN` in a `numeric`, a date before the
common era) comes back as its text. The typed readers: `bool` reads `bool`; `int` reads the
integer types; `float` reads the float and integer types; `string` reads any column as the
server's text; `bytes` decodes `bytea` and gives the text bytes of anything else.
`Column.typeName` is the type's name for the builtin types above and `oid N` otherwise;
`Column.nullable` is always true (a result does not say).

## Errors

`DbError.sqlState` is the server's SQLSTATE; `native` is 0.

| SQLSTATE | `ErrorKind` |
|---|---|
| class `23` | `Constraint` |
| class `42` (`42501` is `Denied`) | `Syntax` |
| class `28`, `25006` | `Denied` |
| `40001`, `40P01`, `55P03` | `Conflict` |
| `57014`, `25P03` | `TimedOut` |
| class `22` | `Type` |
| class `08`, `57` | `Connection` |
| anything else | `Other` |

A refusal before the session is up (wrong password, unknown database) is `Denied`.

## Tests

The suite passes without running unless `ADM_TEST_POSTGRES` holds `host:port` of a server with
a database `shop` owned by user `app` (password `secret`), TLS on, and two more users: `md5user`
(password `md5pw`, stored and checked as MD5) and `plainuser` (password `plainpw`, method
`password` in `pg_hba.conf`).

## Not done

Unix sockets, several hosts in one URL, client certificates, `COPY`, the binary result format,
notifications, and GSSAPI/SSPI logins.
