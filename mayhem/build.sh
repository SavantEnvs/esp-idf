#!/usr/bin/env bash
#
# mayhem/build.sh — build the fuzz harnesses, their -standalone reproducers and the selftest oracle.
#
# ESP-IDF has no host build for the code fuzzed here (it is built for the chip with idf.py), so the
# sources are compiled directly with clang against stub headers in mayhem/stubs/. Everything is built
# from the in-tree sources only: no network, no submodules.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
# SanCov on the fuzzed code: $LIB_FUZZING_ENGINE only instruments what it is linked into, so the
# library and harness objects of the libFuzzer binaries need -fsanitize=fuzzer-no-link themselves.
# The -standalone reproducers link no fuzzer runtime, so they use a second, uninstrumented object set.
FUZZ_COV="-fsanitize=fuzzer-no-link"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS FUZZ_COV

SRC="${SRC:-/mayhem}"
cd "$SRC"
OUT="$SRC"
B="$SRC/build/mayhem"
H="$SRC/mayhem/harnesses"
C="$SRC/components"
W="$C/wpa_supplicant"
N="$C/nvs_flash"
mkdir -p "$B/san" "$B/sast" "$B/norm"

STUBS="-I$SRC/mayhem/stubs"
HTTP_INC="-I$C/http_parser"
WPA_INC="-I$W/esp_supplicant/src -I$W/esp_supplicant/include -I$C/esp_event/include -I$C/esp_wifi/include/local
 -I$C/esp_wifi/wifi_apps/include $STUBS -I$W/port/include -I$W/src -I$W/src/utils -I$W/include
 -I$C/esp_common/include -I$C/log/include -I$C/esp_rom/include -I$C/esp_hw_support/include
 -I$C/soc/linux/include -I$C/soc/include -I$C/esp_system/include -I$C/esp_timer/include -I$C/hal/include
 -idirafter $C/esp_wifi/include"
WPA_DEFS="-DESP_SUPPLICANT -DESP_PLATFORM -DCONFIG_RRM -DCONFIG_WNM -DCONFIG_WMM"
NVS_INC="$STUBS -I$N/include -I$N/src -I$N/private_include -I$C/esp_common/include -I$C/esp_rom/include
 -I$C/esp_partition/include -I$C/esp_system/include -I$C/spi_flash/include -I$C/log/include
 -I$C/esp_hw_support/include -I$C/soc/include -I$H"
NVS_DEFS="-DLINUX_TARGET -DESP_PLATFORM"
CXXSTD="-std=gnu++17 -fno-exceptions"
WARN="-w"

HTTP_SRCS="$C/http_parser/http_parser.c"
WPA_SRCS="$W/src/utils/json.c $W/src/utils/base64.c $W/src/utils/wpabuf.c $W/src/utils/common.c
 $W/src/common/ieee802_11_common.c $W/src/wps/wps_attr_parse.c $H/wpa_shim.c"
NVS_SRCS="$N/src/nvs_item_hash_list.cpp $N/src/nvs_page.cpp $N/src/nvs_pagemanager.cpp
 $N/src/nvs_storage.cpp $N/src/nvs_types.cpp $N/src/nvs_platform.cpp $H/nvs_shim.c"

# obj <san|sast|norm> <compiler> <flags...> -- <src> ; prints the object path
obj() {
  local v="$1" cc="$2"; shift 2
  local flags=() src
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done
  src="$2"
  local o="$B/$v/$(echo "${src#$SRC/}" | tr '/' '_').o"
  if [ "$v" = san ]; then
    $cc $SANITIZER_FLAGS $FUZZ_COV $DEBUG_FLAGS -O1 $WARN "${flags[@]}" -c "$src" -o "$o"
  elif [ "$v" = sast ]; then
    $cc $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $WARN "${flags[@]}" -c "$src" -o "$o"
  else
    $cc -O2 $WARN $COVERAGE_FLAGS "${flags[@]}" -c "$src" -o "$o"
  fi
  echo "$o"
}

objs_of() { # <variant> <group>
  local v="$1" g="$2" s list=()
  case "$g" in
    http) for s in $HTTP_SRCS; do list+=("$(obj "$v" "$CC" $HTTP_INC -- "$s")"); done ;;
    wpa)  for s in $WPA_SRCS;  do list+=("$(obj "$v" "$CC" $WPA_DEFS $WPA_INC -- "$s")"); done ;;
    nvs)  for s in $NVS_SRCS; do
            case "$s" in
              *.cpp) list+=("$(obj "$v" "$CXX" $CXXSTD $NVS_DEFS $NVS_INC -- "$s")") ;;
              *.c)   list+=("$(obj "$v" "$CC" $NVS_DEFS $NVS_INC -- "$s")") ;;
            esac
          done ;;
  esac
  printf '%s\n' "${list[@]}"
}

# 1) lsan_off hook + standalone driver objects (sanitized)
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/san/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/san/standalone_main.o"

# 2) project sources, sanitized: SanCov-instrumented (san) for libFuzzer, plain (sast) for -standalone
mapfile -t HTTP_O  < <(objs_of san http)
mapfile -t WPA_O   < <(objs_of san wpa)
mapfile -t NVS_O   < <(objs_of san nvs)
mapfile -t HTTP_SO < <(objs_of sast http)
mapfile -t WPA_SO  < <(objs_of sast wpa)
mapfile -t NVS_SO  < <(objs_of sast nvs)

# 3) harnesses: fuzzer binary + standalone reproducer
link_target() { # <name> <lang c|cxx> <harness src> <group http|wpa|nvs> <flags...>
  local name="$1" lang="$2" hsrc="$3" grp="$4"; shift 4
  local flags=("$@") ld cc cxxstd="" ho="$B/san/$name.o" hso="$B/sast/$name.o"
  local -n fobjs="${grp^^}_O" sobjs="${grp^^}_SO"
  if [ "$lang" = c ]; then cc="$CC"; else cc="$CXX"; cxxstd="$CXXSTD"; fi
  ld="$cc"
  $cc $SANITIZER_FLAGS $FUZZ_COV $DEBUG_FLAGS -O1 $WARN $cxxstd "${flags[@]}" -c "$hsrc" -o "$ho"
  $cc $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $WARN $cxxstd "${flags[@]}" -c "$hsrc" -o "$hso"
  $ld $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$ho" "${fobjs[@]}" "$B/san/lsan_off.o" -o "$OUT/$name"
  $ld $SANITIZER_FLAGS $DEBUG_FLAGS "$hso" "${sobjs[@]}" "$B/san/standalone_main.o" "$B/san/lsan_off.o" -o "$OUT/$name-standalone"
}

link_target fuzz_http_parser c   "$H/fuzz_http_parser.c" http $HTTP_INC
link_target fuzz_wpa_json    c   "$H/fuzz_wpa_json.c"    wpa  $WPA_DEFS $WPA_INC
link_target fuzz_wpa_ie      c   "$H/fuzz_wpa_ie.c"      wpa  $WPA_DEFS $WPA_INC
link_target fuzz_nvs         cxx "$H/fuzz_nvs.cpp"       nvs  $NVS_DEFS $NVS_INC

# 4) the selftest oracle, with normal flags (no sanitizers, no debug flags)
mapfile -t HTTP_N < <(objs_of norm http)
mapfile -t WPA_N  < <(objs_of norm wpa)
mapfile -t NVS_N  < <(objs_of norm nvs)
$CC -O2 $WARN $COVERAGE_FLAGS $WPA_DEFS $WPA_INC $HTTP_INC -c "$SRC/mayhem/selftest.c" -o "$B/norm/selftest.o"
$CXX -O2 $WARN $COVERAGE_FLAGS $CXXSTD $NVS_DEFS $NVS_INC -c "$SRC/mayhem/selftest_nvs.cpp" -o "$B/norm/selftest_nvs.o"
$CXX $COVERAGE_FLAGS "$B/norm/selftest.o" "$B/norm/selftest_nvs.o" "${HTTP_N[@]}" "${WPA_N[@]}" "${NVS_N[@]}" \
  -o "$OUT/esp_selftest"
