#! /usr/bin/env ruby

require 'cgi'
require 'json'
require 'socket'

SOCKET = '/home/bfjoust/bfjoust/socket/server.sock'
#SOCKET = '/home/fis/src/chainlance/server.sock'

def submit(prog, code, try_only)
  prog = prog.tr("\n", ' ')
  code = code.tr("\n", ' ')

  joustcode = code.tr('^({})*%0-9 []<>.+-','')
  bfcode = joustcode.tr('^[]<>.+-','')

  if prog.length == 0
    return { :result => :error, :message => "Missing program name not allowed." }
  elsif joustcode.length < code.length/2 || bfcode.length == 0
    return { :result => :error, :message => "Spam protection circuit fired: program does not look BF Jousty enough." }
  end

  UNIXSocket.open(SOCKET) do |sock|
    sock.write("#{try_only ? 'test' : 'joust'}\n#{prog}\n#{code}")
    sock.close_write

    reply = sock.read.split("\n", 2)

    if reply.length != 2 || (reply[0] != 'ok' && reply[0] != 'error')
      { :result => :error, :message => "Unparseable reply: #{reply}" }
    elsif reply[0] == 'error'
      { :result => :error, :message => reply[1] }
    else
      { :result => :ok, :message => reply[1] }
    end
  end
end

result = nil
cgi = CGI.new

begin
  if cgi.has_key?('prog') && cgi.has_key?('code') && cgi.has_key?('submit')
    result = submit(cgi['prog'], cgi['code'], cgi['submit'] != 'yes')
  else
    result = { :result => :error, :message => "Missing query parameters." }
  end
rescue => err
  result = { :result => :error, :message => "Exception: #{err.inspect}" }
end

cgi.out('application/json') { JSON.dump(result) }
