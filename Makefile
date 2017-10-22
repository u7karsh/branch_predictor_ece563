CC = gcc
OPT = -O3
#OPT = -g
WARN = -Wall
CFLAGS = $(OPT) $(WARN) $(INC) $(LIB)

# List all your .cc files here (source files, excluding header files)
SIM_SRC = main.c bp.c

# List corresponding compiled object files here (.o files)
SIM_OBJ = main.o bp.o
 
#################################

# default rule
all: sim
	@echo "my work is done here..."


# rule for making sim
sim: $(SIM_OBJ)
	$(CC) -o sim $(CFLAGS) $(SIM_OBJ) -lm
	@echo "-----------DONE WITH SIM -----------"


%.o:
	$(CC) $(CFLAGS) -c $*.c


clean:
	rm -f *.o sim


clobber:
	rm -f *.o

