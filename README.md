# Pasta / Tricolore

**Pasta** is a typeless retroprogramming language.

Pasta is simple but substantial:
- Syntax is highly consistent, so code is parsed without any backtracking
  - All expressions are prefixed, including operations: `+ 1 2`
  - Literal types: number, string, variable
  - Bracket types: `{ block }`, `( sub-expression )`
- All core functionality is provided through common primitive functions, e.g.:
  taking arguments, loops, conditionals
- Code is compiled on-the-fly into Pasta machine code, consisting of just 4
  instructions:
  - PUSH a value on the stack
  - REF a variable and push its value
  - SKIP n bytes, but push the starting position
  - EVAL the n arguments on the stack

Pasta is nostalgic, and highly tangible:
- 8/16-bit Pasta directly accesses 64k of sequential memory
- This includes machine registers and interpreter code and state
- Data abstractions are deliberately thin, but practical
- Poking and peeking is encouraged
- You can't really break stuff (that can't be fixed by a 'reset')

**Tricolore** is a Pasta-based fantasy console specification.

Tricolore extends the virtual Pasta machine with display hardware, sporting up
to 16 layers of tile/sprite/character maps, each with its own palette of 4
colors (or 3 colors plus transparency) out of a system wide 16-color pallete.


## Status
With experience from similar recipes, Pasta was cooked up on a whim, but is
already starting to feel close to _al dente_, especially when considering that
its quirky 80's vibe is a feature, not a bug.

Work on Tricolore has only just started.

In both subsystems, memory for all of the different features and registers has
been allocated haphazardly, and is in need of being organized.

## Building, running and examples
For the build to succeed, in addition to a regular C compiler, you need the GTK
headers and libs. On a Mac install homebrew, and then run
`brew install gtk+ pkg-config`.

Next, typing 'make' should produce 2 local executables:
- `pasta` is the core interpreter
- `tricolore` adds a screen, as well as the relevant primitives

Both accept file arguments. Examples for various features are found under
[recipes](recipes/). Add a final '-' argument to enter the REPL after running a
file.

