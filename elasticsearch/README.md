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
try products.put("7", {id: "7", name: "Kettle", price: 39.9, tags: ["kitchen"]})

let query = elasticsearch.match("name", "kettle").and(elasticsearch.term("tags", "kitchen"))
let hits = try products.search(query, size = 20, sort = "price")
for let hit in hits {
	print("{hit.source.name} {hit.score}")
}
```

## Tests

`adm test database/elasticsearch` runs against a loopback server. With a server the whole
suite runs, in indices named `adm-test-*` it creates and deletes:

```bash
docker run -d -p 127.0.0.1:9200:9200 -e discovery.type=single-node -e xpack.security.enabled=false elasticsearch:8.19.0
ADM_TEST_ELASTICSEARCH=http://127.0.0.1:9200 adm test database/elasticsearch
```

The same suite runs against OpenSearch with `ADM_TEST_OPENSEARCH`.
