#
# bcmbm-mruby mruby script
#

begin

yabm = YABM.new

yabm.print "Hello Bear Metal mruby on YABM"

loop do
   yabm.print "."
   yabm.msleep 500
end

rescue => e
  yabm.print e.to_s
end
