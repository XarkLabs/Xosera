#!/usr/bin/env ruby
#
# Converts copper list binary to C array of uint16_t
# Copyright (c) 2021 Ross Bamford
# See LICENSE.md (hint: MIT)
#
if $0 == __FILE__
  def compact_words(words)
    words.flatten.each_slice(9).map do |row|
      "    #{row.join(', ')}"
    end.join("\n")
  end

  def expanded_words(words)
    words.map { |(w1,w2)| "    #{w1}, #{w2}" }.join(",\n")
  end

  compact = !ARGV.reject! { |a| a == '-c' }.nil?

  if (ARGV.length != 1)
    puts "Usage: bin2c.rb [-c] <binfile>"
  else
    if !File.exists?(fn = File.expand_path(ARGV.first))
      puts "Error: '#{fn}' not found"
    else
      words = []

      File.open(fn, "rb") do |f|
        loop do
          break if f.eof?
          w1 = f.read(2).unpack('n').first
          break if f.eof?
          w2 = f.read(2).unpack('n').first

          words << ["0x%04x" % w1, "0x%04x" % w2]
        end
      end
  
      puts "// #{File.basename(fn)} #{'(compact)' if compact}"
      puts <<~EOT
      uint16_t copper_list_size = #{words.length * 2};
      uint16_t copper_list = [
      #{compact ? compact_words(words) : expanded_words(words)}
      };
      EOT
    end
  end
end

