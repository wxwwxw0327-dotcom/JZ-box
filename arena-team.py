#!/usr/bin/env python3
"""arena-team: send a branch of the school team's repository to the arena platform, play it, read the numbers.

Needs Python 3 only. Works on Windows, macOS and Linux.

  python3 arena-team.py login --server http://10.140.20.127:8790     paste your personal token once (it is not shown)
  python3 arena-team.py whoami
  python3 arena-team.py run --why "what this change should prove" --matches 20 --minutes 5 --wait
  python3 arena-team.py run --continue 12 --matches 40 --wait        more matches for run 12 (no new build)
  python3 arena-team.py status 12 [--wait]
  python3 arena-team.py branches
  python3 arena-team.py note --verdict 有效 --text "one sentence"      a draft; you confirm it on the web page

`run` takes the current branch of the repository you are in, checks that its last commit is on GitHub, and asks the
platform to build exactly that commit (simulation and real robot) and play it against the baseline. Output is JSON.
The token is kept in ~/.arena-team.json (readable by you only). Never print it, never commit it.
"""
import argparse
import getpass
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

CONF = Path.home() / ".arena-team.json"


def fail(msg, code=1):
    print(json.dumps({"error": msg}, ensure_ascii=False))
    sys.exit(code)


def conf():
    if not CONF.exists():
        fail("还没有登录。先运行：python3 arena-team.py login --server <平台地址>")
    return json.loads(CONF.read_text(encoding="utf-8"))


def call(method, path, body=None, timeout=90):
    c = conf()
    req = urllib.request.Request(c["server"].rstrip("/") + path, method=method, data=None if body is None else json.dumps(body).encode(),
                                 headers={"Authorization": "Bearer " + c["token"], "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read() or b"null")
    except urllib.error.HTTPError as e:
        try:
            detail = json.loads(e.read()).get("detail")
        except Exception:      # noqa: BLE001
            detail = None
        fail(f"{e.code} {detail or e.reason}", 2)
    except (urllib.error.URLError, TimeoutError) as e:
        fail(f"连不上平台：{getattr(e, 'reason', e)}", 3)


def git(*args):
    p = subprocess.run(["git", *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return p.returncode, p.stdout.strip()


def brief(run):
    """The part of a run an agent needs: builds, progress, result with verdicts."""
    out = {"run": run["id"], "status": run["status"], "progress": run["progress"], "why": run.get("why"), "minutes_per_match": round(run["seconds"] / 60, 1),
           "a": {"branch": run["a"]["ref"], "commit": run["a"]["sha"], "build": run["a"]["status"]},
           "b": {"branch": run["b"]["ref"], "commit": run["b"]["sha"], "build": run["b"]["status"], "baseline": run["b"].get("baseline")}}
    for side in ("a", "b"):
        d = run[side].get("detail") or {}
        if run[side]["status"] == "failed":
            out[side]["error"] = {k: d.get(k) for k in ("stage", "message", "errors") if d.get(k)}
        if d.get("compat"):
            out[side]["compat"] = d["compat"]
    if run.get("error"):
        out["error"] = run["error"]
    if run.get("waiting"):          # the teacher paused the team's runs, or it is outside the allowed hours
        out["waiting"] = run["waiting"]
    if run.get("result"):
        out["result"] = run["result"]
    return out


def wait(rid):
    last = None
    while True:
        run = call("GET", f"/api/squad/runs/{rid}?wait=50", timeout=80)
        mark = (run["status"], run["progress"]["done"], run["a"]["status"], run["b"]["status"])
        if mark != last:
            print(f"… {run['status']}  builds {run['a']['status']}/{run['b']['status']}  matches {run['progress']['done']}/{run['progress']['total']}", file=sys.stderr, flush=True)
            last = mark
        if run["status"] in ("done", "failed", "cancelled"):
            return run
        if run.get("waiting") and run["progress"]["running"] == 0:      # nothing will happen until the teacher resumes: do not hang
            print("… " + run["waiting"], file=sys.stderr, flush=True)
            return run
        time.sleep(1)


def main():
    ap = argparse.ArgumentParser(prog="arena-team", description="School team channel of the arena platform")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("login"); p.add_argument("--server", required=True)
    sub.add_parser("whoami")
    p = sub.add_parser("run")
    p.add_argument("--branch", help="default: the current branch of this repository")
    p.add_argument("--vs", default="baseline", help="baseline (default), or another branch / full commit id")
    p.add_argument("--matches", type=int, default=20)
    p.add_argument("--minutes", type=int, choices=[2, 5, 10], help="length of each match (default: the platform's, usually 2)")
    p.add_argument("--seconds", type=int, help=argparse.SUPPRESS)
    p.add_argument("--why", default="")
    p.add_argument("--scenario", choices=["keeper-shots"], help="a drill instead of full matches: keeper-shots = only the keeper, seeded shots; each version plays every drill once")
    p.add_argument("--continue", dest="cont", type=int, help="add matches to this run")
    p.add_argument("--no-git", action="store_true", help="send the branch name as it is, without looking at the local repository")
    p.add_argument("--wait", action="store_true")
    p = sub.add_parser("status"); p.add_argument("run", type=int); p.add_argument("--wait", action="store_true")
    sub.add_parser("branches")
    p = sub.add_parser("note"); p.add_argument("--branch"); p.add_argument("--verdict", choices=["有效", "无效", "待定"]); p.add_argument("--text"); p.add_argument("--hypothesis")
    a = ap.parse_args()

    if a.cmd == "login":
        token = os.environ.get("ARENA_TEAM_TOKEN") or getpass.getpass("个人令牌（输入不会显示）：").strip()
        if not token.startswith("mdfz_"):
            fail("令牌以 mdfz_ 开头。在平台的 /squad 页面“令牌和 agent”里生成")
        CONF.write_text(json.dumps({"server": a.server.rstrip("/"), "token": token}), encoding="utf-8")
        try:
            os.chmod(CONF, 0o600)
        except OSError:
            pass
        me = call("GET", "/api/squad/me")
        print(json.dumps({"ok": True, "name": me["name"], "quota": me["quota"]}, ensure_ascii=False))
    elif a.cmd == "whoami":
        print(json.dumps(call("GET", "/api/squad/me"), ensure_ascii=False, indent=2))
    elif a.cmd == "run":
        if a.cont:
            body = {"cont": a.cont, "matches": a.matches}
        else:
            body = {"vs": a.vs, "matches": a.matches, "why": a.why, "scenario": a.scenario}
            if a.minutes or a.seconds:
                body["seconds"] = a.minutes * 60 if a.minutes else a.seconds
            branch = a.branch
            if a.no_git:
                if not branch:
                    fail("--no-git 需要 --branch")
                body["ref"] = branch
            else:
                if not branch:
                    rc, branch = git("rev-parse", "--abbrev-ref", "HEAD")
                    if rc or branch == "HEAD":
                        fail("不在一个分支上。用 --branch 指定，或先切到分支")
                rc, sha = git("rev-parse", branch)
                if rc:
                    fail(f"本地没有分支 {branch}")
                git("fetch", "-q", "origin", branch)
                rc, remote = git("rev-parse", f"origin/{branch}")
                if rc or remote != sha:
                    fail(f"分支 {branch} 的最新提交 {sha[:8]} 还没有推到 GitHub。先 git push origin {branch}")
                body.update(ref=sha, branch=branch)
        run = call("POST", "/api/squad/runs", body)
        print(json.dumps(brief(wait(run["id"]) if a.wait else run), ensure_ascii=False, indent=2))
    elif a.cmd == "status":
        print(json.dumps(brief(wait(a.run) if a.wait else call("GET", f"/api/squad/runs/{a.run}")), ensure_ascii=False, indent=2))
    elif a.cmd == "branches":
        print(json.dumps(call("GET", "/api/squad/branches"), ensure_ascii=False, indent=2))
    elif a.cmd == "note":
        branch = a.branch or git("rev-parse", "--abbrev-ref", "HEAD")[1]
        body = {"branch": branch, "verdict": a.verdict, "text": a.text, "hypothesis": a.hypothesis}
        print(json.dumps(call("POST", "/api/squad/note", {k: v for k, v in body.items() if v is not None}), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
