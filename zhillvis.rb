#! /usr/bin/env ruby

require_relative 'zhill/vis'

# Ruby script to generate pre-aggregated JSON statistics files
# required to generate the zhill web site visualizations. The input is
# a cranked hill statistics file from zhillcrank.rb.

if ARGV.length != 2
  puts "usage: #{$0} stats.bin output-directory [vis1 vis2 ...]"
  exit
end

in_file, out_dir, *what = ARGV

stats = File.open(in_file, 'rb') do |f|
  data = Marshal.load(f)
  Stats.new(data[:names], data[:data])
end

Vis.generate(stats, out_dir, what)
