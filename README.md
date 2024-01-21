# Selfless: Typeless Selfish
Selfless is a retroprogramming language, sprinkling a few Selfish novelties
over an otherwise quite achaic LISP dialect.

It runs by the _very_ modest paradigm that "everything is an integer".

"Abstractions are there so you can't break stuff. This has two downsides: 1)
abstractions are there; and 2) you can't break stuff."

## Current state
...is 'work in progress'.

Upon execution, known strings and variables are listed. Most of the variables
presently represent primitive functions (variables are untyped, so the system
cannot supply more information than just their number).

Here are some examples of expressions you can type:

        hi               --> you are greeted from a primitive function
        hi print         --> you get the integer (pointer) value of 'hi'
        hi 42 print      --> print variable number of args
        (1 2 +) print    --> nested expression support
        42               --> will try to evaluate value as function(!)
        {42 print} 1 if  --> conditionaly execute block
        {42 print} print --> print the address of block (within buffer)
        42 "x" define    --> define variable x

This demonstrates the preliminary framework for variables, (primitive)
functions, and conditionals, as well as their compilation into bytecode and
subsequent evaluation. It also demonstrates the quirk that, without types, it
is easy to cause values to be interpreted as function references.

You can also define a function:

        ({ ("x" args) (ls) } "foo" define) (42 foo)

For now, this has to be a one-liner, as the function is never copied into main
memory from the parse buffer, so it is overwritten by the very next read. Next,
the interpreter half chokes on the perceived single top-level expression -- so
yeah, here is still work to do.

What follows below is design, not current state.

## Simplifications
Respective to (modern) LISP, Selfless introduces many simplifications, aiming
at small-footprint execution:
- Consecutive addressable memory stores both compiled code and data
- All values are untyped words (i.e. integers or pointers)
- Single, shadowable, dynamic variable list
- Dynamic typing; no binding of lambdas
- No garbage collection; instead, direct memory access + 'compiled' code
- No macros; only compile time transformations
- No parsing into linked lists; instead use a parse buffer
- Zero read-ahead parsing (but a conversion to postfix follows)
- No 'special forms'; instead, explicitly use "strings", { blocks }
- Transform lambdas, blocks at compile time
- Also employ lambda syntax for 'let' scoping ({[x] (+ x x)} 21)

## Compiling
- Statements are first read and saved as prefixed, so that lenghts of (sub)-
statements can be predetermined when converting to postfixed.
- Encountering any opening bracket, we push a new statement counter: { type, location, count }
- Encountering any closing bracket, we replace the opening bracket with a type+size announcement
- For all statements and blocks that we want in-lined, we add sub size to parent size
- We also in-line blocks so that they remain part of the parent function

## Bracket counting
NOTE: Actually, we count (num args), resp {code length}.

        (define "foo" {
          lambda (x) (if x {print "yes"})
        })

After converting lambda into pop / undef var statements (NOTE: see "(Var)arg processing"):

        (define "foo" {
          (pop "x") (if x { (print "yes") }) (undef "x")
        })

Bracket counted (adding outer accolades):

        {17} (3) define "foo" {12} (2) pop "x" (3) if x {3} (2) print "yes" (2) undef "x"

## Postfixing
- Add skip count / method size {17} up front
- Back to front, add: (3) define, "foo", and the method:
- Add skip count {12} up front
- Front to back, add each statement:
- Back to front, add (2), pop, "x"
- Back to front, add (3), if, x, and the block:
- Add skip count {2} up front
- Back to front, add print, "yes"
- Back to front, add (2), undef, "x"

Result:

        {17} {12} "x" pop (2) {2} "yes" print x if (3) x undef (2) "foo" define (3)

Assuming a REPL situation, this is to be stored in temporal memory, after which
"define" may decide to copy the referenced method code over into main memory.
As this includes its block definitions, the copy is easy enough.

## Postfixed source code
It is possible, for instance during development, to parse and run postfixed
code immediately by keeping track of start-of-block brackets and skip block
code:

        { ["x"] ({ "yes" print } if x) } "foo" define

## Lambda syntax
LISP's lambda syntax implies runtime creation: "make a function by combining
these argnames with these expressions" (and in lexically scoped LISP, bind the
current environment).

In Selfless it is both easier and more evident if we make the syntax match the
fact that functions are created at compile time. That is to say, in Selfless a
function is block which at runtime takes arguments from the stack and names
them for the duration of the block.

At the end of a block, the interpreter already should reset the argstack to the
original position + a single return value; now it is also asked to reset the
variable list to the position before execution. As this can be applied to any
kind of block, this can be built into the interpreter.

This makes the initial act of taking stack arguments 'non-special', so it can
be supported through any (primitive) function call, or even sequence thereof:

        { "x" "y" args; "z" remainder; * x y } foo define;

## Virtual machine

### 4 core commands
- Push val: push a number value (integer, string reference, ...)
- Ref var: resolve var label (= string reference) to value at runtime
- Skip n: jump over in-line method or code block, saving its start on arg stack
- Eval n: call last pushed item as method, passing n-1 args

### Short / long versions
A byte-sized command leaves us 6 bits to encode the argument, or to indicate
that a (larger) value follows:

- nn xxxxxx = core command nn, arg xxxxxx (e.g. a value from 0-252);
- nn 111101 = byte arg follows
- nn 111110 = word arg follows (16-bit)
- nn 111111 = long arg follows (32-bit)

### Primitives
Native and primitive functions are differentiated by a starting marker. For a
primitive-referencing function, only a primitive number follows. This indirect
administration allows us to reference primitives just like any other function.
On startup, the interpreter not only adds these primitive stubs, but also names
them through variables.

### Variable and function resolution
For both variables and functions, we should be able to predict their location
at compile time: either they are arguments to the (nested) lambdas currently
being defined (this includes 'let' blocks), or they already exist globally.

In the latter case they can be fully resolved at compile time; but in the
former their location may differ between calls (especially when recursive), and
our best way to refer to them is as 'local var #n'; so by an offset relative to
the current start of local vars.

Notice that the code still needs to reference the variable slot and not just
insert its value. Also, fixing the slot does not stop functions from being
first-class citizens, as we can still pass the function pointer value around
and assign it other names.

For now, we just stick to the simpler method of runtime lookup by name, and
make that work first.

### 32-bit support
Although the envisioned environment for this language may be that of a home
computer style fantasy console, we must not forget its potential on embedded
systems. In this case, it would be weird to to artificially limit adressable
memory.

### Compiler, VM, REPL
The core virtual machine initializes memory, unique strings, and primitives
before reading in a program.

It may be made to read (and write) precompiled programs, but its first mode of
operation should be reading, (compiling,) and evaluating statements, either
interactively or from an input source (file or stdin).

In theory a standalone compiler can be introduced, but here the first obstacle
is that it must substitute the runtime unique string administration, which
would be initialized with the names of primitives. Being typeless code, it
would be challenging to find back any string references to reassign.

### (Tail) recursion
Having postfixed code, we should be able to make our own call-state stack
through which we can optimize for tail calls; on the other hand, piggybacking
on the C stack may (temporarily) save us from deciding where and how to store
this administration (in target memory, or outside?)

Overall, we should recognize the rabbit hole that is wanting to write a "home
computer assembly level" language in a "home computer assembly level" language
-- at least before we can prove it to be simple enough by implementing it in a
more high-level environment.

Using target memory for processor data (like statement evaluation memory, or
the variety of proposed stacks) must be regarded the gateway into his rabbit
hole: on one hand it seems to make life easier, on the other hand it perhaps
unneccessarily complicates the target environment.

