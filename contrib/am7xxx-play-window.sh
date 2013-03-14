#!/bin/sh
#
# am7xxx-play-window - show only a given window with am7xxx-play
#
# Copyright (C) 2013  Antonio Ospite <ospite@studenti.unina.it>
#
# This program is free software. It comes without any warranty, to
# the extent permitted by applicable law. You can redistribute it
# and/or modify it under the terms of the Do What The Fuck You Want
# To Public License, Version 2, as published by Sam Hocevar. See
# http://sam.zoy.org/wtfpl/COPYING for more details.

set -e

DISPLAY=":0"
 
WIN_INFO="$(xwininfo)"
 
X=$(echo "$WIN_INFO" | sed -n -e "/^[[:space:]]*Absolute upper-left X:[[:space:]]*/s///p")
Y=$(echo "$WIN_INFO" | sed -n -e "/^[[:space:]]*Absolute upper-left Y:[[:space:]]*/s///p")
WIDTH=$(echo "$WIN_INFO" | sed -n -e "/^[[:space:]]*Width:[[:space:]]*/s///p")
HEIGHT=$(echo "$WIN_INFO" | sed -n -e "/^[[:space:]]*Height:[[:space:]]*/s///p")
 
set -x
am7xxx-play -f x11grab -i "${DISPLAY}+${X},${Y}" -o video_size="${WIDTH}x${HEIGHT}"
