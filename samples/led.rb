begin

yabm = YABM.new

pins = [0, 1, 2, 3, 4, 5, 6, 7]

orgdir = yabm.gpiogetdir
orgctl = yabm.gpiogetctl
yabm.print "ORG DIR: " + orgdir.to_s + " CTL: " + orgctl.to_s + "\r\n"
yabm.gpiosetdir(0xff)

loop do
  pins.each do |n|
    yabm.print n.to_s + ": "

    5.times {
      yabm.print "1"
      yabm.gpiosetdat(1 << n)
      yabm.msleep(1000)
      yabm.print "0"
      yabm.gpiosetdat(0)
      yabm.msleep(1000)
    }

    yabm.print "\r\n"
  end
end

rescue => e
  yabm.print e.to_s
end
