#! /usr/bin/env ruby

require_relative '../zhill/gear'

if ARGV.length != 2
  puts "usage: #{$0} gearlanced-binary N"
  exit
end

pointmap = { -1 => '<', 0 => 'X', 1 => '>' }

gear = Gear.new(ARGV[1].to_i, ARGV[0])

while !(line = STDIN.gets()).nil?
  line.strip!

  if line.start_with?('set ')
    index = line[4..-1].to_i
    prog = STDIN.gets().strip()
    begin
      gear.set(index, prog)
      puts "ok"
    rescue => e
      puts "error: #{e}"
    end
    STDOUT.flush
  elsif line == 'test'
    prog = STDIN.gets().strip()
    begin
      results = gear.test(prog)
      puts "ok"
      results.each do |points|
        encoded = points.map { |p| pointmap[p] }.join('')
        puts "#{encoded[0..20]} #{encoded[21..-1]}"
      end
    rescue => e
      puts "error: #{e}"
    end
    STDOUT.flush
  else
    puts "error: i don't get it"
    exit
  end
end
