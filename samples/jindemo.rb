#
# yabm mruby script
#
#


def delay(yabm, val) 
  start = yabm.count() 
  while yabm.count() < start + val do
  end
end

def ledon(yabm) 
  dat = yabm.gpiogetdat
  yabm.gpiosetdat(dat & ~(1 << 4))
end

def ledoff(yabm) 
  dat = yabm.gpiogetdat
  yabm.gpiosetdat(dat | (1 << 4))
end

begin

# ip address setting

yabm = YABM.new

last = 0;
count = 0

yabm.gpiosetdat(1 << 3 | 1 << 4)

while 1 do
  delay(yabm, 200)
  if last == 0 && yabm.gpiogetdat & 0x01 == 1
    yabm.print "*" 
    ledon yabm
    last = 1
  end
  if last == 1 && yabm.gpiogetdat & 0x01 == 0
    ledoff yabm
    last = 0
  end
  if count % 5 == 0 then
    dat = yabm.gpiogetdat
    if dat & (1 << 5) == 0 then
      yabm.gpiosetdat(dat | (1 << 5))
    else
      yabm.gpiosetdat(dat & ~(1 << 5))
    end
  end
end

rescue => e
  yabm.print e.to_s
end
