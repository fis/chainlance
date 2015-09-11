require 'protobuf/message/decoder'
require 'protobuf/message/field'

require_relative 'geartalk.pb'

# Minimal Ruby wrapper class around the gearlanced tool.

class Gear
  # Number of program slots in the +gearlanced+ instance.
  attr_reader :size

  # Spawns a new gearlanced process to manage a set of +n+ programs.
  #
  # If gearlanced is not in your path, the +bin+ parameter can be
  # set to an absolute path.
  def initialize(n, bin='gearlanced')
    @size = n

    stdin, @gear_in = IO.pipe
    @gear_out, stdout = IO.pipe
    @gear = Process.spawn(bin, n.to_s, :in=>stdin, :out=>stdout, :err=>[:child, :out])
  end

  # Tests +code+ against all programs currently in the hill.
  #
  # Returns an array of #size arrays, each of which contains the
  # results against a particular program.  The result arrays have one
  # element for each tape length/polarity configuration (normally
  # 2*21 = 42), and each element is +1, 0 or -1 if the tested program
  # won, tied or lost (respectively) against the corresponding program
  # on the hill.
  #
  # If the running binary is cranklanced, returns #size hashes
  # instead. Each hash contains the raw Joust message (key :joust),
  # and the possibly empty list of Statistics messages (key :stats)
  # returned by cranklanced.
  def test(code)
    put(Action::TEST)
    @gear_in.write(code.tr("\0", ' ') + "\0")
    @gear_in.flush

    reply = get(Reply.new)
    raise GearException, reply.error unless reply.ok

    Array.new(@size) do
      stats = nil
      if reply.with_statistics
        stats_count = ::Protobuf::Decoder::read_varint(@gear_out)
        stats = Array.new(stats_count) do
          s = get(Statistics.new)
          {
            :cycles => s.cycles,
            :tape_abs => s.tape_abs,
            :tape_max => s.tape_max,
            :heat_position => s.heat_position.to_a,
          }
        end
      end

      joust = get(Joust.new)
      if joust.sieve_points.length == 0 || joust.kettle_points.length != joust.sieve_points.length
        raise GearException, "gearlanced produced gibberish: #{joust.inspect}"
      end

      reply.with_statistics ?
        {
          :points => joust.sieve_points + joust.kettle_points,
          :cycles => joust.cycles,
          :stats => stats,
        } : joust.sieve_points + joust.kettle_points
    end
  end

  # Sets the +i+'th program's code to +code+.
  #
  # The indices run from 0 to #size-1.
  def set(i, code)
    put(Action::SET, i)
    @gear_in.write(code.tr("\0", ' ') + "\0")
    @gear_in.flush

    reply = get(Reply.new)
    raise GearException, reply.error unless reply.ok
  end

  # Unsets the +i+'th program.
  #
  # This will deallocate the storage used by the program, and leave
  # the corresponding program slot to contain the initial "loses to
  # everything" dummy program.
  def unset(i)
    put(Action::UNSET, i)
    @gear_in.flush

    reply = get(Reply.new)
    raise GearException, reply.error unless reply.ok
  end

  # Sends a protobuf command message to the gearlanced process.
  def put(action, index=nil)
    cmd = Command.new
    cmd.action = action
    cmd.index = index unless index.nil?
    bytes = cmd.serialize_to_string
    len = ::Protobuf::Field::VarintField.encode(bytes.length)
    @gear_in.write(len)
    @gear_in.write(bytes)
  end
  private :put

  # Receives a protobuf message from the gearlanced process.
  def get(message)
    len = ::Protobuf::Decoder::read_varint(@gear_out)
    bytes = @gear_out.read(len)
    message.parse_from_string(bytes)
  end
  private :get
end

# Exception class for BF Joust syntax errors.
class GearException < RuntimeError; end
