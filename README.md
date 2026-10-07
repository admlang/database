# ADM database drivers

Official [ADM](https://github.com/admlang/adm) libraries that connect an ADM program to SQL
databases. Each library is one driver for `std.data.db`, published as `adm.database.<name>` and
signed by the `admlang` publisher. The drivers live outside the standard library so that a fix
or a new protocol version ships without a std release.

| Library | Database | How it connects | URL schemes |
|---|---|---|---|
| [`adm.database.sqlite`](sqlite/) | SQLite 3.53.4 | the engine's C source, compiled into the program | `sqlite:`, `sqlite3:` |
| [`adm.database.postgres`](postgres/) | PostgreSQL | the wire protocol (3.0), written in ADM | `postgres:`, `postgresql:` |
| [`adm.database.mysql`](mysql/) | MySQL, MariaDB | the client/server protocol, written in ADM | `mysql:`, `mariadb:` |

One library is a database of its own rather than a driver:

| Library | What it is |
|---|---|
| [`adm.database.store`](store/) | an embedded key-value database written in ADM: one file, ordered buckets, crash-safe transactions; its own API, not `std.data.db` |

## Use

Install the driver for your database, for example:

```bash
adm get admlang:adm.database.postgres
```

Importing the library is what registers its URL schemes; the program then talks to
`std.data.db`, the same way for every driver:

```adm
application Shop {
	use (
		std.data.db
		adm.database.postgres
	)

	struct Customer {
		id    int
		name  string
		@db.column("mail")
		email ?string
	}

	def run() !none {
		let conn = try db.connect("postgres://app:secret@db.internal/shop?sslmode=verify-full")
		try conn.exec("INSERT INTO customers (id, name) VALUES ($1, $2)", 7, "Ana")

		let rows = try conn.query("SELECT * FROM customers WHERE id >= $1", 1)
		let all = try db.readAll<Customer>(rows)

		let tx = try conn.beginTx()
		try tx.exec("UPDATE customers SET name = $1 WHERE id = $2", "Anna", 7)
		try tx.commitTx()
		return conn.close()
	}

	def new(args string[]) {
		run() onerror (err error) {
			println(err.message)
			recover none
		}
	}
}
```

Connecting with a scheme no linked driver handles fails with `no driver for 'postgres'`.

A connection serves one task at a time. Tasks that share a database use a pool, which is a
`db.Connection` too and lends a real connection to each statement, result or transaction:

```adm
let shop = try db.pool("postgres://app:secret@db.internal/shop", 16)
try shop.exec("UPDATE stock SET remaining = remaining - 1 WHERE id = $1", id)
```

What differs between drivers is the placeholder in statement text: `?` for SQLite and MySQL,
`$1` for PostgreSQL. Statements written with `:name` placeholders and the `named` overloads
(`conn.exec("... VALUES (:id, :name)", db.values(customer))`) run unchanged on all three, and so
do `sql def` methods, whose `{argument}` placeholders the compiler hands to the driver in its
own style:

```adm
type Customers {
	@db.connection()
	conn db.Queryable

	def new(conn db.Queryable) {
		self.conn = conn
	}

	sql def find(id int) !?Customer {
		SELECT * FROM customers WHERE id = {id}
	}
}
```

A `sql def` outside such a type runs on the `Database` service's default connection, or on
the one `@db("name")` names:

```adm
use std.services.database::(Database, db)

@db("reports")
sql def dailyTotal(day time.Date) !float {
	SELECT sum(amount) FROM sales WHERE day = {day}
}

try await Database.start()
try Database.add("reports", "sqlite:reports.db")
let total = try dailyTotal(today)
```

Every failure is a `db.DbError` with a `kind` (`Constraint`, `Syntax`, `Denied`, `Conflict`,
`TimedOut`, `Connection`, `Closed`, `Type`, `NoRows`, ...), the database's SQLSTATE where it
has one, and its own error number.

Each library's README lists its URL options, how values map to the database's types, and what
it offers beyond `std.data.db`.

## Tests

`adm test database/sqlite` needs nothing. The PostgreSQL and MySQL suites pass without running
unless they are told where a server is:

```bash
ADM_TEST_POSTGRES=127.0.0.1:5432 adm test database/postgres
ADM_TEST_MYSQL=127.0.0.1:3306 adm test database/mysql      # MySQL or MariaDB
```

The READMEs of those libraries say what the server must hold.

## License

SQLite is in the public domain; its source ships beside the driver (`sqlite/sqlite/UPSTREAM`).
