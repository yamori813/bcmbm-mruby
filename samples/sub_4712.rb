#
# yabm mruby script
# BCM4712 subroutine script
#

def gpioinit yabm
  yabm.gpiosetdat(1 << 3 | 1 << 4)
end

def ledon yabm
    yabm.gpiosetdat(1 << 3)
end

def ledoff yabm
  yabm.gpiosetdat(1 << 3 | 1 << 4)
end
