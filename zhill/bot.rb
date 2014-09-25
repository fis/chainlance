require 'cinch'
require 'date'
require 'open-uri'
require 'thread'

require_relative 'gear'

module Bot
  def Bot.make(hill)
    cfg = hill.cfg['irc']
    command = cfg['command']
    log = File.open(File.join(hill.dir, 'hill.log'), 'a')

    hill_mutex = Mutex.new

    Cinch::Bot.new do
      configure do |c|
        c.server = cfg['server']
        c.port = cfg['port'] || 6667
        c.nick = cfg['nick']
        c.realname = cfg['nick']
        c.channels = cfg['channels'].split
      end

      if cfg['help']
        on :message, cfg['help'] do |m|
          m.reply('I do !bfjoust; see http://zem.fi/bfjoust/ for more information.', true)
        end
      end

      on :message, /^#{Regexp.escape(command)}/ do |m|
        msg = m.message.split(' ', 3)

        if msg.length <= 2
          m.reply("\"#{command} progname code\". See http://zem.fi/bfjoust/ for documentation.", true)
          break
        end

        orig_prog = msg[1]
        code = msg[2]

        prog = orig_prog.gsub(/[^a-zA-Z0-9_-]/, '')
        if prog != orig_prog
          m.reply("Program name (#{orig_prog}) is restricted to characters in [a-zA-Z0-9_-], sorry.", true)
          break
        elsif prog.length > 48
          m.reply('Program name is overly long. 48 characters should be enough for everyone.', true)
          break
        end

        nick = m.user.nick
        prog = nick + '.' + prog

        tstamp = DateTime.now.strftime('%Y-%m-%d %H:%M:%S')
        log.write("#{tstamp} #{nick} #{code}\n")
        log.flush

        if /^(?:https?|ftp):\/\// =~ code
          maxsize = 2*1024*1024

          begin
            open(code) do |f|
              code = f.read(maxsize + 1)
              raise "maximum size limit (#{maxsize} bytes) exceeded" if code.length > maxsize
            end
          rescue => err
            m.reply("URL fetch problems: #{err.message}", true)
            break
          end
        end

        begin
          summary = nil

          hill_mutex.synchronize do
            # run challenge

            summary = hill.challenge(prog, code)

            # update web reports

            report = cfg['report']
            File.open(report, 'w') { |f| hill.write_report(f) } unless report.nil?

            breakdown = cfg['breakdown']
            File.open(breakdown, 'w') { |f| hill.write_breakdown(prog, f) } unless breakdown.nil?

            json = cfg['json']
            File.open(json, 'w') { |f| hill.write_json(f, cfg['jsonvar']) } unless json.nil?
          end

          m.reply(summary)
          if cfg['summarychannel'] && m.target.name != cfg['summarychannel']
            Channel(cfg['summarychannel']).msg(summary)
          end

        rescue GearException => err
          m.reply(err.message, true)
          break

        rescue => err
          m.reply('I broke down! Ask fizzie to help! The details are in the log!', true)
          log.write(err.inspect + "\n")
          log.write(err.backtrace.join("\n") + "\n")
          log.flush
          m.bot.quit('Abandon ship, abandon ship!')
        end
      end
    end
  end
end
