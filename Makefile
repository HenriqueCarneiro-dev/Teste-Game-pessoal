CC      = gcc
CFLAGS  = -Wall -std=c99
LIBS    = -lraylib -lopengl32 -lgdi32 -lwinmm -lm

SRC = $(wildcard src/*.c)
HDR = $(wildcard src/*.h)

volterra.exe: $(SRC) $(HDR)
	$(CC) $(SRC) -o volterra.exe $(CFLAGS) $(LIBS)

run: volterra.exe
	./volterra.exe

clean:
	rm -f volterra.exe