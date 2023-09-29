begin

yabm = YABM.new

bit = 0

maxpin = 8

orgdir = yabm.gpiogetdir()
orgctl = yabm.gpiogetctl()
yabm.print "ORG DIR: " + orgdir.to_s + " CTL: " + orgctl.to_s + "\r\n"
yabm.gpiosetdir(0xff)

while 1 do

  yabm.print bit.to_s + ": "
  5.times {
    yabm.print "1"
    yabm.gpiosetdat(1 << bit)
    yabm.msleep(1000)
    yabm.print "0"
    yabm.gpiosetdat(0)
    yabm.msleep(1000)
  }
  bit = bit + 1
  if bit == maxpin
    bit = 0
  end
  yabm.print "\r\n"
end

rescue => e
  yabm.print e.to_s
end
