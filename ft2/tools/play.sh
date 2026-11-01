#!/bin/bash
# play.sh TABLE prefix : start game, plunge, flip; trimmed screenshots along the way
T=$1; P=$2; K=/home/claude/tools/xkey/xkey
cd /home/claude/run; export DISPLAY=:9 SDL_AUDIODRIVER=dummy
cp -f /home/claude/ft2-decomp/bin/SpaceCadetPinball ./scp_$T
(FT2_DAT=$T.DAT timeout -k 2 30 ./scp_$T > $T.log 2>&1 &)
sleep 5
shot() { import -window root /tmp/a.png; convert /tmp/a.png -trim +repage $1; }
$K warp 1280 800 sleep 300 F2 tap 100 sleep 1500; shot ${P}_1start.png
$K space press sleep 1500; shot ${P}_2plunger.png
$K space release sleep 600; shot ${P}_3launch.png
sleep 1.2; shot ${P}_4.png
for i in 1 2 3 4 5; do $K z tap 150 slash tap 150 sleep 500; done; shot ${P}_5.png
sleep 3; shot ${P}_6.png
for p in $(pgrep scp_$T); do kill -9 $p; done
montage ${P}_2plunger.png ${P}_3launch.png ${P}_4.png ${P}_6.png -tile 2x2 -geometry 640x480+2+2 ${P}_m.png
