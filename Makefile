OBJS = main.o
PROG = textrpg

all: $(PROG)
$(PROG): $(OBJS) ; $(CC) $(LDFLAGS) $(OBJS) -o $@
clean: ; rm -f $(OBJS) $(PROG)
