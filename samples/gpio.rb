def bit(val)
bit = 0
  while val != 0 do
    val = val >> 1
    bit = bit + 1
  end
  return bit - 1
end

begin

yabm = YABM.new

count = 0

last = yabm.gpiogetdat

while 1 do

  start = yabm.count() 
  while yabm.count() < start + 1000 do
  end
  val = yabm.gpiogetdat
  if val != last
    b = bit((val - last).abs)
    yabm.print b.to_s + "\n"
    last = val
  end
end

rescue => e
  yabm.print e.to_s
end
