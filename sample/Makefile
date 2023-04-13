CXX=g++
CXXFLAGS=-Wall -Werror
PRJ=heap_overflow_from_file
OBJ=${PRJ}.o

${PRJ}: ${OBJ}

all: ${PRJ}

clean:
	${RM} ${OBJ}

fclean: clean
	${RM} ${PRJ}

.PHONY: all clean fclean

%: %.cc
	${LINK.cc} $< -o $@
