NAME = parser
IDIR =./
LIBS = -ljpeg
CC=g++
FLAGS = -Wall -Wextra
CFLAGS=$(FLAGS) $(LIBS)
SRC = parser.c

all :
	$(MAKE) $(NAME)

$(NAME) :
	$(CC) -o $(NAME) $(SRC) $(CFLAGS)

.PHONY: clean

clean : 
	rm -f $(NAME)

	
