#!/bin/bash
# Publish ParMmg packages to Feel++ APT repository using PyPI package
#
# Usage:
#   ./publish-parmmg-pypi.sh [stable|testing|pr] [noble|jammy|bookworm]
#
# Examples:
#   ./publish-parmmg-pypi.sh stable noble    # Publish to stable channel for Ubuntu Noble
#   ./publish-parmmg-pypi.sh testing jammy   # Publish to testing channel for Ubuntu Jammy
#
# Note: ParMmg depends on MMG. Ensure MMG packages are published first!

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGES_DIR="${SCRIPT_DIR}/packages"
VENV_DIR="${SCRIPT_DIR}/.venv-publishing"

CHANNEL="${1:-stable}"
DISTRO="${2:-noble}"

# Check if we have a uv venv, if not try to activate it or create one
if [[ -d "$VENV_DIR" ]]; then
    echo "Using virtual environment: $VENV_DIR"
    source "$VENV_DIR/bin/activate"
elif [[ -n "$VIRTUAL_ENV" ]]; then
    echo "Using active virtual environment: $VIRTUAL_ENV"
else
    # Check if feelpp-apt-publish is available globally
    if ! command -v feelpp-apt-publish &> /dev/null; then
        echo "❌ feelpp-apt-publish not found!"
        echo
        echo "Please set up the publishing environment first:"
        echo "  ./setup-publishing-env.sh"
        echo
        echo "Or activate an existing environment:"
        echo "  source .venv-publishing/bin/activate"
        echo
        echo "Or install globally with uv:"
        echo "  uv pip install feelpp-aptly-publisher"
        exit 1
    fi
fi

if [[ ! -d "$PACKAGES_DIR" ]]; then
    echo "Error: Packages directory not found: $PACKAGES_DIR"
    echo "Please build the packages first with: ./build-parmmg-deb.sh"
    exit 1
fi

# Check for .deb files (exclude debug symbols)
DEB_COUNT=$(find "$PACKAGES_DIR" -maxdepth 1 -name "*.deb" ! -name "*-dbgsym*.ddeb" | wc -l)
if [[ $DEB_COUNT -eq 0 ]]; then
    echo "Error: No .deb files found in $PACKAGES_DIR"
    exit 1
fi

echo "=========================================="
echo "Publishing ParMmg packages to Feel++ APT"
echo "=========================================="
echo "Component : base (core dependencies layer)"
echo "Channel   : $CHANNEL"
echo "Distro    : $DISTRO"
echo "Packages  : $DEB_COUNT .deb file(s)"
echo "Tool      : feelpp-apt-publish (PyPI)"
echo "=========================================="
echo
echo "⚠️  Note: ParMmg depends on MMG (libmmg5 >= 5.8.0)"
echo "    Ensure MMG is published to the same channel/distro first!"
echo

# Run the publish tool from PyPI
feelpp-apt-publish \
    --component base \
    --channel "$CHANNEL" \
    --distro "$DISTRO" \
    --debs "$PACKAGES_DIR" \
    --verbose

echo
echo "=========================================="
echo "✓ Publication complete!"
echo "=========================================="
echo
echo "Users can install with:"
echo "  curl -fsSL https://feelpp.github.io/apt/feelpp.gpg | sudo tee /usr/share/keyrings/feelpp.gpg >/dev/null"
echo "  echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/feelpp.gpg] https://feelpp.github.io/apt/$CHANNEL $DISTRO base' | sudo tee /etc/apt/sources.list.d/feelpp-base.list"
echo "  sudo apt update"
echo "  sudo apt install mmg libmmg5 libmmg-dev parmmg libparmmg5 libparmmg-dev libnapp-dev"
echo