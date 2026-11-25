#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Creates (if needed) a project-local PostgreSQL cluster, starts it, creates the
# application role + database, and applies schema + seed data.
#
#   bash database/setup_db.sh            # init/start + (re)create database
#   bash database/setup_db.sh start      # just start the server
#   bash database/setup_db.sh stop       # stop the server
#
# Settings come from config/app.conf (DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD).
# Run from the project root inside an MSYS2 UCRT64 shell (PostgreSQL 18 or newer is expected).
#
# A PostgreSQL data folder can only be opened by the SAME major version that created it. If database/pgdata was created
# by an older version (for example 17) it is moved to database/pgdata_pg17_backup - never deleted - and a fresh cluster
# is created for the installed version.
# ---------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."

conf() { grep -E "^$1=" config/app.conf | head -1 | cut -d= -f2- | tr -d '\r'; }
PORT=$(conf DB_PORT); DB=$(conf DB_NAME); APPUSER=$(conf DB_USER); APPPASS=$(conf DB_PASSWORD)
PGDATA="database/pgdata"
SUPERPASS="postgres"
export PGPASSWORD="$SUPERPASS"

# major version of the installed server ("pg_ctl (PostgreSQL) 18.6" -> 18) and of the existing data folder
SERVER_MAJOR=$(pg_ctl --version | sed -E 's/^[^0-9]*([0-9]+).*/\1/')
cluster_major() { [ -f "$PGDATA/PG_VERSION" ] && tr -d '\r\n ' < "$PGDATA/PG_VERSION" || echo ""; }
version_mismatch() { local v; v=$(cluster_major); [ -n "$v" ] && [ "$v" != "$SERVER_MAJOR" ]; }

start_server() {
  if pg_ctl -D "$PGDATA" status >/dev/null 2>&1; then
    echo "[db] server already running on port $PORT"
  else
    pg_ctl -D "$PGDATA" -o "-p $PORT" -l database/postgres.log -w start
  fi
}

case "${1:-setup}" in
  stop)
    if version_mismatch; then echo "[db] $PGDATA belongs to PostgreSQL $(cluster_major), the installed server is $SERVER_MAJOR - nothing of this version is running."; exit 0; fi
    pg_ctl -D "$PGDATA" -m fast stop; exit 0 ;;
  start)
    if version_mismatch; then
      echo "[db] $PGDATA was created by PostgreSQL $(cluster_major) but the installed server is PostgreSQL $SERVER_MAJOR, which cannot open it." >&2
      echo "[db] Run:  bash database/setup_db.sh   (moves the old folder aside and creates a PostgreSQL $SERVER_MAJOR cluster)" >&2
      exit 1
    fi
    start_server; exit 0 ;;
esac

if version_mismatch; then
  OLD=$(cluster_major)
  BACKUP="database/pgdata_pg${OLD}_backup"
  [ -e "$BACKUP" ] && BACKUP="${BACKUP}_$(date +%Y%m%d%H%M%S)"
  echo "[db] $PGDATA was created by PostgreSQL $OLD, the installed server is PostgreSQL $SERVER_MAJOR (cannot open it)."
  echo "[db] moving the old folder to $BACKUP (kept as a backup - delete it when you no longer need it)"
  mv "$PGDATA" "$BACKUP" || { echo "[db] could not move it - is the old server still running? Stop it first." >&2; exit 1; }
fi

if [ ! -f "$PGDATA/PG_VERSION" ]; then
  echo "[db] initialising a PostgreSQL $SERVER_MAJOR cluster in $PGDATA"
  printf '%s\n' "$SUPERPASS" > database/.pwfile
  initdb -D "$PGDATA" -U postgres -E UTF8 --locale=C --auth=scram-sha-256 --pwfile=database/.pwfile >/dev/null
  rm -f database/.pwfile
fi
start_server

PSQL="psql -h localhost -p $PORT -U postgres -v ON_ERROR_STOP=1 -q"
echo "[db] (re)creating role $APPUSER and database $DB"
$PSQL -d postgres -c "DROP DATABASE IF EXISTS $DB;"
$PSQL -d postgres -c "DO \$\$BEGIN IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname='$APPUSER') THEN CREATE ROLE $APPUSER LOGIN PASSWORD '$APPPASS'; END IF; END\$\$;"
$PSQL -d postgres -c "CREATE DATABASE $DB OWNER $APPUSER;"

export PGPASSWORD="$APPPASS"
APSQL="psql -h localhost -p $PORT -U $APPUSER -d $DB -v ON_ERROR_STOP=1 -q"
# pgcrypto must be created by a superuser once
PGPASSWORD="$SUPERPASS" psql -h localhost -p $PORT -U postgres -d "$DB" -q -c "CREATE EXTENSION IF NOT EXISTS pgcrypto;"
$APSQL -c "ALTER SCHEMA public OWNER TO $APPUSER;" 2>/dev/null || true
echo "[db] applying schema.sql";    $APSQL -f database/schema.sql 2>&1 | grep -v "NOTICE" || true
echo "[db] applying seed_geo.sql";  $APSQL -f database/seed_geo.sql
echo "[db] applying seed.sql";      $APSQL -f database/seed.sql
echo "[db] applying demo_data.sql (demo citizen accounts)"; $APSQL -f database/demo_data.sql
$APSQL -At -c "SELECT 'states='||(SELECT COUNT(*) FROM states)||' districts='||(SELECT COUNT(*) FROM districts)||' cities='||(SELECT COUNT(*) FROM cities)||' users='||(SELECT COUNT(*) FROM users);"
echo "[db] ready: postgresql://$APPUSER@localhost:$PORT/$DB"
