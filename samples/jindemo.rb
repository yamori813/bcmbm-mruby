#
# yabm mruby script
#
#

def delay(yabm, val) 
  start = yabm.count() 
  while yabm.count() < start + val do
  end
end

begin

# ip address setting

yabm = YABM.new

interval = 0

yabm.gpiosetdat(1 << 3 | 1 << 4)

while 1 do
  yabm.print "."
  delay(yabm, 200)
  if interval == 0 && yabm.gpiogetdat & 0x01 == 1
    yabm.print "*" 
    yabm.gpiosetdat(1 << 3)
    interval = 1
  end
  if interval != 0
    interval = interval + 1
  end
  if interval == 15
    interval = 0
    yabm.gpiosetdat(1 << 3 | 1 << 4)
  end
end

rescue => e
  yabm.print e.to_s
end
