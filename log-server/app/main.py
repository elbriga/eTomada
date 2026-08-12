from datetime import datetime, timezone
from pathlib import Path
import sqlite3

from fastapi import FastAPI, Query
from fastapi.responses import HTMLResponse
from pydantic import BaseModel


DB_PATH = Path("/app/data/logs.db")

app = FastAPI(title="eTomada Log Server")


class LogEntry(BaseModel):
    deviceID: str
    level: str = "INFO"
    module: str = ""
    message: str
    uptime: int | None = None
    timestamp: int | None = None


def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)

    conn = get_db()

    conn.execute("""
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER,
            device_id TEXT NOT NULL,
            level TEXT NOT NULL,
            module TEXT,
            message TEXT NOT NULL,
            uptime INTEGER
        )
    """)

    conn.execute("""
        CREATE INDEX IF NOT EXISTS idx_logs_device
        ON logs(device_id)
    """)

    conn.execute("""
        CREATE INDEX IF NOT EXISTS idx_logs_timestamp
        ON logs(timestamp)
    """)

    conn.commit()
    conn.close()


@app.on_event("startup")
def startup():
    init_db()


@app.get("/", response_class=HTMLResponse)
def index():
    return """
    <html>
        <head>
            <title>eTomada Logs</title>
        </head>
        <body>
            <h1>eTomada Log Server</h1>
            <p>Servidor funcionando.</p>
            <p>
                <a href="/docs">API</a>
            </p>
        </body>
    </html>
    """


@app.post("/api/log")
def receive_log(log: LogEntry):

    conn = get_db()

    cursor = conn.execute("""
        INSERT INTO logs (
            timestamp,
            device_id,
            level,
            module,
            message,
            uptime
        )
        VALUES (?, ?, ?, ?, ?, ?)
    """, (
        log.timestamp,
        log.deviceID,
        log.level,
        log.module,
        log.message,
        log.uptime
    ))

    conn.commit()

    log_id = cursor.lastrowid

    conn.close()

    return {
        "ok": True,
        "id": log_id
    }


@app.get("/api/logs")
def get_logs(
    deviceID: str | None = None,
    level: str | None = None,
    module: str | None = None,
    limit: int = Query(100, ge=1, le=1000)
):

    conn = get_db()

    query = """
        SELECT
            id,
            timestamp,
            device_id,
            level,
            module,
            message,
            uptime
        FROM logs
        WHERE 1=1
    """

    params = []

    if deviceID:
        query += " AND device_id = ?"
        params.append(deviceID)

    if level:
        query += " AND level = ?"
        params.append(level)

    if module:
        query += " AND module = ?"
        params.append(module)

    query += " ORDER BY id DESC LIMIT ?"
    params.append(limit)

    rows = conn.execute(query, params).fetchall()

    conn.close()

    return [dict(row) for row in rows]


@app.get("/api/nodes")
def get_nodes():

    conn = get_db()

    rows = conn.execute("""
        SELECT
            device_id,
            MAX(timestamp) AS last_log,
            COUNT(*) AS total_logs
        FROM logs
        GROUP BY device_id
        ORDER BY device_id
    """).fetchall()

    conn.close()

    return [dict(row) for row in rows]

