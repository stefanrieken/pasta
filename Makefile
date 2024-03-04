CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections
OBJS=stack.o base.o int.o selfless.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o selfless

clean:
	rm -rf *.o selfless
