# Pasta / Tricolore

**Pasta** is a typeless retroprogramming language.

Pasta is simple but substantial:
- Syntax is highly consistent, so code is parsed without any backtracking
  - All expressions are prefixed, including operations: `+ 1 2`
  - Literal types are: number (`42`), string (`"foo"`), variable reference (`foo`)
  - Bracket types are: `{ block }`, `( sub-expression )`
- No special syntax is required for primitive core features like loops,
  conditionals, or function definitions.
- Code is compiled on-the-fly into Pasta machine code, consisting of just 4
  instructions:
  - PUSH a value on the stack
  - REF a variable and push its value
  - SKIP n bytes, but push the starting position
  - EVAL the n arguments on the stack

Pasta is nostalgic, and highly tangible:
- 8/16-bit Pasta directly accesses 64k of sequential memory
- This includes code and (some) interpreter 'machine state'
- Data abstractions are deliberately made thin, but practical
- Peeking and poking around is encouraged
- You can't really break stuff ...that can't be fixed by a 'reset'

**Tricolore** is a Pasta-based fantasy console specification.

Tricolore is 80's chic:
- Tricolore extends the virtual Pasta machine with equally virtual display hardware
- Sporting up to 16 layers of tile/sprite/character maps,
- Each with its own palette of 4 colors (or 3 colors plus transparency) out of a system wide 16-color pallete.

## Building, running and examples
Plain Pasta should build on a regular (Posix) C compiler.

For the Tricolore build to succeed, you also need the SDL2 headers and libs.
The legacy GTK based build needs GTK+3 instead, and lacks audio support.
On a Mac using homebrew, run `brew install pkg-config sdl2 gtk+3`.

Next, typing 'make' should produce 2 local executables:
- `pasta` is the core interpreter
- `tricolore` adds a screen, as well as many relevant primitives

Both accept file arguments. Examples for various features are found under
[recipes](recipes/). Add a final '-' argument to enter the REPL after running a
file. And if a script features a function named `run`, this function will
automatically execute, unless a '-' argument is specified.

The desktop version of Tricolore features mouse input, and a 'hires' mode of
256x256 pixels / 32x32 characters (as opposed to 128x128 / 16x16).

## For the Thumby Color
Tricolore can be built to run on the Thumby Color. It requires a copy of the
Pico SDK with the TinyUSB submodule to be placed alongside the Pasta folder.

Type `make rp2350` to make the executables. Presently the build involves
launching the desktop version to make a ram image; click away the Tricolore
screen to continue the build.

Copy the file `rp2350/tricolore.uf2` to your Thumby Color. You should be
greeted with a startup sound, followed by a running program.

You should now be able to contact your Thumby over serial. From there, press
enter to initiate the Pasta REPL (pausing the running program), where you can
also copy / paste any sample code.

## For the original Thumby
Pasta **Bianco** is a minimal serving whipped up for the original Thumby. Type
`make rp2040` to build this version, then copy `bianco.uf2` to your Thumby.

Being a minimalistic subset of Tricolore functionality, any `.bianco` scripts
can also be run in Tricolore.

## Status
Having had experience with similar recipes, Pasta was cooked up on a whim, but
is already starting to feel close to _al dente_, especially when considering
that its quirky 80's vibe is a feature, not a bug.

Tricolore perfectly matches both language and vibe. and with a growing number
of self-hosted utility programs, it is quickly coming around to being a viable
retroprogramming platform.
