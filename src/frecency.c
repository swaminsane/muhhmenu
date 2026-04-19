/* frecency.c - frecency scoring via SQLite
 * stores selection events, scores by recency bucket + frequency
 * db: ~/.local/share/muhhmenu/frecency.db
 */

#define _POSIX_C_SOURCE 200809L

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "muhhmenu.h"

#define FRECENCY_DIR "/.local/share/muhhmenu"
#define FRECENCY_DB "/.local/share/muhhmenu/frecency.db"
#define MAX_EVENTS 100 /* max stored events per item */

static sqlite3 *db = NULL;

/* ── init ───────────────────────────────────────────────────────────────── */

void frecency_init(void) {
  char path[512], dir[512];
  const char *home = getenv("HOME");
  if (!home)
    return;

  /* ensure dir exists */
  snprintf(dir, sizeof dir, "%s%s", home, FRECENCY_DIR);
  snprintf(path, sizeof path, "%s%s", home, FRECENCY_DB);
  mkdir(dir, 0755);

  if (sqlite3_open(path, &db) != SQLITE_OK) {
    fprintf(stderr, "muhhmenu: frecency db open failed: %s\n",
            sqlite3_errmsg(db));
    db = NULL;
    return;
  }

  /* create table if not exists */
  const char *sql = "CREATE TABLE IF NOT EXISTS events ("
                    "  id        TEXT    NOT NULL,"
                    "  ts        INTEGER NOT NULL"
                    ");"
                    "CREATE INDEX IF NOT EXISTS idx_id ON events(id);";

  char *err = NULL;
  if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "muhhmenu: frecency init failed: %s\n", err);
    sqlite3_free(err);
  }

  menu.frecency_db = db;
}

/* ── score ──────────────────────────────────────────────────────────────── */

double frecency_score(const char *id) {
  if (!db || !id || !id[0])
    return 0.0;

  const char *sql =
      "SELECT ts FROM events WHERE id = ? ORDER BY ts DESC LIMIT ?;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    return 0.0;

  sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, MAX_EVENTS);

  time_t now = time(NULL);
  double score = 0.0;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    time_t ts = (time_t)sqlite3_column_int64(stmt, 0);
    double age = difftime(now, ts);

    /* recency buckets — same as Firefox frecency */
    if (age < 3600)
      score += 100.0; /* last hour    */
    else if (age < 86400)
      score += 70.0; /* last day     */
    else if (age < 7 * 86400)
      score += 50.0; /* last week    */
    else if (age < 30 * 86400)
      score += 30.0; /* last month   */
    else
      score += 10.0; /* older        */
  }

  sqlite3_finalize(stmt);
  return score;
}

/* ── record ─────────────────────────────────────────────────────────────── */

void frecency_record(const char *id) {
  if (!db || !id || !id[0])
    return;

  time_t now = time(NULL);

  /* insert new event */
  const char *ins = "INSERT INTO events (id, ts) VALUES (?, ?);";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, ins, -1, &stmt, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  /* prune old events beyond MAX_EVENTS per item */
  const char *prune =
      "DELETE FROM events WHERE id = ? AND ts NOT IN ("
      "  SELECT ts FROM events WHERE id = ? ORDER BY ts DESC LIMIT ?"
      ");";
  if (sqlite3_prepare_v2(db, prune, -1, &stmt, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, id, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 3, MAX_EVENTS);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

/* ── close ──────────────────────────────────────────────────────────────── */

void frecency_close(void) {
  if (db) {
    sqlite3_close(db);
    db = NULL;
    menu.frecency_db = NULL;
  }
}
