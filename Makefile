CC     = gcc
CFLAGS = -Wall -std=c99 -O2
LIBS   = -lraylib -lopengl32 -lgdi32 -lwinmm -lm
TARGET = VolTerra.exe
SRC    = src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean