require 'date'
require 'open-uri'
require 'socket'
require 'thread'

require_relative 'hill'

# BF Joust hill server.

class Server
  # Maximum number of currently pending jobs.
  QUEUE_SIZE = 16
  # Maximum size (in bytes) for web requests.
  HTTP_MAX = 2*1024*1024

  # Constructs a new server.
  #
  # +hill_manager+ must be a HillManager object.  \Server configuration
  # will be read from the +server+ section of its configuration.  The
  # following configuration keys are known.  All keys are optional,
  # and will disable the corresponding function if not set.
  #
  # [report]     Text file to write the plaintext report to.
  # [breakdown]  Text file to write the plaintext breakdown to.
  # [json]       JSON/JavaScript file to write the JSON report to.
  # [jsonvar]    If specified, writes an assignment to this variable
  #              around the JSON object, so that the result can be
  #              included directly as a script on a page.
  # [socket]     Unix domain socket path to provide service at.
  #
  # A worker thread will be used to handle the submitted jobs.  The
  # callback will be called from within the single worker thread, and
  # should not block for extended periods of time.
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

    update_reports()
  end

  # Sets the herald callback, used to announce hill changes.
  #
  # Summary messages for new jousts via the Unix domain socket (as
  # well as some fatal error messages) will be passed to this block.
  def herald(&herald) # :yields: message
    @herald = herald
  end

  # Announces +message+ via the current herald.
  def heraldize(message)
    @herald.call(message) unless @herald.nil?
  end

  # Logs a message to the server's hill log.
  #
  # If +err+ is set, it should be an exception, the backtrace of which
  # is then included in the log.
  def log(message, err=nil)
    tstamp = DateTime.now.strftime('[%Y-%m-%d %H:%M:%S] ')
    @log.write(tstamp + message + "\n")
    @log.write(err.backtrace.join("\n") + "\n") unless err.nil?
    @log.flush
  end

  # Submits a joust for processing.
  #
  # The name of the new challenger is +prog+, and +code+ gives its
  # source code; either as is, or as a http/ftp URL.  If +try_only+ is
  # true, the hill will not be changed.
  #
  # The callback's +result+ is one of +:ok+ (success), +:error+ (bad
  # program or transient failure) or +:fatal+ (fatal error).  The
  # +message+ argument provides further detail.  In case of a
  # successful execution, this is the single-line summary of the
  # submitted program's score.
  def joust(prog, code, try_only, &callback) # :yields: result, message
    job = {
      :task => :joust,
      :prog => prog,
      :code => code,
      :try_only => try_only,
      :callback => callback
    }
    submit(job)
  end

  # Requests a termination of the server thread.
  def quit
    submit({ :task => :quit })
  end

  # Submits a new job.
  def submit(job)
    if @queue.length >= QUEUE_SIZE
      job[:callback].call(:error, 'System busy; ask again later.')
      return
    end

    @queue.push(job)
  end
  private :submit

  # Worker thread's main loop.
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

  # Worker thread's joust execution code.
  def work_joust(prog, code, try_only, callback)
    # fetch web URL if code looks like one

    if /^(?:https?|ftp):\/\// =~ code
      begin
        URI.open(code) do |f|
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
        update_reports(prog)
      end

      callback.call(:ok, summary)

    rescue GearException => err
      callback.call(:error, err.message)
    end
  end
  private :work_joust

  # Updates the report files. Generally called by the worker, but also once right at startup.
  # This latter call does not update breakdown.txt, because there's no program to apply it to.
  def update_reports(prog=nil)
    report = @cfg['report']
    File.open(report, 'w') { |f| @hill_manager.write_report(f) } unless report.nil?

    unless prog.nil?
      breakdown = @cfg['breakdown']
      File.open(breakdown, 'w') { |f| @hill_manager.write_breakdown(prog, f) } unless breakdown.nil?
    end

    json = @cfg['json']
    File.open(json, 'w') { |f| @hill_manager.write_json(f, @cfg['jsonvar']) } unless json.nil?
  end
  private :update_reports
end

# Unix domain socket server for a BF Joust hill.
class SocketServer
  # Listening Unix domain socket.
  attr_reader :socket

  # Constructs a new Unix domain endpoint for a Server +server+.
  #
  # The server will accept connections at +socketpath+.  A single
  # worker thread will be spawned to handle connections.
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

  # Worker thread's main loop.
  def serve
    while true
      client = nil

      # accept connection

      begin
        client = @socket.accept
      rescue => err
        @server.log(err.inspect, err)
        @server.heraldize('fizzie! Socket server trouble (in accept)! ' + err.inspect)
        break
      end

      # serve client

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

  # Worker thread's client handling code.
  def handle(client)
    # read in, parse and verify request

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

    # mangle program name and code for web submissions

    prog = orig_prog.gsub(/[^a-zA-Z0-9_-]/, '')
    if prog != orig_prog
      return "Program name (#{orig_prog}) is restricted to characters in [a-zA-Z0-9_-], sorry."
    elsif prog.length > 48
      return "Program name is overly long. 48 characters should be enough for everyone."
    end

    prog = "web." + prog

    code = ' ' + code if code.start_with?('http', 'ftp') # evade HTTP requests
    code.tr!('\n', ' ')

    # submit for processing at the server

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
