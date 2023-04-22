#
# yabm mruby script
# BCM4712 subroutine script
#

def gpioinit yabm
  yabm.gpiosetdat(1 << 6)
end

def ledon yabm
  dat = yabm.gpiogetdat
  yabm.gpiosetdat(dat & ~(1 << 6))
end

def ledoff yabm
  dat = yabm.gpiogetdat
  yabm.gpiosetdat(dat | (1 << 6))
end
