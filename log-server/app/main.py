from datetime import datetime, timezone
from pathlib import Path
import sqlite3

from fastapi import FastAPI, Query
from fastapi.responses import HTMLResponse
from pydantic import BaseModel

from fastapi.templating import Jinja2Templates
from fastapi import Request

from fastapi.staticfiles import StaticFiles

DB_PATH = Path("/app/data/logs.db")

app = FastAPI(title="eTomada Log Server")

app.mount(
    "/static",
    StaticFiles(directory="/app/app/static"),
    name="static"
)

templates = Jinja2Templates(
    directory="/app/app/templates"
)

class LogEntry(BaseModel):
    deviceID: str
    level: int
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
            level INTEGER,
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

    conn.execute("""
        CREATE TABLE IF NOT EXISTS level (
            id INTEGER,
            nome TEXT NOT NULL
        )
    """)

    conn.execute("""
        DELETE FROM level;
    """)
    conn.execute("""
        INSERT INTO level(id, nome) VALUES
            (  0, "!OFF!!"),
            (  1, "!CRIT!"),
            (  5, "AVISO!"),
            ( 10, "NORMAL"),
            ( 50, "DEBUG0"),
            ( 70, "DEBUG!"),
            (100, "TESTE!");
    """)

    conn.commit()
    conn.close()


@app.on_event("startup")
def startup():
    init_db()


@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse(
        request=request,
        name="index.html",
        context={}
    )


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
    search: str | None = None,
    limit: int = Query(250, ge=1, le=10000)
):

    conn = get_db()

    query = """
        SELECT
            l.id,
            l.timestamp,
            l.device_id,
            lv.nome AS level,
            l.module,
            l.message,
            l.uptime
        FROM logs l
        JOIN level lv ON l.level = lv.id
        WHERE 1=1
    """

    params = []

    if deviceID:
        query += " AND l.device_id = ?"
        params.append(deviceID)

    if level:
        query += " AND lv.nome = ?"
        params.append(level)

    if module:
        query += " AND l.module = ?"
        params.append(module)

    if search:
        query += " AND l.message LIKE ?"
        params.append(f"%{search}%")

    query += " ORDER BY l.id DESC LIMIT ?"
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

