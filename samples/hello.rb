#
# bcmbm-mruby mruby script
#

begin

yabm = YABM.new

yabm.print "Hello Bear Metal mruby on YABM"

while 1 do
   yabm.print "."
   start = yabm.count() 
   while yabm.count() < start + 500 do
   end
end

rescue => e
  yabm.print e.to_s
end
