#! /usr/bin/env ruby

require_relative 'zhill/hill'
require_relative 'zhill/bot'

if ARGV.length != 1
  puts "usage: #{$0} hill"
  exit
end

hill = Hill.new(ARGV[0])
bot = Bot.make(hill)

bot.start
