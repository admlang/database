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
use (
	adm.database.mongodb
	std.data.encoding::(bson)
)

struct Customer {
	@bson("_id")
	id    mongodb.ObjectId
	name  string
	email ?string = none
	tags  string[]
}

let client = try mongodb.connect("mongodb://app:secret@db.internal/shop?replicaSet=rs0&tls=true")
let customers = client.database("shop").collection<Customer>("customers")
try customers.insert(Customer{name: "Ana", tags: ["new"]})        // the driver makes the _id
let ana = try customers.findOne({"name": "Ana"})                  // ?Customer
for let c in try customers.find({"tags": "new"}).sort({"name": 1}).limit(100).cursor() {
	print(c.name)
}
try customers.updateOne({"name": "Ana"}, {"$set": {"email": "ana@example.net"}})
try client.close()
```

`find` returns a builder; `cursor()` runs it and streams the results batch by batch, `all()`
reads them into an array, `first()` reads one. Filters, updates and pipelines are written as map
literals, structs, or `mongodb.Document` (a `BSONValue`) trees.

## Connecting

`connect` takes a connection string (`mongodb://` or `mongodb+srv://`, every option of the URI
Options spec) and `ClientOptions`, which set the same things in code, along with what a string
cannot hold (a `tls.Config`, a password kept out of the string); an option the string gives wins
over the field:

```adm
let options = mongodb.ClientOptions{appName: "shop", maxPoolSize: 20, timeout: 5s, compressors: ["zstd"]}
let client = try mongodb.connect("mongodb+srv://cluster0.example.net/", options)
```

- Topologies: a standalone server, replica sets (discovery, elections, read preferences with
  tags and max staleness), sharded clusters through mongos, and load balancers.
- Authentication: SCRAM-SHA-256 and SCRAM-SHA-1 (negotiated when no mechanism is named), PLAIN
  (LDAP), MONGODB-X509, MONGODB-AWS (environment, ECS, EC2 and web identity credentials, or
  `ClientOptions.awsCredentialProvider`). GSSAPI and MONGODB-OIDC are not built.
- TLS through `std.net.tls`: `tlsCAFile`, `tlsCertificateKeyFile` with
  `tlsCertificateKeyFilePassword` (encrypted PKCS #8 keys), `tlsAllowInvalidHostnames`,
  `tlsInsecure`, or a `tls.Config` in `ClientOptions.tls`. OCSP checks are not made.
- Wire compression (zstd, snappy, zlib), the Stable API (`serverApi`), client-side operation
  timeouts (`timeout`, `timeoutMS`), connection pools per server, retryable reads and writes,
  and the retries of operations an overloaded server refuses.

## What it covers

Sessions with causal consistency and snapshot reads, transactions and `withTransaction`; CRUD
including bulk writes and the find-and-modify family; aggregation, with `$out` and `$merge` on
secondaries when every server is 5.0 or later; `distinct` and counts; tailable and exhaust
cursors; indexes; change streams on a collection, a database or the deployment, resumed after a
network error or an election; GridFS buckets with upload and download streams.

## Not built yet

- GSSAPI (Kerberos) and MONGODB-OIDC authentication.
- OCSP checks of the server's certificate.
- Client-side field level encryption and Queryable Encryption.
- `Client.bulkWrite`, the 8.0 command that writes to several collections at once.
- Command, connection pool and topology events (the logging and monitoring specifications).

## Tests

`adm test database/mongodb` runs the protocol and BSON mapping suites. With servers the whole
suite runs, in a database named `adm_test` it creates and drops. `testservers/` starts them in
docker and prints the variables to set:

```bash
testservers/replicaset.sh     # ADM_TEST_MONGODB=localhost:27018 (a one-member replica set)
testservers/auth.sh           # ADM_TEST_MONGODB_AUTH=root:secret@localhost:27019
ADM_TEST_MONGODB=localhost:27018 ADM_TEST_MONGODB_AUTH=root:secret@localhost:27019 adm test database/mongodb
```

Transactions and change streams need the replica set; the authentication test signs in with
SCRAM-SHA-256, SCRAM-SHA-1 and a user name and password SASLprep rewrites.

The driver specifications' own tests (connection strings, authentication options, server
discovery and monitoring, server selection, max staleness) run from a checkout of
[mongodb/specifications](https://github.com/mongodb/specifications), whose licence keeps them out
of this repository:

```bash
ADM_TEST_MONGODB_SPECS=~/src/specifications/source adm test database/mongodb
```
