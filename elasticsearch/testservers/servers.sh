#!/bin/sh
# The clusters the gated tests use: Elasticsearch and OpenSearch without
# security, and Elasticsearch with security on (its own TLS CA, the `elastic`
# user's password, a trial license for bearer tokens).
# Usage: servers.sh <folder for the secure cluster's CA certificate>
#
#   ADM_TEST_ELASTICSEARCH=http://127.0.0.1:9200 ADM_TEST_OPENSEARCH=http://127.0.0.1:9201 \
#   ADM_TEST_ELASTICSEARCH_SECURE=https://localhost:9202 ADM_TEST_ELASTICSEARCH_PASSWORD=adm-test-pw \
#   ADM_TEST_ELASTICSEARCH_CERTS=<folder> adm test database/elasticsearch
set -e
dir=${1:?usage: servers.sh <folder for the secure cluster's CA certificate>}
mkdir -p "$dir"
heap="-Xms512m -Xmx512m"
docker rm -f adm-test-es adm-test-os adm-test-es-secure >/dev/null 2>&1 || true
docker run -d --name adm-test-es -p 127.0.0.1:9200:9200 -e discovery.type=single-node \
  -e xpack.security.enabled=false -e ES_JAVA_OPTS="$heap" elasticsearch:8.19.0 >/dev/null
docker run -d --name adm-test-os -p 127.0.0.1:9201:9200 -e discovery.type=single-node \
  -e DISABLE_SECURITY_PLUGIN=true -e OPENSEARCH_JAVA_OPTS="$heap" opensearchproject/opensearch:2.19.0 >/dev/null
docker run -d --name adm-test-es-secure -p 127.0.0.1:9202:9200 -e discovery.type=single-node \
  -e ELASTIC_PASSWORD=adm-test-pw -e xpack.security.authc.token.enabled=true -e ES_JAVA_OPTS="$heap" elasticsearch:8.19.0 >/dev/null
until curl -sfk -u elastic:adm-test-pw https://127.0.0.1:9202/ >/dev/null 2>&1; do sleep 2; done
docker cp adm-test-es-secure:/usr/share/elasticsearch/config/certs/http_ca.crt "$dir/http_ca.crt"
curl -sf --cacert "$dir/http_ca.crt" -u elastic:adm-test-pw -XPOST "https://localhost:9202/_license/start_trial?acknowledge=true" >/dev/null
until curl -sf http://127.0.0.1:9200/ >/dev/null 2>&1 && curl -sf http://127.0.0.1:9201/ >/dev/null 2>&1; do sleep 2; done
echo "started adm-test-es on 9200, adm-test-os on 9201, adm-test-es-secure on 9202; CA in $dir/http_ca.crt"
