# Autonomous multi-agent implementation workflow

A supervisor loop that implements the remaining SoundShelf architecture using six
role-agents, each running on the model best suited to its job. The supervisor runs
unattended until the architect reports that everything is implemented (or it hits a
hard block), checkpointing after every step so it survives token-limit pauses and
restarts.

## Pipeline

```
        ┌──────────────────────────────────────────────────────────────┐
        ▼                                                              │
   ARCHITECT ─► PROGRAMMER ─► REVIEWER ─► CRITIC ─► TESTER ─► COMMITTER ┘
   (opus)       (sonnet)      (sonnet)    (opus)    (haiku)   (haiku)
        │            ▲   ▲         │          │        │
        │            │   └─ needs_changes ────┘        │
        │            └────── score < 7 / test fail ────┘
        └─ all_implemented ─► STOP (you review)
```

Each role is a separate headless `claude -p` call with its own role system prompt
(`scripts/workflow_roles/<role>.md`) and model. Every role must end its reply with
a machine-readable line `===VERDICT=== {json}` that the supervisor parses to decide
the next stage.

| Role | Model | Responsibility |
|------|-------|----------------|
| architect | Opus 4.8 | Reads docs + git; picks the ONE next feature and writes its spec + Definition of Done. Stops the loop when nothing is left. |
| programmer | Sonnet 4.6 | Implements exactly the spec: code + Doxygen + unit tests, cross-platform. |
| reviewer | Sonnet 4.6 | Checks the diff matches the spec and is Linux+Windows-clean and convention-compliant. |
| critic | Opus 4.8 | Scores the implementation 0–9 against C/C++ best practices; **< 7 sends it back** to programmer or architect. |
| tester | Haiku 4.5 | Runs the **real** builds/tests: `ctest` on Linux + the vcpkg/MSVC build on Windows over SSH. |
| committer | Haiku 4.5 | Commits and `git push` once everything passed; keeps the CLAUDE.md status table in sync. |

### Definition of Done (enforced every feature)
Doxygen on new public API · unit tests in `tests/` · tests build **and pass**
(`ctest`) · clean build on **Linux and Windows** · CLAUDE.md conventions.

## Running

```bash
# Full autonomous loop (resumes from the last checkpoint automatically):
python3 scripts/agent_workflow.py

# Run a single stage and stop (good for watching one step):
python3 scripts/agent_workflow.py --once

# Try it without committing/pushing (skips the committer):
python3 scripts/agent_workflow.py --dry-run --max-iterations 1

# Bound the run:
python3 scripts/agent_workflow.py --max-iterations 5
```

Run it from the repo root. Each agent runs with `--permission-mode bypassPermissions`
so it can edit, build, and (committer) push **without prompts** — this is the
approved full-autonomy mode. Keep an eye on it.

Tip: run it in the background and watch progress live:
```bash
nohup python3 scripts/agent_workflow.py > .workflow/run.out 2>&1 &
tail -f .workflow/status        # current agent + what it's doing
tail -f .workflow/progress.log  # full timeline
```

## Watching progress

- `.workflow/status` — one line: which agent is active, model, attempt, and the
  current task (overwritten each step).
- `.workflow/progress.log` — append-only timeline with timestamps and verdicts.
- `.workflow/transcripts/<iter>-<role>-<attempt>.txt` — full raw output of each
  agent call (for debugging a bad verdict).
- `python3 scripts/agent_workflow.py --status` — print the current state.

## Token / usage-limit handling

If any agent call hits a usage/token/rate limit, the supervisor:
1. writes the checkpoint (`.workflow/state.json`) keeping the **same stage**,
2. sets status `waiting_tokens`, parses the reset time if the CLI reports one,
3. sleeps until the reset (or backs off 5→10→…→30 min when the time is unknown),
4. re-runs the exact stage it paused on and continues.

So the loop self-heals across limit windows. Because state is on disk, you can also
Ctrl-C and re-launch `python3 scripts/agent_workflow.py` later — it resumes from the
checkpoint. (To survive machine reboots too, you could wrap the launch in a cron
`@reboot` job; the in-process waiter already covers same-session limit windows.)

## Control flow details

- reviewer `needs_changes` → back to programmer with the issue list.
- critic score `< 7` → back to programmer (or architect if the *design* is wrong),
  carrying the feedback.
- tester `fail` → back to programmer with the failing log tail.
- After `MAX_ATTEMPTS` (6) loop-backs on one task, it escalates to the architect
  once, then marks the task **blocked** and stops for a human.
- committer success → next iteration (architect picks the next feature).

## Stopping & resetting

- Terminal states: `all_done` (success), `blocked` (needs you), `error`.
- `python3 scripts/agent_workflow.py --reset` clears the checkpoint to start over.

## Tuning

Edit the constants at the top of `scripts/agent_workflow.py`:
`MODEL` (per-role models), `TIMEOUT`, `CRITIC_PASS`, `MAX_ATTEMPTS`, token backoff.
Role behaviour lives in `scripts/workflow_roles/*.md` — edit those to change what
each agent does or how strict it is.
