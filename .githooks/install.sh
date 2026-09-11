#!/usr/bin/env bash
# Installer script for superPod git hooks
set -e

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT"

echo "Configuring git hooks for superPod repository..."

# Make hooks executable
chmod +x .githooks/pre-commit
chmod +x .githooks/pre-push

# Configure git to use the tracked .githooks directory
git config core.hooksPath .githooks

# Also link into .git/hooks for fallback compatibility
if [ -d ".git/hooks" ]; then
    ln -sf ../../.githooks/pre-commit .git/hooks/pre-commit
    ln -sf ../../.githooks/pre-push .git/hooks/pre-push
fi

echo "✔ Git hooks successfully configured (.githooks/pre-commit, .githooks/pre-push)."
echo "  Regression tests will now automatically run before commits and pushes."

