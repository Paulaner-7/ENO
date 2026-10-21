#!/bin/sh
# Rebuilds resources/plugins/vendor.js from the pinned libraries (needs
# Node.js and npm; network access for the first install).
set -e
cd "$(dirname "$0")"
npm ci --no-audit --no-fund 2>/dev/null || npm install --no-audit --no-fund
npm run build
