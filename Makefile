CC=gcc
LIBS=-lcurl -lxml2 -lpng -ljansson
OBJ=piwx.o wx.o gfx.o

%.o: %.c
	@echo Compiling $<...
	@$(CC) $(CFLAGS) -o $@ $<

piwx: $(OBJ)
	@echo Linking $@...
	@$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

all: piwx

clean:
	@echo Cleaning up...
	@rm -f piwx $(OBJ)
