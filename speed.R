speed <- read.csv("speed.csv")
speed
n <- length(speed$frame)
plot(speed$frame/30.0, speed$speed, type = "l", col = "blue", ylab="speed", xlab="Time (sec)", main="BMW M5 F80 speed")
str(speed)
