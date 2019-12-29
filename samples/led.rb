begin

yabm = YABM.new

bit = 0

maxpin = 8

orgdir = yabm.gpiogetdir()
orgctl = yabm.gpiogetctl()
yabm.print "ORG DIR: " + orgdir.to_s + " CTL: " + orgctl.to_s + "\r\n"
yabm.gpiosetdir(0xff)

while 1 do

  yabm.print bit.to_s + " "
  yabm.gpiosetdat(1 << bit)
  bit = bit + 1
  if bit == maxpin
    bit = 0
  end
  6.times {
    start = yabm.count() 
    while yabm.count() < start + 1000 do
    end
    yabm.print "*"
  }
  yabm.print "\r\n"
end

rescue => e
  yabm.print e.to_s
end
