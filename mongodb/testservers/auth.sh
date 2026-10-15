#!/usr/bin/env bash
# Starts a MongoDB server that requires authentication in docker on
# 127.0.0.1:27019, with the users the SCRAM test signs in as: root (SCRAM-SHA-256),
# sha1user (SCRAM-SHA-1 only), and the RFC 4013 example user "ⅨⅨ" whose
# password "I<soft hyphen>X" SASLprep maps to "IX". Prints the variable the
# suite reads. MONGO_ROOT_PASSWORD sets root's password (default "secret").
set -euo pipefail

name=${MONGO_AUTH_NAME:-adm-mongo-auth}
image=${MONGO_IMAGE:-mongo:8.0}
port=27019
password=${MONGO_ROOT_PASSWORD:-secret}

if ! docker ps --format '{{.Names}}' | grep -qx "$name"; then
	docker rm -f "$name" >/dev/null 2>&1 || true
	docker run -d --name "$name" -p "127.0.0.1:$port:27017" \
		-e MONGO_INITDB_ROOT_USERNAME=root -e MONGO_INITDB_ROOT_PASSWORD="$password" "$image" >/dev/null
fi

for _ in $(seq 60); do
	docker exec "$name" mongosh --quiet -u root -p "$password" --eval 'db.runCommand({ping: 1}).ok' >/dev/null 2>&1 && break
	sleep 1
done

docker exec -e PW="$password" "$name" mongosh --quiet -u root -p "$password" --eval '
const admin = db.getSiblingDB("admin")
function ensure(user, pwd, mechanisms) {
	if (admin.getUser(user) === null) {
		admin.createUser({user: user, pwd: pwd, roles: ["readWriteAnyDatabase"], mechanisms: mechanisms})
	}
}
ensure("sha1user", "sha1pass", ["SCRAM-SHA-1"])
ensure("ⅨⅨ", "I­X", ["SCRAM-SHA-256"])
' >/dev/null

echo "ADM_TEST_MONGODB_AUTH=root:$password@localhost:$port"
