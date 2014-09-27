require 'date'
require 'open-uri'
require 'socket'
require 'thread'

require_relative 'hill'

class Server
  QUEUE_SIZE = 16
  HTTP_MAX = 2*1024*1024

  attr_reader :herald

  def initialize(hill_manager)
    @hill_manager = hill_manager
    @cfg = hill_manager.cfg['server']

    @log = File.open(File.join(hill_manager.dir, 'hill.log'), 'a')
    @herald = nil

    @queue = Queue.new
    @worker = Thread.new { work }

    socket = @cfg['socket']
    if socket

      @socket_server = SocketServer.new(self, socket)
    end
  end

  def herald(&herald)
    @herald = herald
  end
  def heraldize(message)
    @herald.call(message) unless @herald.nil?
  end

  def log(message, err=nil)
    tstamp = DateTime.now.strftime('[%Y-%m-%d %H:%M:%S] ')
    @log.write(tstamp + message + "\n")
    @log.write(err.backtrace.join("\n") + "\n") unless err.nil?
    @log.flush
  end

  def joust(prog, code, try_only, &callback)
    job = {
      :task => :joust,
      :prog => prog,
      :code => code,
      :try_only => try_only,
      :callback => callback
    }
    submit(job)
  end

  def quit
    submit({ :task => :quit })
  end

  def submit(job)
    if @queue.length >= QUEUE_SIZE
      job[:callback].call(:error, 'System busy; ask again later.')
      return
    end

    @queue.push(job)
  end
  private :submit

  def work
    while true
      job = @queue.pop
      task = job[:task]

      if task == :quit
        break
      elsif task == :joust
        begin
          work_joust(job[:prog], job[:code], job[:try_only], job[:callback])
        rescue => err
          log(err.inspect, err)
          job[:callback].call(:fatal, err.inspect)
          break
        end
      else
        log("Broken job: #{job}")
        job[:callback].call(:fatal, "Broken job: #{job}.")
        break
      end
    end
  end
  private :work

  def work_joust(prog, code, try_only, callback)
    # fetch web URL if code looks like one

    if /^(?:https?|ftp):\/\// =~ code
      begin
        open(code) do |f|
          code = f.read(HTTP_MAX + 1)
          raise "maximum size limit (#{HTTP_MAX} bytes) exceeded" if code.length > HTTP_MAX
        end
      rescue => err
        callback.call(:error, "URL fetch problems: #{err.message}")
        return
      end
    end

    begin
      # run challenge

      summary = @hill_manager.challenge(prog, code, try_only)

      # update web reports

      unless try_only
        report = @cfg['report']
        File.open(report, 'w') { |f| @hill_manager.write_report(f) } unless report.nil?

        breakdown = @cfg['breakdown']
        File.open(breakdown, 'w') { |f| @hill_manager.write_breakdown(prog, f) } unless breakdown.nil?

        json = @cfg['json']
        File.open(json, 'w') { |f| @hill_manager.write_json(f, @cfg['jsonvar']) } unless json.nil?
      end

      callback.call(:ok, summary)

    rescue GearException => err
      callback.call(:error, err.message)
    end
  end
end

class SocketServer
  attr_reader :socket

  def initialize(server, socketpath)
    @server = server

    if File.socket?(socketpath)
      File.unlink(socketpath)
    elsif File.exists?(socketpath)
      raise "socket path (#{socketpath}) in use by a non-socket"
    end
    @socket = UNIXServer.new(socketpath)
    File.chmod(0777, socketpath)

    Thread.new { serve }
  end

  def serve
    while true
      client = nil

      begin
        client = @socket.accept
      rescue => err
        @server.log(err.inspect, err)
        @server.heraldize('fizzie! Socket server trouble (in accept)! ' + err.inspect)
        break
      end

      begin
        refused = handle(client)
        unless refused.nil?
          client.send("error\n#{refused}\n", 0)
          client.close
        end
      rescue => err
        begin
          client.close unless client.closed?
        rescue
          # just ignore
        end
        @server.log(err.inspect, err)
      end
    end
  end
  private :serve

  def handle(client)
    request = ''
    while request.length < Server::HTTP_MAX+1
      data = client.recv(Server::HTTP_MAX + 1 - request.length)
      break if data.length == 0
      request += data
    end
    return "Request too long: maximum #{Server::HTTP_MAX} bytes." if request.length > Server::HTTP_MAX

    request = request.split("\n", 3)
    return "Bad request: #{request}" if request.length != 3

    command, orig_prog, code = request

    return "Bad command: #{command}" unless command == 'joust' || command == 'test'

    prog = orig_prog.gsub(/[^a-zA-Z0-9_-]/, '')
    if prog != orig_prog
      return "Program name (#{orig_prog}) is restricted to characters in [a-zA-Z0-9_-], sorry."
    elsif prog.length > 48
      return "Program name is overly long. 48 characters should be enough for everyone."
    end

    prog = "web." + prog

    code = ' ' + code if code.start_with?('http', 'ftp') # evade HTTP requests
    code.tr!('\n', ' ')

    @server.log(prog + ' ' + code[0,256])
    @server.joust(prog, code, command == 'test') do |result, message|
      begin
        if result == :fatal
          client.send("error\nFatal error: #{message}\n", 0)
        elsif result == :error
          client.send("error\n#{message}\n", 0)
        else
          @server.heraldize(message) unless command == 'test'
          client.send("ok\n#{message}\n", 0)
        end
      ensure
        client.close
      end
    end

    nil
  end
  private :handle
end
