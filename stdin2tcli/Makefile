CC=gcc
TARGET=stdin2tcli

all:
	$(CC) $(TARGET).c -o $(TARGET).exe
	printf "1234567890\r\n" | ./$(TARGET).exe
