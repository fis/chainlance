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
  def test(code)
    @gear_in.write("test\n")
    @gear_in.write(code.tr("\n", ' ') + "\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'

    Array.new(@size) do |i|
      points = @gear_out.readline.chomp.split
      if points.length != 2 || points[0].length != points[1].length
        raise GearException, "gearlanced produced gibberish: #{points.inspect}"
      end

      points = points[0] + points[1]
      if points.tr('<>X', '').length > 0
        raise GearException, 'gearlanced produced gibberish points: #{points}'
      end

      pointmap = { '<' => -1, 'X' => 0, '>' => 1 }
      points.chars.map { |p| pointmap[p] }
    end
  end

  # Sets the +i+'th program's code to +code+.
  #
  # The indices run from 0 to #size-1.
  def set(i, code)
    @gear_in.write("set #{i}\n")
    @gear_in.write(code.tr("\n", ' ') + "\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'
  end

  # Unsets the +i+'th program.
  #
  # This will deallocate the storage used by the program, and leave
  # the corresponding program slot to contain the initial "loses to
  # everything" dummy program.
  def unset(i)
    @gear_in.write("unset #{i}\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'
  end
end

# Exception class for BF Joust syntax errors.
class GearException < RuntimeError; end
