#!/usr/bin/env bash
#
# Structural checks on a linked firmware image. These are the properties that
# decide whether the chip boots at all, and every one of them can be verified
# without hardware -- so CI runs this on every cross build.
#
# Usage: tools/verify-firmware.sh <build-dir>

set -euo pipefail

BUILD_DIR="${1:?usage: verify-firmware.sh <build-dir>}"
OBJPREFIX="${OBJPREFIX:-arm-none-eabi-}"
VECTOR_ADDRESS="08000000"

readonly NM="${OBJPREFIX}nm"
readonly READELF="${OBJPREFIX}readelf"

failures=0

fail() {
  printf '  FAIL: %s\n' "$1"
  failures=$((failures + 1))
}

check_image() {
  local elf="$1"
  printf '%s\n' "${elf#"${BUILD_DIR}"/}"

  # The boot ROM loads the initial stack pointer and reset vector from the
  # first two words of flash. A vector table nothing references is exactly
  # what --gc-sections removes, so this catches a dropped KEEP.
  local vector
  vector=$("${READELF}" -S "${elf}" | awk '/\.isr_vector/ {print $5}')
  if [[ "${vector}" != "${VECTOR_ADDRESS}" ]]; then
    fail ".isr_vector at 0x${vector:-<missing>}, expected 0x${VECTOR_ADDRESS}"
  fi

  # The ELF entry point must be Reset_Handler with the Thumb bit set: on
  # Cortex-M, bit 0 of a branch target selects Thumb state, and an even entry
  # address faults on the first instruction.
  local entry reset
  entry=$("${READELF}" -h "${elf}" | awk '/Entry point address:/ {print $NF}')
  reset=$("${NM}" "${elf}" | awk '$3 == "Reset_Handler" {print "0x" $1}')
  if [[ -z "${reset}" ]]; then
    fail "no Reset_Handler symbol"
  elif (( entry != (reset | 1) )); then
    fail "entry ${entry} is not Reset_Handler (${reset}) with the Thumb bit"
  fi

  # Static constructors run only if __libc_init_array has entries to walk.
  local init_size
  init_size=$("${READELF}" -S "${elf}" | awk '/\.init_array/ {print $6}')
  if [[ -z "${init_size}" || "${init_size}" == "000000" ]]; then
    fail ".init_array is empty: namespace-scope constructors will not run"
  fi

  # A firmware image has no loader to resolve anything later.
  local undefined
  undefined=$("${NM}" -u "${elf}")
  if [[ -n "${undefined}" ]]; then
    fail "undefined symbols: $(tr -s '[:space:]' ' ' <<<"${undefined}")"
  fi
}

# The interrupt-callback paths must not allocate. mcu::InputPin and mcu::Uart
# take std::function handlers, which is allocation-free only while the captured
# state fits libstdc++'s 16-byte small-buffer optimization. That is a real
# property of the code today, and this is what keeps it true: an operator new
# reference in these objects means a handler outgrew the buffer.
check_no_allocation() {
  # operator new only. operator delete is deliberately not in this pattern:
  # a polymorphic class's vtable references its deleting destructor, which
  # references operator delete, whether or not anything is ever allocated --
  # so every one of these objects has an undefined _ZdlPvj and always will.
  # An operator new reference is the thing that means a handler allocated.
  local pattern='_Znwj|_Znaj|_Znw|_Zna'
  local objects
  mapfile -t objects < <(find "${BUILD_DIR}" -name 'gpio_pin.cpp.obj' \
                                          -o -name 'exti.cpp.obj' \
                                          -o -name 'usart.cpp.obj')
  (( ${#objects[@]} == 0 )) && return 0

  printf 'interrupt-path allocation check\n'
  for object in "${objects[@]}"; do
    if "${NM}" -u "${object}" | grep -qE "${pattern}"; then
      fail "${object##*/} references operator new/delete; a std::function
        handler has outgrown the small-buffer optimization. See the note in
        src/libs/mcu/pin.hpp."
    fi
  done
}

mapfile -t images < <(find "${BUILD_DIR}" -name '*.elf' | sort)
if (( ${#images[@]} == 0 )); then
  echo "No .elf images under ${BUILD_DIR}" >&2
  exit 1
fi

for image in "${images[@]}"; do
  check_image "${image}"
done
check_no_allocation

if (( failures > 0 )); then
  printf '\n%d check(s) failed.\n' "${failures}" >&2
  exit 1
fi
printf '\nAll %d image(s) verified.\n' "${#images[@]}"
