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
    @gear_in.write([0x01, code.length].pack('I<2'))
    @gear_in.write(code)
    @gear_in.flush

    ok, reply = get()
    raise GearException, reply or 'geartalk error' unless ok

    min_tape, max_tape = reply[1].ord, reply[2].ord
    tapes = max_tape - min_tape + 1

    Array.new(@size) do
      # TODO: fix statistics
      #stats = nil
      #if reply.with_statistics
      #  stats_count = ::Protobuf::Decoder::read_varint(@gear_out)
      #  stats = Array.new(stats_count) do
      #    s = get(Geartalk::Statistics.new)
      #    {
      #      :cycles => s.cycles,
      #      :tape_abs => s.tape_abs,
      #      :tape_max => s.tape_max,
      #      :heat_position => s.heat_position.to_a,
      #    }
      #  end
      #end

      joust = @gear_out.read(2 * tapes)
      raise GearException, 'short points data' unless joust.length == 2 * tapes
      joust = joust.unpack('c*')

      joust
      #reply.with_statistics ?
      #  {
      #    :points => joust.sieve_points + joust.kettle_points,
      #    :cycles => joust.cycles,
      #    :stats => stats,
      #  } : joust.sieve_points + joust.kettle_points
    end
  end

  # Sets the +i+'th program's code to +code+.
  #
  # The indices run from 0 to #size-1.
  def set(i, code)
    @gear_in.write([0x02 | (i << 8), code.length].pack('I<2'))
    @gear_in.write(code)
    @gear_in.flush

    ok, reply = get()
    raise GearException, reply or 'geartalk error' unless ok
  end

  # Unsets the +i+'th program.
  #
  # This will deallocate the storage used by the program, and leave
  # the corresponding program slot to contain the initial "loses to
  # everything" dummy program.
  def unset(i)
    @gear_in.write([0x03 | (i << 8), 0].pack('I<2'))
    @gear_in.flush

    ok, reply = get()
    raise GearException, reply or 'geartalk error' unless ok
  end

  # Receives a reply message from the gearlanced process.
  def get()
    header = @gear_out.read(4)
    raise GearException, 'short reply header' unless header.length == 4
    if (header[0].ord & 0x01) == 0
      error = @gear_out.read(header.unpack('I<')[0] >> 8)
      return false, error
    end
    return true, header
  end
  private :get
end

# Exception class for BF Joust syntax errors.
class GearException < RuntimeError; end
