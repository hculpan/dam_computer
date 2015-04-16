TARGET_PLATFORM=none
#VICE_BASE=C:/Users/usucuha/retrocomputing/C64/WinVICE-2.4-x64
#VICE_TARGET=$(VICE_BASE)/x64.exe
#VICE_TOOLS=$(VICE_BASE)

CC65BASE=C:/Users/usucuha/retrocomputing/C64/cc65

CC=$(CC65BASE)/bin/cc65
CA=$(CC65BASE)/bin/ca65
LD=$(CC65BASE)/bin/ld65
CO=$(CC65BASE)/bin/co65

MODULES=rom.o

CURRDIR=$(shell pwd)

all: rom.bin

rom.bin: $(MODULES)
	$(LD) -o rom.bin -t $(TARGET_PLATFORM) $(MODULES)

#tribal.o: tribal.c
#	$(CC) -O -t $(TARGET_PLATFORM) tribal.c
#	$(CA) tribal.s

rom.o: rom.s
	$(CA) -t $(TARGET_PLATFORM) -o rom.o rom.s

clean:
	rm -f *.o
	rm -f rom.bin
#	rm -f c64life.d64

#mkdisk: c64life
#	rm -f c64life.d64
#	$(VICE_TOOLS)/c1541 -format C64Life,1 d64 c64life.d64 8 -write c64life

#run: mkdisk
#	$(VICE_TARGET) $(CURRDIR)/c64life.d64
