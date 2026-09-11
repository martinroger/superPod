# superPod Git Hooks

This directory contains tracked Git hooks to enforce quality checks and prevent regressions.

## Active Hooks

1. **`pre-commit`**:
   - Executes `./tests/run_tests.sh` before allowing a commit.
   - If any regression test fails, the commit is aborted.
   - Bypassing (emergency only): `git commit --no-verify`

2. **`pre-push`**:
   - Executes `./tests/run_tests.sh` before pushing commits to a remote repository.
   - If tests fail, the push is blocked.
   - Bypassing (emergency only): `git push --no-verify`

## Setup / Activation

Run the installation script once on any new clone:

```bash
./.githooks/install.sh
```

Or configure manually:

```bash
git config core.hooksPath .githooks
chmod +x .githooks/*
```

