#!/bin/sh

. ./common.sh

setup_test storage
init_config

STORAGE=$REAL_HOME/jai-storage-$$
HOST_HOME_FILE=$REAL_HOME/jai-storage-home-$$
UPPER_FILE=$STORAGE/default.changes/$(basename "$HOST_HOME_FILE")

register_cleanup_path "$STORAGE"
register_cleanup_path "$HOST_HOME_FILE"

real_user_mkdir_p "$STORAGE"
real_user_write_file "$STORAGE/.sentinel" "sentinel"
real_user_write_file "$HOST_HOME_FILE" "host"

capture_in_dir "$WORKDIR" run_jai --storage "$STORAGE" /bin/sh -c '
  printf overlay > "$1"
  if [ -e "$2/.sentinel" ]; then
    printf exposed
  else
    printf hidden
  fi
' sh "$HOST_HOME_FILE" "$STORAGE"

assert_status 0
assert_eq "$CAPTURE_STDOUT" "hidden"
assert_file_equals "$HOST_HOME_FILE" "host"
assert_path_exists "$UPPER_FILE"
assert_file_equals "$UPPER_FILE" "overlay"

mount_line=$(get_mount_line "/run/jai/$REAL_USER/default.home")
assert_contains "$mount_line" "upperdir=$STORAGE/default.changes"
