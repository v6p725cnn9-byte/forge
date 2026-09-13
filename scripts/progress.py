#!/usr/bin/env python3
"""Read and close TOMLMD tasks in README.md (Python 3.9+, no tomllib)."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Dict, List, Optional, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
FENCE = "+++"

VALID = ("todo", "doing", "done", "blocked")


def split_readme(text: str) -> Tuple[str, str, str]:
    if not text.startswith(FENCE):
        raise SystemExit("README.md must start with +++ TOMLMD frontmatter")
    rest = text[len(FENCE) :]
    if rest.startswith("\n"):
        rest = rest[1:]
    end = rest.find("\n" + FENCE + "\n")
    if end < 0:
        raise SystemExit("README.md is missing the closing +++ of TOMLMD")
    toml = rest[:end]
    body = rest[end + len("\n" + FENCE + "\n") :]
    return FENCE + "\n", toml, body


def parse_tables(toml: str, kind: str) -> List[Dict[str, str]]:
    tables: List[Dict[str, str]] = []
    current: Optional[Dict[str, str]] = None
    header = f"[[{kind}]]"
    for raw in toml.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[[") and line.endswith("]]"):
            current = {} if line == header else None
            if current is not None:
                tables.append(current)
            continue
        if current is None or "=" not in line:
            continue
        key, value = line.split("=", 1)
        current[key.strip()] = value.strip().strip('"')
    return tables


def set_status(toml: str, task_id: str, status: str) -> str:
    lines = toml.splitlines()
    in_tasks = False
    this_task = False
    found = False
    out: List[str] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[[") and stripped.endswith("]]"):
            in_tasks = stripped == "[[tasks]]"
            this_task = False
        if in_tasks and re.match(r'^id\s*=\s*"' + re.escape(task_id) + r'"\s*$', stripped):
            this_task = True
        if this_task and re.match(r"^status\s*=", stripped):
            indent = line[: len(line) - len(line.lstrip())]
            line = f'{indent}status = "{status}"'
            found = True
            this_task = False
        out.append(line)
    if not found:
        raise SystemExit(f"unknown task id: {task_id}")
    return "\n".join(out) + ("\n" if toml.endswith("\n") else "")


def sync_checkboxes(body: str, task_id: str, status: str) -> str:
    mark = "x" if status == "done" else " "
    pattern = re.compile(
        r"^(\s*[-*]\s*\[)[ xX](\]\s+`" + re.escape(task_id) + r"`)",
        re.MULTILINE,
    )
    updated, n = pattern.subn(rf"\g<1>{mark}\2", body)
    if n == 0:
        raise SystemExit(f"no checkbox for `{task_id}` in README body")
    return updated


def maybe_close_milestones(toml: str) -> str:
    tasks = parse_tables(toml, "tasks")
    by_ms: Dict[str, List[str]] = {}
    for task in tasks:
        by_ms.setdefault(task.get("milestone", ""), []).append(task.get("status", "todo"))
    lines = toml.splitlines()
    in_ms = False
    ms_id = ""
    out: List[str] = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[[") and stripped.endswith("]]"):
            in_ms = stripped == "[[milestones]]"
            ms_id = ""
        if in_ms and stripped.startswith("id"):
            ms_id = stripped.split("=", 1)[1].strip().strip('"')
        if in_ms and ms_id and re.match(r"^status\s*=", stripped):
            statuses = by_ms.get(ms_id, [])
            if statuses and all(s == "done" for s in statuses):
                new = "done"
            elif any(s == "doing" for s in statuses):
                new = "doing"
            elif any(s == "done" for s in statuses):
                new = "doing"
            else:
                new = "todo"
            indent = line[: len(line) - len(line.lstrip())]
            line = f'{indent}status = "{new}"'
        out.append(line)
    return "\n".join(out) + ("\n" if toml.endswith("\n") else "")


def write_readme(prefix: str, toml: str, body: str) -> None:
    README.write_text(prefix + toml.rstrip() + "\n" + FENCE + "\n" + body, encoding="utf-8")


def cmd_list(toml: str) -> int:
    tasks = parse_tables(toml, "tasks")
    if not tasks:
        print("no [[tasks]] in README")
        return 0
    width = max(len(t.get("id", "")) for t in tasks)
    done = sum(1 for t in tasks if t.get("status") == "done")
    print(f"{done}/{len(tasks)} done\n")
    current_ms = None
    for task in tasks:
        ms = task.get("milestone", "")
        if ms != current_ms:
            current_ms = ms
            print(f"[{ms}]")
        status = task.get("status", "todo")
        mark = {"done": "x", "doing": ">", "blocked": "!", "todo": " "}.get(status, "?")
        print(f"  [{mark}] {task.get('id', ''):<{width}}  {task.get('title', '')}")
    return 0


def cmd_set(task_id: str, status: str) -> int:
    if status not in VALID:
        raise SystemExit(f"status must be one of {', '.join(VALID)}")
    text = README.read_text(encoding="utf-8")
    prefix, toml, body = split_readme(text)
    toml = set_status(toml, task_id, status)
    toml = maybe_close_milestones(toml)
    body = sync_checkboxes(body, task_id, status)
    write_readme(prefix, toml, body)
    print(f"{task_id} -> {status}")
    return 0


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Forge TOMLMD progress")
    parser.add_argument("command", nargs="?", default="list", choices=["list", "close", "reopen", "doing", "block"])
    parser.add_argument("task_id", nargs="?")
    args = parser.parse_args(argv)

    if args.command == "list":
        _, toml, _ = split_readme(README.read_text(encoding="utf-8"))
        return cmd_list(toml)

    if not args.task_id:
        raise SystemExit(f"{args.command} needs a task id")

    status = {
        "close": "done",
        "reopen": "todo",
        "doing": "doing",
        "block": "blocked",
    }[args.command]
    return cmd_set(args.task_id, status)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
