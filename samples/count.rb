begin

yabm = YABM.new

count = 0

while 1 do
  yabm.msleep(1000)
  yabm.print "*"
  count = count + 1
  if count % 10 == 0
    yabm.print "\n"
  end
end

rescue => e
  yabm.print e.to_s
end
