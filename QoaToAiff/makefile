CC = g++
LD = g++
CFLAGS = -fbaserel -O2 -fomit-frame-pointer -mregparm -fno-exceptions -fno-rtti -D__NOLIBBASE__
LDFLAGS = -nostdlib -fbaserel -nostartfiles -fno-exceptions -fno-rtti
LIBS = -lmfloat
OBJS = start.o main.o frame.o timing.o
EXE = QoaToAiff

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

main.o: main.cpp timing.h
start.o: start.cpp
frame.o: frame.s
timing.o: timing.cpp timing.h
