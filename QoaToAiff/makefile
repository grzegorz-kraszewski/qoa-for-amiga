CC = g++
LD = g++
CFLAGS = -fbaserel -O2 -fomit-frame-pointer -mregparm -fno-exceptions -fno-rtti -D__NOLIBBASE__
LDFLAGS = -nostdlib -fbaserel -nostartfiles -fno-exceptions -fno-rtti
LIBS = -lminteger -lmfloat
OBJS = start.o main.o sysfile.o qoainput.o aiffoutput.o timing.o frame.o nicenumber.o
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

%.o: %.s
	@echo "Assembling $@..."
	@Vasm -Faout -no-opt -o $@ $<

%.o: %.cpp
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c -o $@ $<

# dependencies

aiffoutput.o: aiffoutput.cpp main.h aiffoutput.h sysfile.h
frame.o: frame.s
main.o: main.cpp main.h errors.h timing.h qoainput.h sysfile.h aiffoutput.h locale.h nicenumber.h
nicenumber.o: nicenumber.cpp locale.h nicenumber.h main.h
qoainput.o: qoainput.cpp main.h errors.h locale.h qoainput.h sysfile.h nicenumber.h
start.o: start.cpp main.h
sysfile.o: sysfile.cpp sysfile.h main.h errors.h
timing.o: timing.cpp main.h timing.h
