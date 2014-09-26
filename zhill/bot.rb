require 'cinch'

require_relative 'gear'

module Bot
  def Bot.make(cfg, server)
    command = cfg['command']
    testcommand = cfg['testcommand']

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

      if cfg['summarychannel']
        server.herald do |message|
          Channel(cfg['summarychannel']).msg(message)
        end
      end

      joust = lambda do |m, orig_prog, code, try_only|
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

        server.log(prog + ' ' + code)
        server.joust(prog, code, try_only) do |result, message|
          if result == :fatal
            m.reply('I broke down! Ask fizzie to help! The details are in the log! ' + message, true)
            m.bot.quit('Abandon ship, abandon ship!')
          end

          m.reply(message, result == :error)
          if result == :ok &&
              cfg['summarychannel'] &&
              m.target.name != cfg['summarychannel'] &&
              !try_only
            Channel(cfg['summarychannel']).msg(message)
          end
        end
      end

      on :message, /^#{Regexp.escape(command)}/ do |m|
        msg = m.message.split(' ', 3)

        if msg.length <= 2
          m.reply("\"#{command} progname code\". See http://zem.fi/bfjoust/ for documentation.", true)
          break
        end

        joust.call(m, msg[1], msg[2], false)
      end

      unless testcommand.nil?
        on :message, /^#{Regexp.escape(testcommand)}/ do |m|
          msg = m.message.split(' ', 3)

          if msg.length <= 2
            m.reply("\"#{testcommand} progname code\". See http://zem.fi/bfjoust/ for documentation.", true)
            break
          end

          joust.call(m, msg[1], msg[2], true)
        end
      end
    end
  end
end
