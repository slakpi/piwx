CC=gcc
CFLAGS=-O3 -Wall -g -c
LDFLAGS=
LIBS=-lcurl -lxml2
OBJ=piwx.o

%.o: %.c
	@echo Compiling $<...
	@$(CC) $(CFLAGS) -o $@ $<

piwx: $(OBJ)
	@echo Linking $@...
	@$(CC) -o $@ $< $(LDFLAGS) $(LIBS)

all: piwx

clean:
	@echo Cleaning up...
	@rm -f piwx $(OBJ)
