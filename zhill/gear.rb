##
# Minimal Ruby wrapper around the gearlanced tool.
class Gear
  def initialize(n, bin='gearlanced')
    @hillsize = n

    stdin, @gear_in = IO.pipe
    @gear_out, stdout = IO.pipe
    @gear = Process.spawn(bin, n.to_s, :in=>stdin, :out=>stdout, :err=>[:child, :out])
  end

  def test(code)
    @gear_in.write("test\n")
    @gear_in.write(code.tr("\n", ' ') + "\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'

    Array.new(@hillsize) do |i|
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

  def set(i, code)
    @gear_in.write("set #{i}\n")
    @gear_in.write(code.tr("\n", ' ') + "\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'
  end

  def unset(i)
    @gear_in.write("unset #{i}\n")
    @gear_in.flush

    result = @gear_out.readline.chomp
    raise GearException, result if result != 'ok'
  end
end

##
# Exception class for BF Joust syntax errors
class GearException < RuntimeError; end
