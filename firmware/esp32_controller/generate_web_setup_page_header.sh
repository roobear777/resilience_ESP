#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_file="$script_dir/web_setup_page.html"
output_file="$script_dir/web_setup_page_gzip.h"
temporary_file=$(mktemp "$script_dir/web_setup_page_gzip.h.XXXXXX")
trap 'rm -f "$temporary_file"' EXIT HUP INT TERM

{
  printf '%s\n' '#ifndef TARDI_WEB_SETUP_PAGE_GZIP_H'
  printf '%s\n' '#define TARDI_WEB_SETUP_PAGE_GZIP_H'
  printf '\n%s\n\n' '#include <Arduino.h>'
  printf '%s\n' '// Generated deterministically from web_setup_page.html with: gzip -9 -n'
  printf '%s\n' 'static const uint8_t WEB_SETUP_PAGE_GZIP[] PROGMEM = {'
  gzip -9 -n -c "$source_file" | xxd -i
  printf '%s\n\n' '};'
  printf '%s\n\n' 'static const size_t WEB_SETUP_PAGE_GZIP_SIZE = sizeof(WEB_SETUP_PAGE_GZIP);'
  printf '%s\n' '#endif'
} > "$temporary_file"

chmod 644 "$temporary_file"
mv "$temporary_file" "$output_file"
trap - EXIT HUP INT TERM
