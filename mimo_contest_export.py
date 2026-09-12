#!/usr/bin/env python3
"""
MiMo Desktop -> OpenVela Contest Log Exporter

Reads conversation history from mimocode.db and exports to contest JSONL format.
Designed to work alongside the contest-log-collector system.

Usage:
    python mimo_contest_export.py                     # Export latest session
    python mimo_contest_export.py --session <id>      # Export specific session
    python mimo_contest_export.py --list               # List recent sessions
    python mimo_contest_export.py --all                # Export all sessions today
"""

import sqlite3
import json
import os
import sys
import argparse
from datetime import datetime, timezone
from pathlib import Path

# Configuration
MIMO_DB = Path.home() / ".local" / "share" / "mimocode" / "mimocode.db"
ENV_FILE = Path.home() / ".claude" / "contest-collector.env"

# Default contest repo location (openvela workspace)
DEFAULT_WORKSPACE = Path(os.environ.get(
    "OPENVELA_WORKSPACE",
    r"C:\Users\Administrator\Desktop\openvelageminis1"
))


def load_contest_env():
    """Load TEAM_ID and GITHUB_LOGIN from contest-collector.env"""
    team_id = ""
    github_login = ""

    if ENV_FILE.exists():
        with open(ENV_FILE, "r") as f:
            for line in f:
                line = line.strip()
                if line.startswith("TEAM_ID="):
                    team_id = line.split("=", 1)[1]
                elif line.startswith("GITHUB_LOGIN="):
                    github_login = line.split("=", 1)[1]

    if not team_id or not github_login:
        print("WARNING: contest-collector.env not found or incomplete", file=sys.stderr)
        print(f"  Expected at: {ENV_FILE}", file=sys.stderr)
        # Fallback values
        team_id = team_id or "contest2026_037_TensorFishSpecialActivityDivison"
        github_login = github_login or "LR-C11"

    return team_id, github_login


def get_db():
    """Connect to mimocode.db"""
    if not MIMO_DB.exists():
        print(f"ERROR: Database not found at {MIMO_DB}", file=sys.stderr)
        sys.exit(1)
    return sqlite3.connect(str(MIMO_DB))


def list_sessions(limit=10):
    """List recent sessions"""
    db = get_db()
    cursor = db.cursor()
    cursor.execute("""
        SELECT id, title, time_created, time_updated
        FROM session
        WHERE title IS NOT NULL AND title != ''
        ORDER BY time_updated DESC
        LIMIT ?
    """, (limit,))
    sessions = cursor.fetchall()
    db.close()

    print(f"{'Session ID':<45} {'Updated':<22} {'Title'}")
    print("-" * 100)
    for sid, title, created, updated in sessions:
        ts = datetime.fromtimestamp(updated / 1000, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        title_short = (title[:40] + "...") if len(title) > 40 else title
        print(f"{sid:<45} {ts:<22} {title_short}")


def export_session(session_id=None, workspace=None):
    """Export a session to contest JSONL format"""
    team_id, github_login = load_contest_env()
    db = get_db()
    cursor = db.cursor()

    # Find session
    if session_id:
        cursor.execute("SELECT id, title, time_created FROM session WHERE id = ?", (session_id,))
    else:
        cursor.execute("""
            SELECT id, title, time_created
            FROM session
            WHERE title IS NOT NULL AND title != ''
            ORDER BY time_updated DESC
            LIMIT 1
        """)
    session = cursor.fetchone()
    if not session:
        print("ERROR: No session found", file=sys.stderr)
        db.close()
        return False

    sid, title, created = session
    print(f"Exporting session: {sid}")
    print(f"  Title: {title}")

    # Get all messages
    cursor.execute("""
        SELECT id, agent_id, time_created, data
        FROM message
        WHERE session_id = ?
        ORDER BY time_created
    """, (sid,))
    messages = cursor.fetchall()

    # Determine output path
    if workspace is None:
        workspace = DEFAULT_WORKSPACE
    workspace = Path(workspace)

    # Find demo repo with .repo directory
    demo_repo = None
    for item in workspace.iterdir():
        if (item / ".repo").exists() and item.is_dir():
            demo_repo = item
            break
    if demo_repo is None:
        # Try parent
        for item in workspace.parent.iterdir():
            if (item / ".repo").exists() and item.is_dir():
                demo_repo = item
                break

    if demo_repo:
        log_dir = demo_repo / "logs" / github_login
    else:
        log_dir = workspace / "logs" / github_login

    today = datetime.now().strftime("%Y-%m-%d")
    log_dir = log_dir / today
    log_dir.mkdir(parents=True, exist_ok=True)

    output_file = log_dir / f"mimo-desktop__{sid}.jsonl"

    # Generate JSONL
    seq = 0
    entries = []

    for msg_id, agent_id, time_created, data_str in messages:
        data = json.loads(data_str)
        role = data.get("role", "unknown")
        ts = datetime.fromtimestamp(time_created / 1000, tz=timezone.utc).isoformat()

        # Get parts for this message
        cursor.execute("""
            SELECT data FROM part
            WHERE message_id = ?
            ORDER BY time_created
        """, (msg_id,))
        parts = cursor.fetchall()

        # Extract text content
        text_parts = []
        thinking_parts = []
        tool_calls = []
        model_info = {}
        tokens_in = 0
        tokens_out = 0

        for (part_data_str,) in parts:
            part = json.loads(part_data_str)
            ptype = part.get("type", "")

            if ptype == "text":
                text = part.get("text", "")
                # Skip system reminders for cleaner logs
                if not text.startswith("<system-reminder>"):
                    text_parts.append(text)

            elif ptype == "reasoning":
                thinking_parts.append(part.get("text", ""))

            elif ptype == "tool":
                tool_name = part.get("tool", "")
                status = part.get("status", "")
                input_data = part.get("input", {})
                output_data = part.get("output", "")
                tool_calls.append({
                    "name": tool_name,
                    "input": input_data,
                    "output": str(output_data)[:500] if output_data else ""
                })

            elif ptype == "step-finish":
                tokens = part.get("tokens", {})
                tokens_in += tokens.get("input", 0)
                tokens_out += tokens.get("output", 0)

        # Get model info from first message
        model_data = data.get("model", {})
        if model_data:
            model_info = {
                "provider": model_data.get("providerID", ""),
                "model": model_data.get("modelID", "")
            }

        # Build entry based on role
        if role == "user":
            text = "\n".join(text_parts)
            if text.strip():
                entries.append({
                    "schema_version": "1.0",
                    "session_id": sid,
                    "team_id": team_id,
                    "github_login": github_login,
                    "tool": "mimo-desktop",
                    "seq": seq,
                    "ts": ts,
                    "role": "user",
                    "text": text
                })
                seq += 1

        elif role == "assistant":
            # Thinking/reasoning
            if thinking_parts:
                entries.append({
                    "schema_version": "1.0",
                    "session_id": sid,
                    "team_id": team_id,
                    "github_login": github_login,
                    "tool": "mimo-desktop",
                    "seq": seq,
                    "ts": ts,
                    "role": "assistant",
                    "thinking": "\n".join(thinking_parts),
                    "model": model_info.get("model", ""),
                    "tokens_in": tokens_in,
                    "tokens_out": tokens_out
                })
                seq += 1

            # Text response
            text = "\n".join(text_parts)
            if text.strip():
                entries.append({
                    "schema_version": "1.0",
                    "session_id": sid,
                    "team_id": team_id,
                    "github_login": github_login,
                    "tool": "mimo-desktop",
                    "seq": seq,
                    "ts": ts,
                    "role": "assistant",
                    "text": text,
                    "model": model_info.get("model", ""),
                    "tokens_in": tokens_in,
                    "tokens_out": tokens_out
                })
                seq += 1

            # Tool calls
            for tc in tool_calls:
                entries.append({
                    "schema_version": "1.0",
                    "session_id": sid,
                    "team_id": team_id,
                    "github_login": github_login,
                    "tool": "mimo-desktop",
                    "seq": seq,
                    "ts": ts,
                    "role": "assistant",
                    "tool_name": tc["name"],
                    "input": json.dumps(tc["input"], ensure_ascii=False)[:2000],
                    "output": tc["output"]
                })
                seq += 1

    db.close()

    # Write JSONL
    with open(output_file, "w", encoding="utf-8") as f:
        for entry in entries:
            f.write(json.dumps(entry, ensure_ascii=False) + "\n")

    print(f"  Exported {seq} entries to: {output_file}")
    print(f"  Session ID: {sid}")
    return True


def main():
    parser = argparse.ArgumentParser(description="Export MiMo Desktop sessions to contest JSONL format")
    parser.add_argument("--session", help="Export specific session ID")
    parser.add_argument("--list", action="store_true", help="List recent sessions")
    parser.add_argument("--all", action="store_true", help="Export all sessions today")
    parser.add_argument("--workspace", help="OpenVela workspace path")
    parser.add_argument("--limit", type=int, default=10, help="Number of sessions to list")
    args = parser.parse_args()

    if args.list:
        list_sessions(args.limit)
        return

    if args.all:
        db = get_db()
        cursor = db.cursor()
        today_start = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
        today_ts = int(today_start.timestamp() * 1000)
        cursor.execute("""
            SELECT id FROM session
            WHERE time_updated >= ? AND title IS NOT NULL AND title != ''
            ORDER BY time_updated DESC
        """, (today_ts,))
        sessions = cursor.fetchall()
        db.close()

        for (sid,) in sessions:
            export_session(sid, args.workspace)
        return

    export_session(args.session, args.workspace)


if __name__ == "__main__":
    main()
