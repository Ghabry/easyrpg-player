#!/usr/bin/env ruby -Ku

require 'rubygems'
require 'json'

# usage> to_cxx.rb INPUT_DIRECTORY OUTPUT
raise "argument error" unless ARGV.length == 2

out = File.open(ARGV[1], "w")

out.write "#include <cstdlib>\n"
out.write "extern char const* LCF_SCHEMA_JSON_STRING[];\n"
out.write "char const* LCF_SCHEMA_JSON_STRING[] = {\n"

Dir.glob("#{ARGV[0]}/*.json") { |f|
  # check json
  json_dump = JSON.dump(JSON.parse(IO.read(f))).gsub("\"", "\\\"").scan(/.{1,15000}/)
  json_dump[0..-2].each do |x|
    # Workaround MSVC error C2026
    out.write "  \"" + x + "\"\n"
  end
  out.write "  \"" + json_dump[-1] + "\",\n\n"
}

out.write "  NULL,\n"
out.write "};\n"
