#!/bin/bash

SPEED="40"
VOICE="Samantha"
OUTPUT_BASE="/Volumes/8GB"

create() {
  tmp="./tmp/${3}.aiff"
  out="${OUTPUT_BASE}/${2}/${3}.wav"

  echo "> Generating '${out}'.."
  say -v ${VOICE} -r ${SPEED} "${1}" -o "${tmp}"
  ./sox_prep.sh "${tmp}" "${out}"

  afplay "${out}"
}

create_internal() {
  create "${1}" "internal" "${2}"
}

#-----------------------------------------------------------------------

for number in {1..10}
do
  create_internal "${number}" "${number}"
done

create_internal 'okay' okay
create_internal 'cancelled' cancelled
create_internal 'configuration menu' configuration_menu
create_internal 'configuration saved' configuration_saved
create_internal 'powering down' powering_down

create 'black star' 'fonts/black_star' name
create 'classic' 'fonts/classic' name
create 'old republic jedi' 'fonts/old_republic_jedi' name
create 'star killer' 'fonts/star_killer' name
create 'unstable' 'fonts/unstable' name
create 'dark meat' 'fonts/dark_meat' name
create 'light meat' 'fonts/light_meat' name
