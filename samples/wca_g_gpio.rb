# WCA-G GPIO

# LED1 is direct connect to VCC
LED10 = (1 << 6)
LED2 = (1 << 2)
LED6 = (1 << 1)
LED11 = (1 << 7)
# LED7 and LED4 is not gpio
SW1 = (1 << 0)
SW2 = (1 << 4)
SW3_1 = (1 << 3)
SW3_2 = (1 << 5)

begin

yabm = YABM.new

bit = 0

maxpin = 8

orgdir = yabm.gpiogetdir()
orgctl = yabm.gpiogetctl()
yabm.print "ORG DIR: " + orgdir.to_s + " CTL: " + orgctl.to_s + "\r\n"
yabm.gpiosetdir(LED10 | LED2 | LED6 | LED11)

leds = [LED10, LED2, LED6, LED11]

while 1 do
  leds.each {|led|

    yabm.print ledto_s + " "
    yabm.gpiosetdat(led)
    start = yabm.count() 
    while yabm.count() < start + 5000 do
    end
    yabm.print "input: " + yabm.gpiogetdat.to_s
  }
  yabm.print "\r\n"
end

rescue => e
  yabm.print e.to_s
end
