-- The table the `sql def` suites run on (sqlite_test.adm, people()),
-- for @sqlschema.
CREATE TABLE people (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL UNIQUE,
	mail TEXT,
	score REAL NOT NULL DEFAULT 0,
	active INTEGER NOT NULL DEFAULT 1,
	created TEXT,
	photo BLOB
);
