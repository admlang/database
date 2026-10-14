# adm.database.mongodb

A MongoDB driver in ADM over `std.net`: the wire protocol itself, BSON from
`std.data.encoding`, replica sets and sharded clusters. It talks to MongoDB 4.2 and later and to
the servers that speak its protocol (Amazon DocumentDB, Azure Cosmos DB for MongoDB, FerretDB).
MongoDB is not SQL, so this is not a `std.data.db` driver: it has an API of its own, shaped like
the official drivers.

```bash
adm get admlang:adm.database.mongodb
```

```adm
use adm.database.mongodb

struct Customer {
	@bson("_id")
	id    mongodb.ObjectId
	name  string
	email ?string
	tags  string[]
}

let client = try mongodb.connect("mongodb://app:secret@db.internal/shop?replicaSet=rs0&tls=true")
let customers = client.database("shop").collection<Customer>("customers")
try customers.insert({name: "Ana", tags: ["new"]})
let ana = try customers.findOne({"name": "Ana"})                 // ?Customer
for let c in try customers.find({"tags": "new"}).sort({"name": 1}).limit(100) {
	print(c.name)
}
try customers.updateOne({"name": "Ana"}, {"$set": {"email": "ana@example.net"}})
try client.close()
```

## Tests

`adm test database/mongodb` runs the protocol and BSON mapping suites. With a server the whole
suite runs, in a database named `adm_test` it creates and drops:

```bash
docker run -d -p 127.0.0.1:27017:27017 mongo
ADM_TEST_MONGODB=127.0.0.1:27017 adm test database/mongodb
```

Transactions and change streams need a replica set; `testservers/replicaset.sh` starts one.
