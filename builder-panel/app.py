#!/usr/bin/env python3
import base64
import json
import mimetypes
import os
import re
import sqlite3
import subprocess
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, quote, unquote, urlparse


HOST = os.environ.get("PANEL_HOST", "127.0.0.1")
PORT = int(os.environ.get("PANEL_PORT", "18080"))
BASE_DIR = Path(os.environ.get("PANEL_BASE", "~/redmik40-build")).expanduser()
PROJECT_DIR = Path(os.environ.get("PANEL_PROJECT_DIR", BASE_DIR / "project")).expanduser()
SOURCE_DIR = Path(os.environ.get("PANEL_SOURCE_DIR", BASE_DIR / "lineage-sm8250")).expanduser()
KERNEL_DIR = Path(os.environ.get("PANEL_KERNEL_DIR", SOURCE_DIR / "kernel")).expanduser()
OUT_ROOT = Path(os.environ.get("PANEL_OUT_ROOT", SOURCE_DIR / "out")).expanduser()
ARTIFACT_ROOT = Path(os.environ.get("PANEL_ARTIFACT_ROOT", SOURCE_DIR / "artifacts")).expanduser()
LOG_DIR = Path(os.environ.get("PANEL_LOG_DIR", BASE_DIR / "logs")).expanduser()
DB_PATH = Path(os.environ.get("PANEL_DB", BASE_DIR / "panel" / "panel.sqlite3")).expanduser()
BUILD_SCRIPT = Path(
    os.environ.get("PANEL_BUILD_SCRIPT", PROJECT_DIR / "scripts" / "build_lineage_sm8250_alioth_kernel.sh")
).expanduser()
BUILDER_IMAGE = os.environ.get("PANEL_BUILDER_IMAGE", "redmik40-kernel-builder:bookworm")
DEFAULT_BRANCH = os.environ.get("PANEL_DEFAULT_BRANCH", "lineage-20-headless")
DEFAULT_JOBS = int(os.environ.get("PANEL_DEFAULT_JOBS", os.environ.get("JOBS", "36")))
AUTH_USER = os.environ.get("PANEL_BASIC_USER")
AUTH_PASSWORD = os.environ.get("PANEL_BASIC_PASSWORD")
AUTH_USER_FILE = os.environ.get("PANEL_BASIC_USER_FILE")
AUTH_PASSWORD_FILE = os.environ.get("PANEL_BASIC_PASSWORD_FILE")

if AUTH_USER_FILE:
    AUTH_USER = Path(AUTH_USER_FILE).expanduser().read_text(encoding="utf-8").strip()
if AUTH_PASSWORD_FILE:
    AUTH_PASSWORD = Path(AUTH_PASSWORD_FILE).expanduser().read_text(encoding="utf-8").strip()

PROJECTS = {
    "lineage-sm8250-alioth": {
        "label": "Lineage sm8250 alioth kernel",
        "source_dir": SOURCE_DIR,
        "kernel_dir": KERNEL_DIR,
        "out_dir": OUT_ROOT / "kernel-lineage20-alioth-panel",
        "artifact_root": ARTIFACT_ROOT,
        "build_script": BUILD_SCRIPT,
    }
}

BRANCH_RE = re.compile(r"^[A-Za-z0-9._/@-]{1,96}$")
SAFE_NAME_RE = re.compile(r"^[A-Za-z0-9._@+-]{1,160}$")
STATE_LOCK = threading.Lock()


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def ensure_dirs():
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    ARTIFACT_ROOT.mkdir(parents=True, exist_ok=True)


def connect_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    ensure_dirs()
    with connect_db() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS tasks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                status TEXT NOT NULL,
                project TEXT NOT NULL,
                branch TEXT NOT NULL,
                jobs INTEGER,
                sync_first INTEGER NOT NULL DEFAULT 0,
                systemd_config INTEGER NOT NULL DEFAULT 1,
                log_path TEXT NOT NULL,
                artifact_dir TEXT,
                exit_code INTEGER,
                error TEXT,
                command_json TEXT,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                started_at TEXT,
                finished_at TEXT
            )
            """
        )
        conn.execute(
            "UPDATE tasks SET status='stale', updated_at=? WHERE status IN ('queued', 'running')",
            (utc_now(),),
        )


def row_to_dict(row):
    if row is None:
        return None
    data = dict(row)
    if data.get("command_json"):
        try:
            data["command"] = json.loads(data["command_json"])
        except json.JSONDecodeError:
            data["command"] = data["command_json"]
    data.pop("command_json", None)
    return data


def validate_project(project):
    if project not in PROJECTS:
        raise ValueError("unknown project")
    return project


def validate_branch(branch):
    branch = (branch or "").strip()
    if not BRANCH_RE.fullmatch(branch):
        raise ValueError("invalid branch name")
    if branch.startswith("-") or branch.endswith("/") or ".." in branch or "//" in branch or "@{" in branch:
        raise ValueError("unsafe branch name")
    return branch


def validate_safe_name(value, label):
    value = unquote(value or "")
    if not SAFE_NAME_RE.fullmatch(value):
        raise ValueError(f"invalid {label}")
    return value


def run_logged(task_id, args, cwd, env=None, timeout=None):
    with connect_db() as conn:
        row = conn.execute("SELECT log_path FROM tasks WHERE id=?", (task_id,)).fetchone()
    if row is None:
        raise RuntimeError("task disappeared")

    log_path = Path(row["log_path"])
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.time()

    with log_path.open("a", encoding="utf-8", errors="replace") as log:
        log.write("\n")
        log.write(f"[{utc_now()}] cwd={cwd}\n")
        log.write(f"[{utc_now()}] cmd={' '.join(args)}\n")
        log.flush()
        proc = subprocess.Popen(
            args,
            cwd=str(cwd),
            env=env,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            code = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = proc.wait()
            log.write(f"[{utc_now()}] timeout after {timeout}s\n")
        elapsed = time.time() - started
        log.write(f"[{utc_now()}] exit={code} elapsed={elapsed:.1f}s\n")
        log.flush()
    if code != 0:
        raise subprocess.CalledProcessError(code, args)


def quick_cmd(args, cwd, timeout=8):
    try:
        out = subprocess.check_output(
            args,
            cwd=str(cwd),
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
        )
        return {"ok": True, "output": out.strip()}
    except Exception as exc:
        output = getattr(exc, "output", "")
        return {"ok": False, "output": str(output).strip() or str(exc)}


def sync_repo(task_id, project, branch):
    cfg = PROJECTS[project]
    repo = cfg["kernel_dir"]
    status = quick_cmd(["git", "status", "--porcelain"], repo)
    if not status["ok"]:
        raise RuntimeError(status["output"])
    if status["output"]:
        raise RuntimeError("kernel repo is dirty; commit/stash before sync")

    run_logged(task_id, ["git", "fetch", "--prune", "origin"], repo)
    local_branch = quick_cmd(["git", "show-ref", "--verify", "--quiet", f"refs/heads/{branch}"], repo)
    if local_branch["ok"]:
        run_logged(task_id, ["git", "switch", branch], repo)
    else:
        run_logged(task_id, ["git", "switch", "--track", "-c", branch, f"origin/{branch}"], repo)
    run_logged(task_id, ["git", "pull", "--ff-only"], repo)
    run_logged(task_id, ["git", "rev-parse", "--short=12", "HEAD"], repo)


def build_kernel(task_id, project, branch, jobs, sync_first, systemd_config):
    cfg = PROJECTS[project]
    if sync_first:
        sync_repo(task_id, project, branch)

    artifact_name = f"panel-{branch.replace('/', '_')}-{task_id}"
    artifact_dir = cfg["artifact_root"] / artifact_name
    artifact_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env.update(
        {
            "WORK_DIR": str(cfg["source_dir"]),
            "KERNEL_DIR": str(cfg["kernel_dir"]),
            "OUT_DIR": str(cfg["out_dir"]),
            "ARTIFACT_DIR": str(artifact_dir),
            "BUILDER_IMAGE": BUILDER_IMAGE,
            "JOBS": str(jobs),
            "SYSTEMD_FRIENDLY_CONFIG": "yes" if systemd_config else "no",
        }
    )
    command = ["bash", str(cfg["build_script"])]
    with connect_db() as conn:
        conn.execute(
            "UPDATE tasks SET artifact_dir=?, command_json=?, updated_at=? WHERE id=?",
            (str(artifact_dir), json.dumps(command), utc_now(), task_id),
        )
    run_logged(task_id, command, PROJECT_DIR, env=env)


def task_worker(task_id):
    with connect_db() as conn:
        row = conn.execute("SELECT * FROM tasks WHERE id=?", (task_id,)).fetchone()
    if row is None:
        return
    task = row_to_dict(row)
    try:
        with connect_db() as conn:
            conn.execute(
                "UPDATE tasks SET status='running', started_at=?, updated_at=? WHERE id=?",
                (utc_now(), utc_now(), task_id),
            )
        if task["kind"] == "sync":
            sync_repo(task_id, task["project"], task["branch"])
        elif task["kind"] == "build":
            build_kernel(
                task_id,
                task["project"],
                task["branch"],
                int(task["jobs"] or DEFAULT_JOBS),
                bool(task["sync_first"]),
                bool(task["systemd_config"]),
            )
        else:
            raise RuntimeError("unknown task kind")
        with connect_db() as conn:
            conn.execute(
                "UPDATE tasks SET status='success', exit_code=0, finished_at=?, updated_at=? WHERE id=?",
                (utc_now(), utc_now(), task_id),
            )
    except Exception as exc:
        exit_code = getattr(exc, "returncode", 1) or 1
        with connect_db() as conn:
            conn.execute(
                "UPDATE tasks SET status='failed', exit_code=?, error=?, finished_at=?, updated_at=? WHERE id=?",
                (exit_code, str(exc), utc_now(), utc_now(), task_id),
            )
        with connect_db() as conn:
            log_row = conn.execute("SELECT log_path FROM tasks WHERE id=?", (task_id,)).fetchone()
        if log_row:
            with Path(log_row["log_path"]).open("a", encoding="utf-8", errors="replace") as log:
                log.write(f"\n[{utc_now()}] ERROR {exc}\n")


def has_active_task():
    with connect_db() as conn:
        row = conn.execute(
            "SELECT id FROM tasks WHERE status IN ('queued', 'running') ORDER BY id DESC LIMIT 1"
        ).fetchone()
    return row["id"] if row else None


def create_task(kind, project, branch, jobs=None, sync_first=False, systemd_config=True):
    project = validate_project(project)
    branch = validate_branch(branch)
    if jobs is not None:
        jobs = max(1, min(int(jobs), 72))

    with STATE_LOCK:
        active = has_active_task()
        if active:
            raise RuntimeError(f"task {active} is already active")
        now = utc_now()
        log_path = LOG_DIR / f"panel-{kind}-{now.replace(':', '').replace('-', '')}.log"
        with connect_db() as conn:
            cur = conn.execute(
                """
                INSERT INTO tasks (
                    kind, status, project, branch, jobs, sync_first, systemd_config,
                    log_path, created_at, updated_at
                ) VALUES (?, 'queued', ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    kind,
                    project,
                    branch,
                    jobs,
                    1 if sync_first else 0,
                    1 if systemd_config else 0,
                    str(log_path),
                    now,
                    now,
                ),
            )
            task_id = cur.lastrowid
        thread = threading.Thread(target=task_worker, args=(task_id,), daemon=True)
        thread.start()
        return task_id


def tail_file(path, limit=65536):
    p = Path(path)
    if not p.exists():
        return ""
    size = p.stat().st_size
    with p.open("rb") as fh:
        if size > limit:
            fh.seek(size - limit)
        data = fh.read()
    return data.decode("utf-8", errors="replace")


def list_artifacts():
    items = []
    if not ARTIFACT_ROOT.exists():
        return items
    for directory in sorted(ARTIFACT_ROOT.iterdir(), key=lambda p: p.stat().st_mtime, reverse=True):
        if not directory.is_dir() or not SAFE_NAME_RE.fullmatch(directory.name):
            continue
        files = []
        for path in sorted(directory.iterdir()):
            if not path.is_file() or not SAFE_NAME_RE.fullmatch(path.name):
                continue
            files.append(
                {
                    "name": path.name,
                    "size": path.stat().st_size,
                    "url": f"/download/{quote(directory.name)}/{quote(path.name)}",
                }
            )
        items.append(
            {
                "name": directory.name,
                "mtime": int(directory.stat().st_mtime),
                "files": files,
            }
        )
    return items[:30]


class Handler(BaseHTTPRequestHandler):
    server_version = "RedmiK40BuilderPanel/0.1"

    def log_message(self, fmt, *args):
        print(f"[{utc_now()}] {self.address_string()} {fmt % args}")

    def authorized(self):
        if not AUTH_USER and not AUTH_PASSWORD:
            return True
        expected = base64.b64encode(f"{AUTH_USER}:{AUTH_PASSWORD}".encode()).decode()
        return self.headers.get("Authorization") == f"Basic {expected}"

    def require_auth(self):
        if self.authorized():
            return True
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="builder-panel"')
        self.send_header("Content-Length", "0")
        self.end_headers()
        return False

    def send_json(self, status, payload):
        data = json.dumps(payload, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_GET(self):
        if not self.require_auth():
            return
        parsed = urlparse(self.path)
        try:
            if parsed.path == "/":
                return self.serve_index()
            if parsed.path == "/api/status":
                return self.api_status()
            if parsed.path == "/api/tasks":
                return self.api_tasks()
            if parsed.path.startswith("/api/tasks/"):
                task_id = int(parsed.path.rsplit("/", 1)[1])
                return self.api_task(task_id)
            if parsed.path == "/api/artifacts":
                return self.send_json(200, {"artifacts": list_artifacts()})
            if parsed.path.startswith("/download/"):
                return self.serve_download(parsed.path)
            self.send_error(404)
        except Exception as exc:
            self.send_json(500, {"error": str(exc)})

    def do_POST(self):
        if not self.require_auth():
            return
        parsed = urlparse(self.path)
        try:
            body = self.read_json()
            project = body.get("project") or "lineage-sm8250-alioth"
            branch = body.get("branch") or DEFAULT_BRANCH
            if parsed.path == "/api/sync":
                task_id = create_task("sync", project, branch)
                return self.send_json(202, {"task_id": task_id})
            if parsed.path == "/api/build":
                task_id = create_task(
                    "build",
                    project,
                    branch,
                    jobs=body.get("jobs", DEFAULT_JOBS),
                    sync_first=bool(body.get("sync_first", False)),
                    systemd_config=bool(body.get("systemd_config", True)),
                )
                return self.send_json(202, {"task_id": task_id})
            self.send_error(404)
        except RuntimeError as exc:
            self.send_json(409, {"error": str(exc)})
        except Exception as exc:
            self.send_json(400, {"error": str(exc)})

    def serve_index(self):
        index = Path(__file__).with_name("index.html")
        data = index.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def api_status(self):
        branch = quick_cmd(["git", "branch", "--show-current"], KERNEL_DIR)
        head = quick_cmd(["git", "rev-parse", "--short=12", "HEAD"], KERNEL_DIR)
        dirty = quick_cmd(["git", "status", "--porcelain"], KERNEL_DIR)
        image = quick_cmd(["docker", "image", "inspect", BUILDER_IMAGE, "--format", "{{.Id}} {{.Size}}"], PROJECT_DIR)
        active = has_active_task()
        self.send_json(
            200,
            {
                "host": HOST,
                "port": PORT,
                "base_dir": str(BASE_DIR),
                "project_dir": str(PROJECT_DIR),
                "kernel_dir": str(KERNEL_DIR),
                "artifact_root": str(ARTIFACT_ROOT),
                "default_branch": DEFAULT_BRANCH,
                "default_jobs": DEFAULT_JOBS,
                "branch": branch["output"] if branch["ok"] else None,
                "head": head["output"] if head["ok"] else None,
                "dirty_count": len(dirty["output"].splitlines()) if dirty["ok"] and dirty["output"] else 0,
                "builder_image": image["output"] if image["ok"] else None,
                "active_task": active,
                "projects": [{"id": key, "label": cfg["label"]} for key, cfg in PROJECTS.items()],
            },
        )

    def api_tasks(self):
        with connect_db() as conn:
            rows = conn.execute("SELECT * FROM tasks ORDER BY id DESC LIMIT 25").fetchall()
        self.send_json(200, {"tasks": [row_to_dict(row) for row in rows]})

    def api_task(self, task_id):
        with connect_db() as conn:
            row = conn.execute("SELECT * FROM tasks WHERE id=?", (task_id,)).fetchone()
        if row is None:
            return self.send_json(404, {"error": "not found"})
        task = row_to_dict(row)
        task["log_tail"] = tail_file(task["log_path"])
        self.send_json(200, {"task": task})

    def serve_download(self, path):
        parts = path.split("/")
        if len(parts) != 4:
            return self.send_error(404)
        artifact = validate_safe_name(parts[2], "artifact")
        filename = validate_safe_name(parts[3], "file")
        target = ARTIFACT_ROOT / artifact / filename
        if not target.is_file():
            return self.send_error(404)
        ctype = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
        data_size = target.stat().st_size
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(data_size))
        self.send_header("Content-Disposition", f'attachment; filename="{target.name}"')
        self.end_headers()
        with target.open("rb") as fh:
            while True:
                chunk = fh.read(1024 * 1024)
                if not chunk:
                    break
                self.wfile.write(chunk)


def main():
    init_db()
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"builder panel listening on http://{HOST}:{PORT}")
    print(f"base_dir={BASE_DIR}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
