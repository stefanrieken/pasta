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

## For the rp2040 / Thumby
Plain Pasta should work on the rp2040 once a build target is set up.

Tricolore may be a sweet match for TinyCircuit's Thumby Color, but a port is
not presently started.

Instead, Pasta **Bianco** was whipped up to serve the original Thumby. It
requires a copy of the Pico SDK with the TinyUSB submodule to be found next to
the Pasta folder.

Type 'make rp2040' to make the executables, then copy 'bianco.uf2' to your
Thumby. Alongside a 'ready' message on the Thumby screen, You should now be
able to contact your Thumby over serial. From there, press enter to initiate
the Pasta REPL, where you can also copy / paste any relevant samples.

## Status
Having had experience with similar recipes, Pasta was cooked up on a whim, but
is already starting to feel close to _al dente_, especially when considering
that its quirky 80's vibe is a feature, not a bug.

Tricolore perfectly matches both language and vibe. and with a growing number
of self-hosted utility programs, it is quickly coming around to being a viable
retroprogramming platform.

## Building, running and examples
For the Tricolore build to succeed, in addition to a regular C compiler, you
need the GTK headers and libs. On a Mac using homebrew, run
`brew install gtk+ pkg-config`.

Next, typing 'make' should produce 2 local executables:
- `pasta` is the core interpreter
- `tricolore` adds a screen, as well as many relevant primitives

Both accept file arguments. Examples for various features are found under
[recipes](recipes/). Add a final '-' argument to enter the REPL after running a
file.

