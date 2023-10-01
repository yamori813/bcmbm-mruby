begin

yabm = YABM.new

count = 0

loop do
  yabm.msleep(1000)
  yabm.print "*"
  count += 1
  if count % 10 == 0
    yabm.print "\n"
  end
end

rescue => e
  yabm.print e.to_s
end
