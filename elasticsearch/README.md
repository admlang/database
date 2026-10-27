# adm.database.elasticsearch

An Elasticsearch and OpenSearch client in ADM over `std.net.http`: documents as typed structs,
bulk indexing, search with a typed query builder, aggregations, and index management. It talks
to Elasticsearch 8 and OpenSearch 2 and later.

```bash
adm get admlang:adm.database.elasticsearch
```

```adm
use adm.database.elasticsearch

struct Product {
	id    string
	name  string
	price float
	tags  string[]
}

let es = try elasticsearch.connect("https://search.internal:9200", apiKey = key)
let products = es.index<Product>("products")
try products.put("7", Product{id: "7", name: "Kettle", price: 39.9, tags: ["kitchen"]})

let query = elasticsearch.matches("name", "kettle").and(elasticsearch.term("tags", "kitchen"))
let hits = try products.search(query, size = 20, sort = "price")
for let hit in hits {
	print("{hit.source.name} {hit.score}")
}
```

## Connecting

```adm
let es = new elasticsearch.Client()
es.username = "elastic"
es.password = secret                       // Secret<string>; or apiKey, or bearerToken
es.caFingerprint = "05:C5:27:7F:..."       // the CA Elasticsearch printed when it set up security
es.sniff = true                            // use every node the cluster lists
try es.connect("https://node1:9200", "https://node2:9200")
```

`connectCloud(id)` takes an Elastic Cloud deployment's id. Requests go to the nodes in turn; a
node that does not answer (or answers 502, 503, 504) is passed over for `retireFor`, longer each
time in a row, and a read goes to the next node up to `maxRetries` times. `tls` takes a
`tls.Config` (a CA of your own, client certificates); `caFingerprint` trusts exactly the
certificate with that SHA-256. An API key is the encoded form Elasticsearch hands out, or
`id:key`. The client finds out which family and version it speaks to (`es.family`,
`es.version`) and asks Elasticsearch 8 for its version-8 compatible answers.

## Documents

```adm
let products = es.index<Product>("products")
products.refresh = elasticsearch.Refresh.WaitFor           // writes visible to the next search
try products.put("7", kettle)                              // create or replace
try products.create("8", pan)                              // Conflict when the id is taken
let made = try products.add(chair)                         // the cluster picks the id: made.id
let found = try products.get("7")                          // ?Product
let doc = try products.document("7")                       // ?Document: version, seqNo, primaryTerm
if !(doc is none) {
	try products.put("7", changed, ifSeqNo = doc.seqNo, ifPrimaryTerm = doc.primaryTerm)
}
try products.update("7", {"price": 34.9})
try products.update("7", elasticsearch.Script{source: "ctx._source.stock -= params.n", params: {"n": 1}})
let removed = try products.delete("7")                     // whether there was one
let many = try products.getMany(["7", "8"])                // map<string, Product>
```

A write conditioned on a sequence number fails with `ErrorKind.Conflict` when another write came
first. `putMany` stores a map of documents in one request; `es.bulk()` returns a `Bulk` writer
that sends `index`, `create`, `update` and `delete` items in batches (by count, bytes and a timer),
retries the items the cluster pushes back on, and reports each item that failed:

```adm
let writer = es.bulk(flushActions = 1000, flushInterval = 1s)
for let product in catalogue {
	try writer.index("products", product.id, product)
}
let report = try writer.close()            // created, updated, deleted, failed, failures
```

## Search

Queries are built with `matches`, `matchPhrase`, `matchPhrasePrefix`, `multiMatch`,
`queryString`, `simpleQueryString`, `term`, `terms`, `range`, `exists`, `prefix`, `wildcard`,
`regexp`, `fuzzy`, `ids`, `nested`, `functionScore`, `knn` and `boolQuery`, and combined with
`and`, `or`, `not`, `filter` and `boost`; `rawQuery` takes any other as JSON. `knn` is written
as Elasticsearch's `knn` query or OpenSearch's k-NN query, as the cluster needs.

```adm
let options = elasticsearch.SearchOptions{
	query: elasticsearch.matches("name", "kettle").filter(elasticsearch.range("price", lte = 50)),
	sort: [elasticsearch.Sort{field: "price"}],
	highlight: elasticsearch.Highlight{fields: ["name"]},
	suggest: {"fix": elasticsearch.Suggester{field: "name", text: "ketle"}},
	aggregations: {"by_tag": aggs.terms("tags.keyword").sub("price", aggs.stats("price"))},
}
let found = try products.search(options)
for let bucket in found.aggregation("by_tag").buckets {
	print("{bucket.keyText} {bucket.docCount} {bucket.sub("price").avg}")
}
```

`SearchOptions` also holds paging (`from` and `size`, `searchAfter`, a `pointInTime`), source
filtering, `postFilter`, `collapse`, `minScore`, `exactTotal` and `seqNoPrimaryTerm`. `scan`
walks every match through a point in time a page at a time, `scroll` through a scroll context;
`count`, `explain`, `multiSearch` and `searchTemplate` (inline or stored with
`indices.putSearchTemplate`) cover the rest.

`use adm.database.elasticsearch.aggs` gives the aggregations: `terms`, `histogram`,
`dateHistogram`, `range`, `dateRange`, `stats`, `extendedStats`, `percentiles`, `cardinality`,
`avg`, `sum`, `min`, `max`, `valueCount`, `missing`, `nested`, `reverseNested`, `global`,
`filter`, `filters`, `composite` (paged with its `afterKey`) and `topHits`; `new
Aggregation(kind, settings)` makes any other.

## Indices

```adm
try es.indices.createFor<Product>("products", settings = {"number_of_shards": 1})
try es.indices.addAlias("products-v2", "products", isWriteIndex = true)
try es.indices.putTemplate("logs", ["logs-*"], composedOf = ["logs-settings"], priority = 200)
let rolled = try es.indices.rollover("logs-write", maxAge = "7d", maxSize = "50gb")
try es.indices.reindex("products", "products-v2")
try es.ingest.putPipeline("stamp", [map<string, any>{"set": map<string, any>{"field": "seen", "value": "\{\{_ingest.timestamp}}"}}])
```

`createFor<T>` maps T's fields: strings as text with a `keyword` sub-field, numbers, booleans,
`time.Time` as a date, nested structs as objects; `@json` renames a field and
`@elasticsearch.field(kind = "keyword")`, `analyzer`, `index = false`, `nested = true`, `format`,
and `dims` with `similarity` for vectors set the rest. A vector is `dense_vector` on
Elasticsearch and `knn_vector` on OpenSearch, which also gets `index.knn`. `es.info()`,
`es.health()`, `es.count()` and the cat APIs (`catIndices`, `catNodes`, `catShards`,
`catAliases`, `cat(name)`) report on the cluster.

## Errors

Every failure is an `elasticsearch.ElasticError` with a `kind`: `Connection`, `TimedOut`,
`Closed`, `Argument` (400), `Denied` (401, 403), `NotFound` (404), `Conflict` (409, or an index
that exists), `Busy` (429), `Protocol`, `Server`. `status` holds the HTTP status, `errorType` the
server's error type (`index_not_found_exception`), and `failures` the items of a bulk write that
failed.

## Tests

`adm test database/elasticsearch` runs against a loopback server. With a server the whole
suite runs, in indices named `adm-test-*` it creates and deletes:

```bash
docker run -d -p 127.0.0.1:9200:9200 -e discovery.type=single-node -e xpack.security.enabled=false elasticsearch:8.19.0
ADM_TEST_ELASTICSEARCH=http://127.0.0.1:9200 adm test database/elasticsearch
```

The same suite runs against OpenSearch with `ADM_TEST_OPENSEARCH`. `testservers/servers.sh`
starts both, and an Elasticsearch with security on for the TLS, API key and token tests:

```bash
database/elasticsearch/testservers/servers.sh /tmp/es
ADM_TEST_ELASTICSEARCH=http://127.0.0.1:9200 ADM_TEST_OPENSEARCH=http://127.0.0.1:9201 \
ADM_TEST_ELASTICSEARCH_SECURE=https://localhost:9202 ADM_TEST_ELASTICSEARCH_PASSWORD=adm-test-pw \
ADM_TEST_ELASTICSEARCH_CERTS=/tmp/es adm test database/elasticsearch
```
