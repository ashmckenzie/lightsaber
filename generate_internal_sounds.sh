#!/bin/bash

VOICE="Samantha"
SPEED="40"
OUTPUT_BASE="/Volumes/LS"

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

create 'black star' 'fonts/black_star'  name
create 'classic' 'fonts/classic'  name
create 'old republic jedi' 'fonts/old_republic_jedi'  name
create 'star killer' 'fonts/star_killer'  name
