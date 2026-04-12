# Security Policy

## Scope

This repository contains open-source firmware and hardware documentation for a DIY avionics controller. There are no networked services, servers, or user accounts. Security concerns for this project are primarily about:

1. **Keeping secrets out of version history** (API keys, tokens, credentials that contributors might accidentally commit)
2. **Firmware safety** (bugs that could cause incorrect propeller governor behavior)

Firmware safety issues are treated with the same urgency as security vulnerabilities.

---

## Reporting a Vulnerability

**Do not open a public GitHub issue for security-sensitive findings.**

Instead, use one of the following:

- **GitHub Private Security Advisory:** Go to the repository → Security tab → "Report a vulnerability." This is the preferred channel.
- **Email:** Open an issue asking for a secure contact address, and a maintainer will respond within 48 hours with a private channel.

### What to include

- A clear description of the finding
- File(s) and line number(s) affected
- Steps to reproduce (if applicable)
- Your assessment of severity and potential impact
- Any suggested fix, if you have one

### Response expectations

| Severity | Initial response | Target fix |
|----------|-----------------|------------|
| Critical (firmware safety / propeller behavior) | 24 hours | 48–72 hours |
| High (secret exposure, code execution) | 48 hours | 1 week |
| Medium / Low | 1 week | Best effort |

---

## Secret Handling Rules

These rules apply to **all contributors**:

1. **Never commit secrets.** This includes API keys, tokens, passwords, private keys, AWS credentials, and GitHub tokens of any kind.
2. **`.env` files are blocked by `.gitignore`.** Do not force-add them.
3. **The pre-commit hook (`scan_secrets.py`) scans staged files before every commit.** Install it after cloning:
   ```
   pip install pre-commit
   pre-commit install
   ```
4. **Do not embed credentials in firmware source** — not even development/test credentials. Use `#define` placeholders with placeholder values and document the required substitution in comments.
5. **Ignore/allowlist decisions require explicit justification.** If `scan_secrets.py` flags a false positive, document why in a comment adjacent to the flagged string before using `git commit --no-verify` (which bypasses the hook). The CI Gitleaks scan runs independently and cannot be bypassed locally.

### False Positives

If the local hook flags something that is not actually a secret (e.g., a test vector, a known-safe example string):

1. Add an inline comment explaining why it is not a secret.
2. If the pattern cannot be avoided, add a `.gitleaks.toml` allow-rule specifically scoped to that file/line and document the justification in the commit message.

---

## What to Do if a Secret Is Committed

Act immediately — even if the commit has not been pushed:

### If the secret has NOT been pushed
```
git reset HEAD~1          # undo the commit locally
# Remove the secret from the file
git add .
git commit
```

### If the secret HAS been pushed (treat as fully compromised)

1. **Revoke/rotate the secret immediately** — before doing anything else.
   - AWS: IAM Console → Delete the access key, create a new one.
   - GitHub token: Settings → Developer settings → Personal access tokens → Delete.
   - Any other credential: follow the provider's revocation procedure.

2. **Purge the secret from git history:**
   ```
   # Using git-filter-repo (preferred):
   pip install git-filter-repo
   git filter-repo --path <file-with-secret> --invert-paths
   # OR to scrub a string across all history:
   git filter-repo --replace-text <(echo 'SECRET_VALUE==>REDACTED')
   ```
   Then force-push all branches:
   ```
   git push origin --force --all
   git push origin --force --tags
   ```

3. **Ask GitHub Support to clear the advisory database cache** if the repo is public (cached views may still show the secret for a short time after force-push).

4. **Audit usage** — check provider logs to determine whether the secret was used by any unauthorized party during the exposure window.

5. Open a private security advisory (see Reporting above) to document the incident.

> **Note:** Force-pushing rewrites history and will break clones held by other contributors. Communicate promptly with all contributors to re-clone or reset.

---

## Recommended GitHub Repository Settings

Enable the following in **Settings → Security** for this repository:

| Setting | Location | Recommended |
|---|---|---|
| Secret scanning | Settings → Security → Secret scanning | Enabled |
| Push protection | Settings → Security → Secret scanning → Push protection | Enabled |
| Dependabot alerts | Settings → Security → Dependabot | Enabled |
| Dependabot security updates | Settings → Security → Dependabot | Enabled |
| Code scanning (CodeQL) | Configured via `.github/workflows/codeql.yml` | Active |

Push protection will block pushes containing known secret patterns **at the server side**, as a second line of defense after the local pre-commit hook.

---

## CI Security Scanning Summary

| Tool | Config file | What it checks |
|---|---|---|
| **CodeQL** | `.github/workflows/codeql.yml` | SAST for C/C++ firmware and Python tooling (push, PR, weekly) |
| **Gitleaks** | `.github/workflows/gitleaks.yml` | Secret patterns across full git history (push, PR, weekly) |
| **scan_secrets.py** | `.pre-commit-config.yaml` | High-signal patterns in staged files before every local commit |

---

## Firmware Safety Notes

This firmware controls an aircraft propeller governor. Although this is an open-source DIY project:

- Any change affecting motor control logic, relay timing, watchdog behavior, or RPM sensing should include a rationale.
- Changes to `DEADBAND_RPM`, `WATCHDOG_TIMEOUT_MS`, `RELAY_TIMEOUT_SEC`, or similar safety parameters require justification in the commit message.
- The bench test procedure in [docs/BENCH_TEST.md](docs/BENCH_TEST.md) should be run after any firmware change before aircraft installation.
