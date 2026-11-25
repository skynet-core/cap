#!/usr/bin/env python3
"""Local CAP coding-agent benchmark. Standard library only. Trusted task manifests only."""

import argparse
import concurrent.futures
import datetime
import json
import pathlib
import re
import statistics
import subprocess
import threading
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]


def cmd(argv, cwd=None, timeout=120, env=None):
    try:
        p = subprocess.run(
            argv,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env,
            check=False,
        )
        return {
            "exit": p.returncode,
            "stdout": p.stdout[-12000:],
            "stderr": p.stderr[-12000:],
        }
    except (subprocess.TimeoutExpired, OSError) as e:
        return {"exit": -1, "stdout": "", "stderr": str(e)}


def get(url, timeout=3):
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return r.read().decode()
    except (OSError, UnicodeError, ValueError):
        return None


def gpu_sample():
    r = cmd(
        [
            "nvidia-smi",
            "--query-gpu=index,utilization.gpu,memory.used,memory.total",
            "--format=csv,noheader,nounits",
        ],
        timeout=5,
    )
    if r["exit"] != 0:
        return []
    out = []
    for line in r["stdout"].splitlines():
        try:
            a = [int(x.strip()) for x in line.split(",")]
            out.append(
                {
                    "index": a[0],
                    "utilization_pct": a[1],
                    "memory_used_mib": a[2],
                    "memory_total_mib": a[3],
                }
            )
        except (ValueError, IndexError):
            pass
    return out


def parse_prom(s):
    if not s:
        return {}
    wanted = (
        "vllm:gpu_cache_usage_perc",
        "vllm:kv_cache_usage_perc",
        "vllm:num_requests_running",
        "vllm:num_requests_waiting",
        "vllm:request_queue_time_seconds",
        "vllm:request_prefill_time_seconds",
        "vllm:request_decode_time_seconds",
    )
    d = {}
    for line in s.splitlines():
        if line.startswith("#") or not line.strip():
            continue
        match = re.match(r"^([^\s{]+)(?:\{[^}]*\})?\s+([-+\deE.]+)", line)
        if match and any(match.group(1).startswith(k) for k in wanted):
            try:
                d[match.group(1)] = float(match.group(2))
            except ValueError:
                pass
    return d


def monitor(stop, metrics_url, interval, output):
    with open(output, "w") as f:
        while not stop.is_set():
            row = {
                "ts": time.time(),
                "gpu": gpu_sample(),
                "vllm": parse_prom(get(metrics_url)),
            }
            f.write(json.dumps(row) + "\n")
            f.flush()
            stop.wait(interval)


def request(endpoint, model, messages, max_tokens, timeout):
    url = endpoint.rstrip("/") + "/chat/completions"
    data = json.dumps(
        {
            "model": model,
            "messages": messages,
            "temperature": 0.2,
            "max_tokens": max_tokens,
            "stream": False,
        }
    ).encode()
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}
    )
    start = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            result = json.load(resp)
        elapsed = time.monotonic() - start
        choice = result["choices"][0]["message"]
        content = choice.get("content") or ""
        usage = result.get("usage", {})
        return {
            "ok": True,
            "content": content,
            "latency_s": elapsed,
            "prompt_tokens": usage.get("prompt_tokens"),
            "completion_tokens": usage.get("completion_tokens"),
            "error": None,
        }
    except (
        OSError,
        UnicodeError,
        ValueError,
        KeyError,
        IndexError,
        TypeError,
        AttributeError,
    ) as e:
        return {
            "ok": False,
            "content": "",
            "latency_s": time.monotonic() - start,
            "prompt_tokens": None,
            "completion_tokens": None,
            "error": str(e),
        }


def patch_text(s):
    m = re.search(r"```(?:diff|patch)?\s*\n(.*?)```", s, re.DOTALL)
    return (m.group(1) if m else s).strip() + "\n"


def changed_paths(worktree):
    r = cmd(["git", "status", "--porcelain", "--untracked-files=all"], cwd=worktree)
    return [line[3:] for line in r["stdout"].splitlines() if len(line) > 3]


def allowed(path, patterns):
    p = pathlib.PurePosixPath(path)
    return (
        not p.is_absolute()
        and ".." not in p.parts
        and any(p.match(pattern) for pattern in patterns)
    )


def worker(task, config, run_dir):
    tid = task["id"]
    safe = re.sub("[^a-zA-Z0-9_-]", "_", tid)
    worktree = run_dir / "worktrees" / safe
    log = {
        "id": tid,
        "lane": task.get("lane"),
        "contract_revision": task.get("contract_revision"),
        "attempts": [],
        "accepted": False,
        "review": "not_run",
        "started": time.time(),
    }
    add = cmd(
        ["git", "worktree", "add", "--detach", str(worktree), config["revision"]],
        cwd=config["repo"],
        timeout=60,
    )
    if add["exit"] != 0:
        log["error"] = "worktree: " + add["stderr"]
        return log
    try:
        for attempt in range(1, config["attempts"] + 1):
            relevant = []
            for rel in task.get("context_files", []):
                p = worktree / rel
                if p.is_file():
                    relevant.append(
                        f"FILE {rel}:\n{p.read_text(errors='replace')[:16000]}"
                    )
            prompt = (
                "You are implementing one scoped CAP engineering task. Return ONLY a valid unified git diff "
                "beginning with diff --git, with no commentary. Do not modify files outside allowed paths. "
                "Do not change public contracts.\nTask: "
                + task["prompt"]
                + "\nAllowed paths: "
                + str(task["allowed_paths"])
                + "\nContract revision: "
                + str(task.get("contract_revision", "unspecified"))
                + "\n"
                + "\n".join(relevant)
            )
            if log["attempts"]:
                prev = log["attempts"][-1]
                prompt += (
                    "\nPREVIOUS FAILURE (correct it): "
                    + json.dumps(
                        {
                            "apply": prev.get("apply"),
                            "checks": prev.get("checks"),
                            "violations": prev.get("violations"),
                        }
                    )[-8000:]
                )
            response = request(
                config["endpoint"],
                config["model"],
                [{"role": "user", "content": prompt}],
                config["max_output_tokens"],
                config["timeout"],
            )
            record = {
                "attempt": attempt,
                "inference": {k: v for k, v in response.items() if k != "content"},
            }
            if not response["ok"]:
                record["error"] = "inference"
                log["attempts"].append(record)
                break
            patch = patch_text(response["content"])
            (run_dir / "patches").mkdir(exist_ok=True)
            (run_dir / "patches" / f"{safe}-{attempt}.patch").write_text(patch)
            # Reset worktree between attempts, preventing accumulation of bad edits.
            cmd(["git", "reset", "--hard", "HEAD"], cwd=worktree)
            cmd(["git", "clean", "-fd"], cwd=worktree)
            patchfile = run_dir / "patches" / f"{safe}-{attempt}.patch"
            apply = cmd(["git", "apply", "--check", str(patchfile)], cwd=worktree)
            if apply["exit"] == 0:
                apply = cmd(["git", "apply", str(patchfile)], cwd=worktree)
            record["apply"] = apply
            if apply["exit"] != 0:
                log["attempts"].append(record)
                continue
            changed = changed_paths(worktree)
            violations = [p for p in changed if not allowed(p, task["allowed_paths"])]
            record["violations"] = violations
            if violations:
                log["attempts"].append(record)
                continue
            checks = []
            for check in task.get("checks", []):
                if not isinstance(check, list) or not check:
                    raise ValueError("checks must be argv arrays")
                checks.append(
                    {
                        "argv": check,
                        "result": cmd(
                            check, cwd=worktree, timeout=config["check_timeout"]
                        ),
                    }
                )
            record["checks"] = checks
            record["tests_passed"] = bool(checks) and all(
                c["result"]["exit"] == 0 for c in checks
            )
            log["attempts"].append(record)
            if record["tests_passed"]:
                log["candidate_passed"] = True
                # Explicit review is required for accepted status; no self-certification.
                log["review"] = "pending"
                log["patch"] = str(patchfile)
                break
    finally:
        log["finished"] = time.time()
        cmd(
            ["git", "worktree", "remove", "--force", str(worktree)],
            cwd=config["repo"],
            timeout=30,
        )
    return log


def summarize(records, samples, elapsed):
    infer = [
        a["inference"]
        for r in records
        for a in r.get("attempts", [])
        if a.get("inference", {}).get("ok")
    ]
    g = [gpu for row in samples for gpu in row.get("gpu", [])]
    kv = [
        v
        for row in samples
        for k, v in row.get("vllm", {}).items()
        if ("kv_cache_usage_perc" in k or "gpu_cache_usage_perc" in k)
    ]
    latency = [x["latency_s"] for x in infer]

    def avg(a):
        return round(statistics.mean(a), 3) if a else None

    def p95(a):
        return round(sorted(a)[min(len(a) - 1, int(0.95 * len(a)))], 3) if a else None

    passed = sum(bool(r.get("candidate_passed")) for r in records)
    accepted = sum(bool(r.get("accepted")) for r in records)
    return {
        "tasks": len(records),
        "candidate_test_passes": passed,
        "candidate_test_pass_rate": round(passed / len(records), 4)
        if records
        else None,
        "accepted_tasks": accepted,
        "accepted_tasks_per_hour": round(accepted * 3600 / elapsed, 3)
        if elapsed
        else None,
        "candidate_passes_per_hour": round(passed * 3600 / elapsed, 3)
        if elapsed
        else None,
        "elapsed_s": round(elapsed, 3),
        "inference_calls": len(infer),
        "mean_request_latency_s": avg(latency),
        "p95_request_latency_s": p95(latency),
        "mean_gpu_utilization_pct": avg([x["utilization_pct"] for x in g]),
        "peak_gpu_utilization_pct": max(
            [x["utilization_pct"] for x in g], default=None
        ),
        "mean_gpu_memory_used_mib": avg([x["memory_used_mib"] for x in g]),
        "mean_kv_cache_utilization": avg(kv),
        "peak_kv_cache_utilization": max(kv, default=None),
        "prompt_tokens": sum(x.get("prompt_tokens") or 0 for x in infer),
        "completion_tokens": sum(x.get("completion_tokens") or 0 for x in infer),
        "note": "Accepted requires independent review; candidate passes are NOT accepted tasks.",
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", required=True)
    p.add_argument("--tasks", default=str(ROOT / "tasks" / "example.json"))
    p.add_argument("--endpoint", default="http://10.20.0.1:11535/v1")
    p.add_argument("--model", default="nvidia/Qwen3.8-27B-NVFP4")
    p.add_argument("--workers", type=int, default=3)
    p.add_argument("--revision", default="HEAD")
    p.add_argument("--attempts", type=int, default=3)
    p.add_argument("--max-output-tokens", type=int, default=4096)
    p.add_argument("--timeout", type=int, default=240)
    p.add_argument("--check-timeout", type=int, default=180)
    p.add_argument("--interval", type=float, default=2)
    p.add_argument("--out", default=str(ROOT / "results"))
    args = p.parse_args()
    if not 1 <= args.workers <= 16:
        p.error("workers must be 1..16")
    repo = pathlib.Path(args.repo).resolve()
    taskfile = pathlib.Path(args.tasks).resolve()
    tasks = json.loads(taskfile.read_text())
    if not isinstance(tasks, list) or not tasks:
        p.error("tasks must be a nonempty JSON array")
    for t in tasks:
        if not all(k in t for k in ("id", "prompt", "allowed_paths", "checks")):
            p.error("task missing required fields")
    run_dir = pathlib.Path(args.out).resolve() / datetime.datetime.now().strftime(
        "%Y%m%d-%H%M%S"
    )
    (run_dir / "worktrees").mkdir(parents=True)
    (run_dir / "patches").mkdir()
    cfg = vars(args) | {"repo": str(repo)}
    (run_dir / "configuration.json").write_text(json.dumps(cfg, indent=2))
    stop = threading.Event()
    mt = threading.Thread(
        target=monitor,
        args=(
            stop,
            args.endpoint.removesuffix("/v1") + "/metrics",
            args.interval,
            run_dir / "telemetry.jsonl",
        ),
        daemon=True,
    )
    start = time.monotonic()
    mt.start()
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
            futures = [pool.submit(worker, t, cfg, run_dir) for t in tasks]
            records = []
            for future in concurrent.futures.as_completed(futures):
                try:
                    records.append(future.result())
                except (
                    OSError,
                    ValueError,
                    KeyError,
                    IndexError,
                    TypeError,
                    AttributeError,
                    RuntimeError,
                ) as e:
                    records.append(
                        {"id": "worker_error", "error": repr(e), "accepted": False}
                    )
    finally:
        stop.set()
        mt.join(timeout=5)
    elapsed = time.monotonic() - start
    (run_dir / "tasks.json").write_text(json.dumps(records, indent=2))
    samples = [
        json.loads(line)
        for line in (run_dir / "telemetry.jsonl").read_text().splitlines()
        if line
    ]
    summary = summarize(records, samples, elapsed)
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps({"run_dir": str(run_dir), "summary": summary}, indent=2))


if __name__ == "__main__":
    main()
