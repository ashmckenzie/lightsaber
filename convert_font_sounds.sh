#!/bin/bash

OUTPUT_BASE="/Volumes/LS"

convert() {
  for style in clash swing
  do
    convert_files "${1}" "${style}"
  done
}

convert_files() {
  dir="${OUTPUT_BASE}/${1}"

  for original_file in `find "${dir}" -type f -name "${2}*.wav" | egrep -v 'new.wav'`
  do
    echo $original_file
    emporary_file="${original_file}.new.wav"
    ./sox_prep.sh "${original_file}" "${temporary_file}"
    mv "${temporary_file}" "${original_file}"
  done
}

#-----------------------------------------------------------------------

convert 'fonts/black_star'
convert 'fonts/classic'
convert 'fonts/old_republic_jedi'
convert 'fonts/star_killer'
convert 'fonts/unstable'
