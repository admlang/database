#!/usr/bin/env bash
# Starts a one-member MongoDB replica set (rs0) in docker on 127.0.0.1:27018,
# which transactions and change streams need, and prints the variable the
# suite reads. The member advertises localhost:27018, so the driver's
# replica-set discovery reaches the same container.
set -euo pipefail

name=${MONGO_RS_NAME:-adm-mongo-rs}
image=${MONGO_IMAGE:-mongo:8.0}
port=27018

if ! docker ps --format '{{.Names}}' | grep -qx "$name"; then
	docker rm -f "$name" >/dev/null 2>&1 || true
	docker run -d --name "$name" -p "127.0.0.1:$port:$port" "$image" \
		--replSet rs0 --bind_ip_all --port "$port" >/dev/null
fi

for _ in $(seq 60); do
	docker exec "$name" mongosh --quiet --port "$port" --eval 'db.runCommand({ping: 1}).ok' >/dev/null 2>&1 && break
	sleep 1
done

docker exec "$name" mongosh --quiet --port "$port" --eval "
try { rs.status() } catch (e) {
	rs.initiate({_id: 'rs0', members: [{_id: 0, host: 'localhost:$port'}]})
}
while (!db.hello().isWritablePrimary) { sleep(200) }
" >/dev/null

echo "ADM_TEST_MONGODB=localhost:$port"
