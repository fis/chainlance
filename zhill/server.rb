require 'date'
require 'open-uri'
require 'thread'

require_relative 'hill'

class Server
  QUEUE_SIZE = 16
  HTTP_MAX = 2*1024*1024

  def initialize(hill_manager)
    @hill_manager = hill_manager
    @cfg = hill_manager.cfg['server']
    @queue = Queue.new
    @worker = Thread.new { work }
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
          job[:callback].call(:fatal, err.inspect + "\n" + err.backtrace.join("\n"))
          break
        end
      else
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
