// The C side of adm.database.sqlite: SQLite compiled into the program, behind
// functions that take and return integers, strings and byte arrays.
//
// The amalgamation is included here, not compiled by itself, so that this
// file sets its compile-time options. A connection is a `sqlite3*` and a
// statement a `sqlite3_stmt*`, both carried by ADM as unsigned integers.

// Options (https://www.sqlite.org/compile.html). Serialized threading: a
// connection is safe wherever the scheduler runs the task that uses it.
#define SQLITE_THREADSAFE 1
#define SQLITE_DQS 0
#define SQLITE_DEFAULT_MEMSTATUS 0
#define SQLITE_DEFAULT_WAL_SYNCHRONOUS 1
#define SQLITE_LIKE_DOESNT_MATCH_BLOBS 1
#define SQLITE_MAX_EXPR_DEPTH 0
#define SQLITE_STRICT_SUBTYPE 1
#define SQLITE_USE_URI 1
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_OMIT_SHARED_CACHE 1
// No loading of shared libraries into the program.
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_ENABLE_COLUMN_METADATA 1
#define SQLITE_ENABLE_FTS5 1
#define SQLITE_ENABLE_RTREE 1
#define SQLITE_ENABLE_GEOPOLY 1
#define SQLITE_ENABLE_MATH_FUNCTIONS 1
#define SQLITE_ENABLE_STAT4 1
#define SQLITE_ENABLE_DBSTAT_VTAB 1

#include "sqlite/sqlite3.inc"

#include <stdint.h>
#include <string.h>

// A task about to block in the operating system gives its processor to
// another task for the duration (ADM runtime).
extern void adm_sched_enter_syscall(void);
extern void adm_sched_exit_syscall(void);

// Opens a database. The handle is returned even when opening failed, so the
// reason can be read from it; 0 only when memory ran out.
uint64_t asqOpen(const char* name, int64_t flags, int64_t busy_ms) {
    sqlite3* db = NULL;
    adm_sched_enter_syscall();
    int rc = sqlite3_open_v2(name, &db, (int)flags, NULL);
    adm_sched_exit_syscall();
    if (db == NULL) {
        return 0;
    }
    if (rc == SQLITE_OK) {
        sqlite3_extended_result_codes(db, 1);
        sqlite3_busy_timeout(db, (int)busy_ms);
    }
    return (uint64_t)(uintptr_t)db;
}

// Closes a database; one with statements still open closes with the last of
// them.
int64_t asqClose(uint64_t db) {
    adm_sched_enter_syscall();
    int rc = sqlite3_close_v2((sqlite3*)(uintptr_t)db);
    adm_sched_exit_syscall();
    return rc;
}

// The extended result code of the last failed call on the database.
int64_t asqErrorCode(uint64_t db) {
    return sqlite3_extended_errcode((sqlite3*)(uintptr_t)db);
}

// The message of the last failed call on the database.
const char* asqErrorMessage(uint64_t db) {
    const char* msg = sqlite3_errmsg((sqlite3*)(uintptr_t)db);
    return msg ? msg : "";
}

// The message for a result code.
const char* asqErrorText(int64_t code) {
    const char* msg = sqlite3_errstr((int)code);
    return msg ? msg : "";
}

// Compiles the first statement of sql[offset..n). `tail[0]` receives the
// offset where the next statement starts. Returns the statement, or 0 when
// the text failed to compile (asqErrorCode is then not 0) or held only
// space and comments.
uint64_t asqPrepare(uint64_t db, const uint8_t* sql, int64_t n, int64_t offset, int64_t* tail) {
    sqlite3_stmt* stmt = NULL;
    const char* rest = NULL;
    const char* start = (const char*)sql + offset;
    int rc = sqlite3_prepare_v2((sqlite3*)(uintptr_t)db, start, (int)(n - offset), &stmt, &rest);
    tail[0] = rest ? (int64_t)(rest - (const char*)sql) : n;
    if (rc != SQLITE_OK) {
        return 0;
    }
    return (uint64_t)(uintptr_t)stmt;
}

int64_t asqFinalize(uint64_t stmt) {
    return sqlite3_finalize((sqlite3_stmt*)(uintptr_t)stmt);
}

// Makes a statement ready to run again and drops its bound values.
int64_t asqReset(uint64_t stmt) {
    sqlite3_stmt* s = (sqlite3_stmt*)(uintptr_t)stmt;
    int rc = sqlite3_reset(s);
    sqlite3_clear_bindings(s);
    return rc;
}

int64_t asqBindCount(uint64_t stmt) {
    return sqlite3_bind_parameter_count((sqlite3_stmt*)(uintptr_t)stmt);
}

// The name of parameter `index` (from 1) with its prefix (":id", "@id",
// "$id", "?3"); "" for a plain `?`.
const char* asqBindName(uint64_t stmt, int64_t index) {
    const char* name = sqlite3_bind_parameter_name((sqlite3_stmt*)(uintptr_t)stmt, (int)index);
    return name ? name : "";
}

int64_t asqBindNull(uint64_t stmt, int64_t index) {
    return sqlite3_bind_null((sqlite3_stmt*)(uintptr_t)stmt, (int)index);
}

int64_t asqBindInt(uint64_t stmt, int64_t index, int64_t value) {
    return sqlite3_bind_int64((sqlite3_stmt*)(uintptr_t)stmt, (int)index, value);
}

int64_t asqBindFloat(uint64_t stmt, int64_t index, double value) {
    return sqlite3_bind_double((sqlite3_stmt*)(uintptr_t)stmt, (int)index, value);
}

// Binds text; SQLite keeps its own copy.
int64_t asqBindText(uint64_t stmt, int64_t index, const uint8_t* text, int64_t n) {
    // An empty array has no elements to point at; a null pointer would bind
    // NULL, not the empty string.
    if (text == NULL || n == 0) {
        return sqlite3_bind_text((sqlite3_stmt*)(uintptr_t)stmt, (int)index, "", 0, SQLITE_STATIC);
    }
    return sqlite3_bind_text64((sqlite3_stmt*)(uintptr_t)stmt, (int)index, (const char*)text, (sqlite3_uint64)n,
                               SQLITE_TRANSIENT, SQLITE_UTF8);
}

// Binds a blob; SQLite keeps its own copy.
int64_t asqBindBlob(uint64_t stmt, int64_t index, const uint8_t* data, int64_t n) {
    if (n == 0) {
        return sqlite3_bind_zeroblob((sqlite3_stmt*)(uintptr_t)stmt, (int)index, 0);
    }
    return sqlite3_bind_blob64((sqlite3_stmt*)(uintptr_t)stmt, (int)index, data, (sqlite3_uint64)n, SQLITE_TRANSIENT);
}

// Runs a statement to its first row or its end: 100 a row is ready, 101
// done, anything else the result code of the failure. This is where a
// statement does its work and waits for locks, so the task's processor is
// free meanwhile.
int64_t asqRun(uint64_t stmt) {
    adm_sched_enter_syscall();
    int rc = sqlite3_step((sqlite3_stmt*)(uintptr_t)stmt);
    adm_sched_exit_syscall();
    return rc;
}

// Moves to the next row: 100, 101 or a result code, as asqRun.
int64_t asqNext(uint64_t stmt) {
    return sqlite3_step((sqlite3_stmt*)(uintptr_t)stmt);
}

int64_t asqColumnCount(uint64_t stmt) {
    return sqlite3_column_count((sqlite3_stmt*)(uintptr_t)stmt);
}

const char* asqColumnName(uint64_t stmt, int64_t column) {
    const char* name = sqlite3_column_name((sqlite3_stmt*)(uintptr_t)stmt, (int)column);
    return name ? name : "";
}

// The type the column was declared with; "" for an expression.
const char* asqColumnDeclared(uint64_t stmt, int64_t column) {
    const char* decl = sqlite3_column_decltype((sqlite3_stmt*)(uintptr_t)stmt, (int)column);
    return decl ? decl : "";
}

// Whether the result column may hold NULL: 0 when it is a table column
// declared NOT NULL, 1 otherwise.
int64_t asqColumnNullable(uint64_t db, uint64_t stmt, int64_t column) {
    sqlite3_stmt* s = (sqlite3_stmt*)(uintptr_t)stmt;
    const char* table = sqlite3_column_table_name(s, (int)column);
    const char* origin = sqlite3_column_origin_name(s, (int)column);
    if (table == NULL || origin == NULL) {
        return 1;
    }
    int not_null = 0;
    int rc = sqlite3_table_column_metadata((sqlite3*)(uintptr_t)db, sqlite3_column_database_name(s, (int)column), table,
                                           origin, NULL, NULL, &not_null, NULL, NULL);
    return (rc == SQLITE_OK && not_null) ? 0 : 1;
}

// The storage class of a column of the current row: 1 integer, 2 float,
// 3 text, 4 blob, 5 NULL.
int64_t asqColumnKind(uint64_t stmt, int64_t column) {
    return sqlite3_column_type((sqlite3_stmt*)(uintptr_t)stmt, (int)column);
}

int64_t asqColumnInt(uint64_t stmt, int64_t column) {
    return sqlite3_column_int64((sqlite3_stmt*)(uintptr_t)stmt, (int)column);
}

double asqColumnFloat(uint64_t stmt, int64_t column) {
    return sqlite3_column_double((sqlite3_stmt*)(uintptr_t)stmt, (int)column);
}

// Copies up to `cap` bytes of a text or blob column of the current row into
// `out` and returns how many bytes the column holds; a result above `cap`
// asks for a larger buffer.
int64_t asqColumnCopy(uint64_t stmt, int64_t column, uint8_t* out, int64_t cap) {
    sqlite3_stmt* s = (sqlite3_stmt*)(uintptr_t)stmt;
    const void* data = sqlite3_column_blob(s, (int)column);
    int64_t n = sqlite3_column_bytes(s, (int)column);
    int64_t take = n > cap ? cap : n;
    if (data != NULL && take > 0) {
        memcpy(out, data, (size_t)take);
    }
    return n;
}

// Rows the last INSERT, UPDATE or DELETE on the database changed.
int64_t asqChanges(uint64_t db) {
    return sqlite3_changes64((sqlite3*)(uintptr_t)db);
}

int64_t asqLastInsertId(uint64_t db) {
    return sqlite3_last_insert_rowid((sqlite3*)(uintptr_t)db);
}

// 1 when no transaction is open on the database.
int64_t asqAutocommit(uint64_t db) {
    return sqlite3_get_autocommit((sqlite3*)(uintptr_t)db);
}

// 1 when the statement makes no direct change to the database.
int64_t asqReadOnly(uint64_t stmt) {
    return sqlite3_stmt_readonly((sqlite3_stmt*)(uintptr_t)stmt);
}

// Stops the statements running on the database, from any task.
void asqInterrupt(uint64_t db) {
    sqlite3_interrupt((sqlite3*)(uintptr_t)db);
}

const char* asqVersion(void) {
    return sqlite3_libversion();
}
