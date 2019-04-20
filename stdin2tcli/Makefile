CC=gcc
TARGET=stdin2tcli

all:
	$(CC) $(TARGET).c -o $(TARGET)
	printf "1234567890\r\n" | ./$(TARGET)
