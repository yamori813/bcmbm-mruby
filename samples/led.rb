begin

yabm = YABM.new(1)

bit = 0

maxpin = 8

while 1 do

  yabm.print bit.to_s + "\n"
  yabm.gpiosetdat(1 << bit)
  bit = bit + 1
  if bit == maxpin
    bit = 0
  end
  start = yabm.count() 
  while yabm.count() < start + 10000 do
  end
end

rescue => e
  yabm.print e.to_s
end
