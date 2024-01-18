# Selfless: Typeless Selfish
Selfless is a retroprogramming language, sprinkling a few Selfish novelties
over an otherwise quite achaic LISP dialect.

## Current state
...is 'work in progress'.

Upon execution, known strings and variables are listed. Of these variables,
'+', 'hi' and 'print' indicate primitive functions (variables are untyped, so
the system cannot supply more information than just their number).

Here are some examples of expressions you can type:

        hi              --> no result (you just referenced a variable)
        (hi)            --> you are greeted from a primitive function
        (hi print)      --> you get the integer value of 'hi'
        (hi 42 print)   --> print variable number of args
        ((1 2 +) print) --> nested expression support

This demonstrates the preliminary framework for variables, (primitive)
functions and their evaluation.

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

## (Var)arg processing
In the preceding we used explicit argument popping and scoping, but failed to
address that the first argument would be an argument count, let alone use this
to process varargs, or simply missing args.

A better shortcut would be that any method (or block!) must open with an in-line
array of arg names, for the interpreter to recognize:

        {13} [1] x ...

A negative value (or similar bit manipulation) can be used to imply a remainder
argname, which would require some array handling methods to use. Notice that if
we would ever support in-linable user arrays in general, it would have to be
done exactly like blocks, as we cannot distinguish the two by type.

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

