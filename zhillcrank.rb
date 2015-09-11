#! /usr/bin/env ruby

require_relative 'zhill/vis'

# Ruby script to crank a hill of programs, and store the full
# generated statistics as a marshalled Ruby object. This object can be
# used to generate visualizations later on.

if ARGV.length < 4
  puts "usage: #{$0} stats.bin cranklanced prog1.bfjoust prog2.bfjoust ..."
  exit
end

out, bin, *progs = ARGV
names, data = Vis.crank(bin, progs)

File.open(out, 'wb') do |f|
  Marshal.dump({ :names => names, :data => data }, f)
end
