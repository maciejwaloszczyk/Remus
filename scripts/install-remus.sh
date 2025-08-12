#!/usr/bin/env bash
set -euo pipefail

REPO="maciejwaloszczyk/Remus"
INSTALL_PATH="/usr/local/bin/remus"
TMPDIR="$(mktemp -d)"
CLEANUP() { rm -rf "$TMPDIR"; }
trap CLEANUP EXIT

log() { printf '[Remus][installer] %s\n' "$*"; }
err() { printf '[Remus][installer][error] %s\n' "$*" >&2; }

# Allow override: REMUS_VERSION=v0.1.0 ./install-remus.sh
if [[ "${REMUS_VERSION:-}" == "" ]]; then
  log "Detecting latest release tag via GitHub API..."
  # Use curl; fallback if rate limited
  API_JSON="$(curl -fsSL https://api.github.com/repos/$REPO/releases/latest || true)"
  if [[ -z "$API_JSON" ]]; then
    err "Failed to query GitHub API. Set REMUS_VERSION manually (e.g. REMUS_VERSION=v0.1.0)."; exit 1
  fi
  REMUS_VERSION="$(grep -m1 '"tag_name"' <<<"$API_JSON" | sed -E 's/.*"tag_name": *"([^"]+)".*/\1/')"
  if [[ -z "$REMUS_VERSION" ]]; then
    err "Could not parse latest release tag."; exit 1
  fi
fi
log "Target version: $REMUS_VERSION"

# Adjust asset name to match your release naming convention
# Example expected asset: remus-${REMUS_VERSION}-macos-universal.tar.gz
ASSET="remus-${REMUS_VERSION}-macos-universal.tar.gz"
URL="https://github.com/$REPO/releases/download/${REMUS_VERSION}/${ASSET}"

log "Downloading $URL"
if ! curl -fL "$URL" -o "$TMPDIR/$ASSET"; then
  err "Download failed for $URL"; exit 1
fi

log "Extracting archive"
if ! tar -xf "$TMPDIR/$ASSET" -C "$TMPDIR"; then
  err "Extraction failed"; exit 1
fi

# Expect a binary named 'remus' inside the archive
if [[ ! -f "$TMPDIR/remus" ]]; then
  # Try to find any file named remus* without extension
  CANDIDATE="$(find "$TMPDIR" -maxdepth 2 -type f -name 'remus' | head -n1 || true)"
  if [[ -n "$CANDIDATE" ]]; then
    mv "$CANDIDATE" "$TMPDIR/remus"
  fi
fi

if [[ ! -f "$TMPDIR/remus" ]]; then
  err "Binary 'remus' not found in archive. Check asset contents."; exit 1
fi

chmod 755 "$TMPDIR/remus"

if [[ -e "$INSTALL_PATH" ]]; then
  CURRENT_VER="$($INSTALL_PATH --version 2>/dev/null || true)"
  log "Existing installation detected ($INSTALL_PATH) : $CURRENT_VER"
fi

log "Installing to $INSTALL_PATH (will escalate if needed)"
INSTALL_CMD=(install -m 755 "$TMPDIR/remus" "$INSTALL_PATH")
if "${INSTALL_CMD[@]}" 2>/dev/null; then
  :
else
  if command -v sudo >/dev/null 2>&1; then
    log "Retrying with sudo..."
    if ! sudo "${INSTALL_CMD[@]}"; then
      err "sudo install failed"; exit 1
    fi
  else
    err "Permission denied and 'sudo' not available."; exit 1
  fi
fi

log "Verifying installed binary"
if ! "$INSTALL_PATH" --help >/dev/null 2>&1; then
  err "Installed binary does not execute properly."; exit 1
fi

NEW_VER="$($INSTALL_PATH --version 2>/dev/null || echo 'unknown')"
log "Installation successful. Version: $NEW_VER"

echo
log "Usage examples:"
echo "  remus --list"   
echo "  remus --device disk2 --filesystem FAT32 --name MY_USB" 

echo
log "Done."
