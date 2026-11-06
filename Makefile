CC      = gcc
CFLAGS  = -Wall -std=c99
LIBS    = -lraylib -lopengl32 -lgdi32 -lwinmm -lm

volterra.exe: src/main.c
    $(CC) src/main.c -o volterra.exe $(CFLAGS) $(LIBS)

run: volterra.exe
    ./volterra.exe

clean:
    rm -f volterra.exe