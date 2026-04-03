#!/bin/bash
# ⚡ Curium Git Helper Script

# Fix ownership issues if needed
git config --global --add safe.directory $(pwd)

# Add all files
echo "Staging changes..."
git add .

# Prompt for commit message or use default
if [ -z "$1" ]; then
    MSG="Update: Complete Curium v4.0 Documentation and Tests"
else
    MSG="$1"
fi

# Commit
echo "Committing: $MSG"
git commit -m "$MSG"

echo "✅ Git sync complete!"
