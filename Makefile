CFLAGS=-Wall -Wunused -ffunction-sections -fdata-sections -DANSI_TERM -DLEXICAL_SCOPING
OBJS=src/stack.o src/base.o src/int.o src/pasta.o src/terminal.o src/file.o

EMBEDDEDS=hello.ram.obj assets/ascii1.bmp.obj assets/ascii2.bmp.obj recipes/lib.pasta.obj recipes/lib.trico.obj

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ `pkg-config --cflags cairo gdk-3.0 gtk+-3.0 libpng`

all: pasta tricolore

# This target demonstrates how to script generating a Pasta binary
# (for now, manually close the Tricolore screen to continue to saving)
hello.ram: tricolore recipes/hello.trico
	echo 'save "hello.ram"' | ./tricolore recipes/hello.trico -

## Demonstration of how to make an (initial) empty ram file
lib.ram:
	dd if=/dev/zero of=lib.ram bs=1 count=32

pasta: $(OBJS) src/main.o
	$(CC) $(CFLAGS) $^ -o $@

tricolore: $(OBJS) src/bitmap.o src/tricolore.o src/ports/gtk_cairo.o src/ports/sdl.o
	$(CC) $(CFLAGS) $^ -o $@ `pkg-config --libs cairo gdk-3.0 gtk+-3.0 libpng` -lSDL2

# Use e.g. objdump -t assets/ascii1.bmp.obj
# to see the symbols ld has setup for each binary
%.obj: %
	arm-none-eabi-ld -r -o $@ --format=binary $<

rp2040: rp2040/Makefile src/*.c src/ports/thumby.c $(EMBEDDEDS)
	cd rp2040 && make bianco

rp2040/Makefile: CMakeLists.txt
	mkdir -p rp2040
	cd rp2040 && cmake .. -DPICO_PLATFORM=rp2040 -DPICOTOOL_FETCH_FROM_GIT_PATH=$(realpath ../picotool)


rp2350: rp2350/Makefile $(OBJS) src/*.c src/ports/thumby_color.c $(EMBEDDEDS)
	cd rp2350 && make tricolore

rp2350/Makefile: CMakeLists.txt
	mkdir -p rp2350
	cd rp2350 && cmake .. -DPICO_PLATFORM=rp2350 -DPICOTOOL_FETCH_FROM_GIT_PATH=$(realpath ../picotool)

clean:
	rm -rf src/*.o src/ports/*.o pasta tricolore rp2040 rp2350 assets/*.obj recipes/*.obj
