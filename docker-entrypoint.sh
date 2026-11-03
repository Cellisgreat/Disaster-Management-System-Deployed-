#!/bin/sh
set -eu

DATA_DIR="${DMS_DATA_DIR:-/data}"
mkdir -p "$DATA_DIR"

# Initialize a new persistent volume without overwriting existing user data.
for file in /app/seed-data/*.csv; do
    [ -f "$file" ] || continue
    name="$(basename "$file")"
    if [ ! -e "$DATA_DIR/$name" ]; then
        cp "$file" "$DATA_DIR/$name"
    fi
done

PORT="${PORT:-8080}"
HOST="${HOST:-0.0.0.0}"
exec /app/DisasterManagementServer --host "$HOST" --port "$PORT" --data-dir "$DATA_DIR" --public-dir /app/public
