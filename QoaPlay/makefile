CC = g++
LD = g++
CFLAGS = -fbaserel -O2 -fomit-frame-pointer -mregparm -fno-exceptions -fno-rtti -D__NOLIBBASE__
LDFLAGS = -nostdlib -fbaserel -nostartfiles -fno-exceptions -fno-rtti
LIBS = -lminteger -lmfloat
OBJS = start.o main.o sysfile.o qoainput.o frame.o player-paula.o player-paula-mono8.o
EXE = QoaPlay

all: $(OBJS)
	@echo "Linking $(EXE)..."
	@$(LD) $(LDFLAGS) -o $(EXE).db $^ $(LIBS)
	@strip $(EXE).db -o $(EXE) --strip-unneeded
	@Protect $(EXE) +E
	List $(EXE)

clean:
	rm -vf *.o $(EXE) $(EXE).db

start.o: start.cpp
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -fwritable-strings -c -o $@ $<

frame.o: frame.s
	@echo "Assembling $@..."
	@Vasm -Faout -no-opt -o $@ $<

%.o: %.cpp
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c -o $@ $<

# dependencies

frame.o: frame.s
main.o: main.cpp main.h errors.h qoainput.h sysfile.h locale.h player-paula.h
player-paula.o: player-paula.cpp player-paula.h main.h errors.h
player-paula-mono8.o: player-paula-mono8.cpp player-paula-mono8.h player-paula.h
qoainput.o: qoainput.cpp main.h errors.h locale.h qoainput.h sysfile.h
start.o: start.cpp main.h
sysfile.o: sysfile.cpp sysfile.h main.h errors.h
