# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = gta_campaign
SRC = gta_campaign.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	@echo "Ejecutando con n=7..."
	./$(TARGET) 7

clean:
	rm -f $(TARGET)
