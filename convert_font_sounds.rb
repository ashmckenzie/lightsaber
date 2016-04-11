#!/usr/bin/env ruby

require 'pry-byebug'
require 'pathname'
require 'find'

DEBUG                     = true
OUTPUT_BASE               = "/Volumes/8GB"
ORIGINAL_FONTS_BASE       = "/Users/ash/Dropbox/saber/Sounds/all"
VALID_SOUND_FONTS_MAPPING = {
  [ /lockup/, /lock_up/ ]               => 'lockup',
  [ /idle/, /hum/ ]                     => 'idle',
  [ /poweron/, /power_on/, /pwron/ ]             => 'poweron',
  [ /poweroff/, /power_off/, /pwroff/ ]           => 'poweroff',
  [ /swing(\d{,1})/, /motion(\d{,1})/ ] => 'swing\1',
  [ /clash(\d{,1})/, /impact(\d{,1})/ ] => 'clash\1'
}

def convert(source_dir, destination_dir)
  source_dir = "#{ORIGINAL_FONTS_BASE}/#{source_dir}"
  destination_dir = "#{OUTPUT_BASE}/#{destination_dir}"

  `mkdir -p "#{destination_dir}"` unless DEBUG

  sounds = Find.find(source_dir).select { |x| x.match(/.wav$/) }.select do |x|
    new_filename = nil
    file = Pathname.new(x)

    dirname = file.dirname.to_s
    filename = file.sub_ext('').basename.to_s

    VALID_SOUND_FONTS_MAPPING.each do |candidates, final|
      if match = Regexp.union(candidates).match(filename)
        new_filename = filename.sub(match.regexp, final)
        break
      end
    end

    next if !new_filename

    new_file = file.sub(dirname, destination_dir).sub(filename, new_filename)

    command = "./sox_prep.sh #{file.to_s} #{new_file.to_s}"
    puts command
    `#{command}` unless DEBUG
  end
end

#-----------------------------------------------------------------------

convert('Novastarpackage/Black Star', 'fonts/black_star')
convert('Classic', 'fonts/classic')
convert('OldRepublicJedi', 'fonts/old_republic_jedi')
convert('StarKiller', 'fonts/star_killer')
convert('Unstable-Tsuarbisu', 'fonts/unstable')
convert('Light_Meat', 'fonts/light_meat')
convert('Dark_Meat', 'fonts/dark_meat')
convert('Rogue', 'fonts/rogue')
