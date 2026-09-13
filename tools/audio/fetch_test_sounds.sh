#!/bin/sh
# Populate a local, gitignored directory of test sounds for the audio seam.
#
# This port ships no audio and never will: the originals are MIDP-1.0 and
# silent (see `added-audio` in docs/DIVERGENCES.md). These are somebody else's
# copyrighted assets, borrowed from an installed copy of Morrowind purely to
# hear whether the hooks fire at the right moments. They are not redistributable
# and the output directory is gitignored for that reason.
#
# Usage:
#   tools/audio/fetch_test_sounds.sh ["<Morrowind>/Data Files/Sound"] [out-dir]
#
# Defaults to the usual Steam location, and to the sounds/ folder inside the
# Stormhold data tree -- which is where the front-ends look with no --sounds,
# so fetching is all it takes to make a build audible.
set -e

SRC="${1:-S:/SteamLibrary/steamapps/common/Morrowind/Data Files/Sound}"
OUT="${2:-artifacts/decompiled/v1010/sounds}"

if [ ! -d "$SRC/Fx" ]; then
    echo "no Morrowind sounds at: $SRC" >&2
    echo "pass the path as the first argument." >&2
    exit 1
fi

mkdir -p "$OUT"

# Sound enum value  <-  the closest thing Morrowind has.
# Kept as one table so it is obvious what stands in for what, and so a name
# that moves in the enum is a one-line change here.
copy() {
    if [ -f "$SRC/$2" ]; then
        cp "$SRC/$2" "$OUT/$1.wav"
        echo "  $1.wav  <- $2"
    else
        echo "  MISSING: $2 (for $1)" >&2
    fi
}

copy MenuMove     "Fx/inter/menu2.wav"
copy MenuSelect   "Fx/inter/menu1.wav"
copy MenuBack     "Fx/menu click.wav"
copy PlayerStep   "Fx/FOOT/dirtFOOT/walkl.wav"
copy Blocked      "Fx/LEFTWOD1.wav"      # BMstone is an 8.8s ambience, not a bump
copy MeleeHit     "Fx/Medium Armor Hit.wav"
copy MeleeMiss    "Fx/miss.wav"
copy PlayerHurt   "Fx/body hit.wav"
copy MonsterDied  "Fx/bodyfallMED.wav"
copy SpellCast    "Fx/magic/destC.wav"
copy SpellFizzle  "Fx/magic/destFAIL.wav"
copy ChestOpened  "Fx/chest_opn.wav"
copy ChestLocked  "Fx/trans/contnr_lokd.wav"
copy ItemPickedUp "Fx/item/item.wav"
copy LevelUp      "Fx/inter/levelUP.wav"
copy PlayerDied   "Fx/criticalDMG.wav"

echo "sounds in $OUT (gitignored; not redistributable)"
echo "the front-ends read <data>/sounds with no flag, so this is enough."
