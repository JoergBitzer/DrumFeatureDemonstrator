#!/usr/bin/env bash
# Downloads the MusicRadar "Junk Drums" sample pack (free, royalty-free, from SampleRadar:
# https://www.musicradar.com/news/tech/free-music-samples-royalty-free-loops-hits-and-multis-to-download-sampleradar)
# for tests and for the factory preset. The samples are not stored in the repository.
#
# Usage:  tools/fetch_samples.sh [target-folder]
#   target folder: default $DFD_SAMPLE_DIR, or ~/.cache/DrumFeatureDemonstrator/samples
# Prints the folder "Junk Drums" of the pack on the last line (so CI can use it: SAMPLES=$(tools/fetch_samples.sh | tail -1)).
# If the pack is already there (e.g. in the author's collection ~/Music/samples/musicradar_sub), nothing is downloaded.
set -euo pipefail

PACK_NAME="musicradar-junk-drums-samples"
PACK_URL="https://cdn.mos.musicradar.com/audio/samples/${PACK_NAME}.zip"
AUTHOR_COLLECTION="$HOME/Music/samples/musicradar_sub/Junk Drums"

TARGET="${1:-${DFD_SAMPLE_DIR:-$HOME/.cache/DrumFeatureDemonstrator/samples}}"

if [ -d "$AUTHOR_COLLECTION" ] && [ -z "${1:-}" ] && [ -z "${DFD_SAMPLE_DIR:-}" ]; then
    echo "$AUTHOR_COLLECTION"
    exit 0
fi

PACK_DIR="$TARGET/$PACK_NAME"
DRUMS_DIR="$PACK_DIR/Junk Drums"
if [ -d "$DRUMS_DIR" ]; then
    echo "$DRUMS_DIR"
    exit 0
fi

mkdir -p "$PACK_DIR"
ZIP_FILE="$(mktemp)"
trap 'rm -f "$ZIP_FILE"' EXIT
echo "Downloading $PACK_URL" >&2
curl --fail --location --silent --show-error --output "$ZIP_FILE" "$PACK_URL"
unzip -q -o "$ZIP_FILE" -d "$PACK_DIR" -x "__MACOSX/*"
echo "$DRUMS_DIR"
