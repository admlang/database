# adm.database.store

An embedded key-value database: one file, buckets of ordered keys, transactions that are
all-or-nothing and survive a crash. It is written in ADM; there is nothing to install and no
server to run. For SQL in the same process use [`adm.database.sqlite`](../sqlite/).

```bash
adm get admlang:adm.database.store
```

```adm
use adm.database.store

let db = try store.open("app.db")
try db.write(def (tx store.Transaction) !none {
	let users = try tx.createBucket("users")
	try users.put("ana", "ana@example.com")
	return none
})
try db.read(def (tx store.Transaction) !none {
	let users = try tx.bucket("users")
	print(try users.text("ana")) unless users is none
	return none
})
try db.close()
```

## Transactions

Everything happens in a transaction.

- `db.read(body)` runs `body` on the database as it is at that moment. Whatever commits
  meanwhile, the transaction keeps seeing that state. Any number of tasks read at once.
- `db.write(body)` runs `body` as the one writer; other writers wait their turn. When `body`
  returns, its changes are committed; when it fails, none of them is kept.
- `db.beginTx(write)` hands out the transaction for code that ends it itself with `commitTx()`
  or `rollbackTx()`.

A commit is on the storage device before `write` returns, and a crash or a power cut at any
point leaves the database as the last commit made it: there is no recovery step and no log to
replay. `Options{sync: false}` skips the two syncs of every commit, for a bulk load or a cache
that can be rebuilt.

Keep read transactions short: the pages a commit frees are reused only once no older reader is
left, so a reader held open for hours makes the file grow.

One process has the file open for writing (`ErrorKind.Busy` for the second);
`Options{readOnly: true}` shares it among readers.

## Buckets

A bucket holds keys in byte order, each with a value or a nested bucket.

```adm
let orders = try tx.createBucket("orders")       // or the one that exists
let id = try orders.nextSequence()               // 1, 2, 3, ... stored with the bucket
try orders.put("order-{id}", payload)            // text or bytes, key and value
let found = try orders.get("order-7")            // ?byte[]
let note = try orders.text("order-7")            // ?string
try orders.delete("order-7")                     // missing is not an error
let lines = try orders.createBucket("lines")     // a bucket inside the bucket
try tx.deleteBucket("orders")                    // with everything in it
```

A key is 1 to 32768 bytes, a value up to 1 GiB. Values come back as copies. `tx.buckets()` lists
the top-level buckets, `bucket.count()` walks a bucket and counts its keys.

## Cursors

```adm
let cursor = bucket.cursor()
let entry = try cursor.seek("2026-10-")          // the first key not below it
for {
	break when entry is none || !string(entry.key).startsWith("2026-10-")
	print("{string(entry.key)} = {string(entry.value)}")
	entry = try cursor.next()
}
```

`first`, `last`, `next`, `previous` and `seek` each return the `Entry` they land on (`key`,
`value`, and `bucket` set for a nested bucket) or `none` past either end. A write in the same
transaction does not break a cursor: its next move goes on from the key it stood on.

## Collections

A collection keeps the records of one type in a bucket: a struct or a type per record, stored
under the field marked `@store.key` and found again by the fields marked `@store.index`.

```adm
struct User {
	@store.key(auto = true)
	id    int
	@store.index(unique = true)
	email string
	@store.index
	city  string
	@store.index
	born  ?time.Time
	name  string
}

try db.write(def (tx store.Transaction) !none {
	let users = try tx.collection<User>("users")
	let ana = User{email: "ana@example.com", city: "Cluj", name: "Ana"}
	try users.put(ana)                                   // ana.id is 1 now
	let same = try users.get(1)                          // ?User
	let owner = try users.first("email", "ana@example.com")
	let locals = try users.find("city", "Cluj")          // User[], in index order
	let some = try users.between("city", "A", "D")       // both ends included
	try users.delete(1)
	return none
})
```

- `put` stores a record, replacing the one with the same key; `insert` refuses a key that is
  taken. Both fail with `ErrorKind.Duplicate` when another record holds the same value in a
  unique index, and leave the collection as it was.
- `get`, `has` and `delete` take the key. `all()` returns every record in key order, `count()`
  walks them.
- `find(index, values...)` returns the records with that value, `first` the first of them,
  `between(index, low, high)` a range (`none` leaves an end open; `""` as the index is the key).
- `cursor(index)` walks the records in the order of an index, or of the key, with `first`,
  `last`, `next`, `previous` and `seek(values...)`, each returning the record or `none`.

**Keys.** `@store.key(auto = true)` on one integer field makes the collection number its
records: a record stored with 0 there gets the next number, written back into the field.
Without `auto` the program sets the key. Several fields marked `@store.key` make the key
together, in declaration order: `get("2026-10-07", 7)`.

**Indexes.** `@store.index` orders and finds records by a field; `unique = true` allows one
record per value. Fields that share `name = "..."` make one index over all of them:
`find("place", "RO", "Cluj")`, or `find("place", "RO")` for the first field alone. A record whose
optional field holds `none` is not in that index.

A key or index field is a bool, an integer, a float, a string, an enum, a `duration`, a `byte[]`
or a `time.Time` (an index field may be an optional of one; a key field may not). Keys are
written so that their byte order is the order of the values: numbers by value, negative ones
first, text and bytes by their bytes, times by the instant.

**Records** are stored as MessagePack maps with the field names, so a type can gain and lose
fields: a stored record without a field leaves it as a new record has it. The record type needs
a constructor without arguments. Records come back as copies; storing one again is what changes
the database. `@msgpack` on a field sets its stored name, as it does for the codec.

**When the type changes.** Opening a collection in a write transaction builds each index the
type declares and the file does not hold as declared, from the records already there, and drops
each index the type no longer declares. A read transaction on such a file reads records by key;
an index that is not built yet fails with `ErrorKind.Argument` until a writer has opened the
collection once. The key of a collection cannot change. `reindex()` builds every index again.

Inside the collection's bucket the records are in the bucket `records` under their key, each
index is a bucket inside `indexes`, and `schema` describes both: plain buckets, readable with
the calls above.

## The file

- `db.stats()` gives the page size, the pages in the file, how many of them are free, the file
  size and the number of commits.
- `db.backup(path)` writes a copy of the database as it is now while other transactions go on.
- `db.compact(path)` writes the content to a new file without the free pages: the way to
  shrink a file after large deletes.

`Options`: `readOnly`, `pageSize` (a power of two from 4096 to 65536, for a new file), `sync`,
`cachePages` (how many tree nodes stay in memory; 8192 by default), and `key`, `password`,
`passwordRounds` for an encrypted file.

## Encryption

A database created with a key or a password is encrypted, and opens only with it from then on.

```adm
let db = try store.open("vault.db", store.Options{password: "correct horse battery staple"})
let keyed = try store.open("keys.db", store.Options{key: key32})    // 32 bytes of your own
```

- Every page of the file is sealed with ChaCha20-Poly1305 under a random nonce drawn for each
  write. The tag takes the place of the checksum, and the page number is authenticated with the
  page: a page changed outside the library, or copied over another one, fails to read with
  `ErrorKind.Corrupt` instead of returning other content.
- The two meta pages are sealed the same way. What stays readable in the file is its page size,
  how many pages each node takes, and where the key comes from (the salt and the round count).
- `password` is turned into the key with PBKDF2 over HMAC-SHA-256, a random 16-byte salt and
  `passwordRounds` rounds (600,000 by default; about 0.4 s for every `open`), both kept in the
  meta pages. `key` is used as it is; give one or the other.
- `open` fails with `ErrorKind.Key` when the key or password is missing or wrong, and when one
  is given for a file that is not encrypted.
- `backup` and `compact` write copies that open with the same key or password.
- A page holds 24 bytes less than in a plain file. On the measurements below encryption costs
  between 2 and 7 percent.

Not covered: an attacker who can write the file can put back an older copy of the whole file or
of single pages at their own place, which the key cannot tell from the current ones. An existing
file cannot be encrypted, decrypted or given another key in place; `compact` into a new file
keeps the key it has.

## How it works

The file is a sequence of pages. Each bucket is a B+tree whose nodes are page runs with a
CRC-32C checksum (an authentication tag in an encrypted file); a value larger than a page makes
its leaf a longer run. A write transaction
never changes a page in place: it writes the nodes it changed to free pages, then one of two
alternating meta pages that names the new root. The meta page with the higher transaction
number whose checksum holds is the database, so a torn commit falls back to the one before it.
Pages a commit stops using go to a free list, itself stored in the file, and are reused once no
reader can still reach them.

Measured on one core with `--release` (keys of 13 bytes, values of about 12): a million keys
written in order in one transaction take 2.5 s, in random order 6.9 s; a full scan of them
0.4 s; a random read about 8 µs with the default cache and 3 µs when the tree fits in it; a
durable commit about 7 ms on an NVMe drive, which is the cost of its two syncs.

Typed collections, 200,000 records of six plain fields with three indexes: stored in one
transaction in 4.2 s, read by key in 8 µs each, by a unique index in 19 µs, all of them in key
order in 0.6 s. Records whose fields are all plain values (numbers, bools, strings, enums, bytes,
times and optionals of those) are written and read field by field from a plan made once per
collection; a record with an array, a map or a nested object goes through `std.data.encoding`'s
MessagePack codec, which takes about 10 µs to read one.

## Errors

Every failure is a `store.StoreError` with a `kind`: `Storage`, `Busy`, `Corrupt`, `Closed`,
`ReadOnly`, `NoBucket`, `Exists` (a bucket where a value is wanted, or the other way round),
`Argument`, `Duplicate` (a key or a unique index value that is taken), `Key` (the key or
password of an encrypted file is missing or wrong).

## Not built yet

- Changing the key of a collection in place, and collections in nested buckets.
- Queries over several indexes at once.
- Several processes writing to one file.
- Compression of pages.
- Changing the key or the password of a file, and a memory-hard password function (the
  standard library has PBKDF2 only).
- Windows and macOS file locking (the lock is `flock`).

## Tests

```bash
adm test database/store
```

The suite checks the database against an in-memory model over thousands of random writes,
deletes, rollbacks and reopens, a torn meta page, a damaged data page, readers running beside a
writer, and the copies `backup` and `compact` make. For collections it checks keys and indexes of
every field type and their order, unique indexes, keys and indexes over several fields, a type
that changes between runs, a collection against a model over random writes, and that the
field-by-field record format is byte for byte the codec's. It needs nothing but a writable
`/tmp`.
