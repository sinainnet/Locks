CC      := g++
CCFLAGS := -I
LDFLAGS := -lrt -lpthread -ansi

TARGETS:= main reader
MAINS  := $(addsuffix .o, $(TARGETS) )
OBJ    := main.o reader.o
OBJ2   := 
DEPS   := ./rwlocks.h ./excllocks.h ./os.h ./header.h

BUILDPATH	:= ./build

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJ2)

recompile:	clean all

$(OBJ): %.o : %.cc $(DEPS)
	$(CC) -g -c -o $@ $< $(CCFLAGS) $(BUILDPATH)

$(TARGETS): % : $(filter-out $(MAINS), $(OBJ)) %.o
	$(CC) -g -o $@ $(LIBS) $^ $(CCFLAGS) $(BUILDPATH) $(LDFLAGS)
	echo Build Done

