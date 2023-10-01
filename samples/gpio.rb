def bit(val)
bit = 0
  while val != 0
    val = val >> 1
    bit = bit + 1
  end
  return bit - 1
end

begin

yabm = YABM.new

count = 0

last = yabm.gpiogetdat

loop do
  yabm.msleep(100)
  val = yabm.gpiogetdat
  if val != last
    b = bit((val - last).abs)
    yabm.print "change bit: " + b.to_s + "\r\n"
    last = val
  end
end

rescue => e
  yabm.print e.to_s
end
