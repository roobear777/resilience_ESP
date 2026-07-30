#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
controller_dir="$script_dir/../esp32_controller"
shared_dir="$script_dir/src/shared"

for file in \
  led_animations.cpp led_animations.h \
  led_color.h led_color_convert.cpp led_color_convert.h \
  led_config.h led_engine.cpp led_engine.h led_layout.h \
  led_legs.cpp led_legs.h led_settings.cpp led_settings.h \
  led_state.cpp led_state.h \
  led_z1_mouth.cpp led_z1_mouth.h \
  led_z2_shoulder.cpp led_z2_shoulder.h \
  led_z3_midbody.cpp led_z3_midbody.h \
  led_z4_rear.cpp led_z4_rear.h \
  led_z7_digestive.cpp led_z7_digestive.h
do
  cp "$controller_dir/$file" "$shared_dir/$file"
done

cp "$controller_dir/eclair_link_protocol.h" "$shared_dir/eclair_link_protocol.h"
