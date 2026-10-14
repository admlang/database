# adm.database.orm

A typed data access layer over `std.data.db` and its drivers: a repository per struct, a query
builder that writes each database's SQL with bound parameters, relations, and migrations
derived from the structs. It is one opinion about how to use SQL from ADM, so it is a library
rather than part of `std.data.db`, which stays the neutral SQL layer every driver implements.

```bash
adm get admlang:adm.database.orm
```

```adm
use (
	std.data.db
	adm.database.orm
	adm.database.postgres
)

@orm.table("customers")
struct Customer {
	@orm.key(generated = true)
	id      int
	name    string
	@db.column("mail")
	email   ?string
	@orm.hasMany(by = "customerId")
	orders  Order[]
	@orm.version()
	version int
}

let conn = try db.connect("postgres://app:secret@db.internal/shop")
let customers = new orm.Repository<Customer>(conn)
let ana = try customers.find(7)                                  // ?Customer
let recent = try customers.query().where("name", orm.like, "A%").orderBy("name").limit(50).all()
try customers.save(ana)                                          // insert or update, version checked
let withOrders = try customers.query().include("orders").all()
```

## Tests

`adm test database/orm` runs on SQLite, which needs nothing. `ADM_TEST_POSTGRES` and
`ADM_TEST_MYSQL` add the same suite on those servers, in schemas named `adm_test_*` it creates
and drops.
