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

## The file

- `db.stats()` gives the page size, the pages in the file, how many of them are free, the file
  size and the number of commits.
- `db.backup(path)` writes a copy of the database as it is now while other transactions go on.
- `db.compact(path)` writes the content to a new file without the free pages: the way to
  shrink a file after large deletes.

`Options`: `readOnly`, `pageSize` (a power of two from 4096 to 65536, for a new file), `sync`,
`cachePages` (how many tree nodes stay in memory; 8192 by default).

## How it works

The file is a sequence of pages. Each bucket is a B+tree whose nodes are page runs with a
CRC-32C checksum; a value larger than a page makes its leaf a longer run. A write transaction
never changes a page in place: it writes the nodes it changed to free pages, then one of two
alternating meta pages that names the new root. The meta page with the higher transaction
number whose checksum holds is the database, so a torn commit falls back to the one before it.
Pages a commit stops using go to a free list, itself stored in the file, and are reused once no
reader can still reach them.

Measured on one core with `--release` (keys of 13 bytes, values of about 12): a million keys
written in order in one transaction take 2.5 s, in random order 6.9 s; a full scan of them
0.4 s; a random read about 8 µs with the default cache and 3 µs when the tree fits in it; a
durable commit about 7 ms on an NVMe drive, which is the cost of its two syncs.

## Errors

Every failure is a `store.StoreError` with a `kind`: `Storage`, `Busy`, `Corrupt`, `Closed`,
`ReadOnly`, `NoBucket`, `Exists` (a bucket where a value is wanted, or the other way round),
`Argument`.

## Not built yet

- Typed collections over buckets: a struct per record, secondary indexes declared by
  annotation.
- Several processes writing to one file.
- Encryption and compression of pages.
- Windows and macOS file locking (the lock is `flock`).

## Tests

```bash
adm test database/store
```

The suite checks the database against an in-memory model over thousands of random writes,
deletes, rollbacks and reopens, a torn meta page, a damaged data page, readers running beside a
writer, and the copies `backup` and `compact` make. It needs nothing but a writable `/tmp`.
