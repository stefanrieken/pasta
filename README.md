# Selfless
Selfless is a typeless retroprogramming language, sprinkling Selfish style
elements over what would otherwise be a quite achaic LISP dialect.

It runs by the _very_ modest paradigm that "everything is an integer".

"Abstractions are there so you can't break stuff. This has two downsides: 1)
abstractions are there; and 2) you can't break stuff."

## Current state
...is 'proof of concept'.

Upon execution, known strings and variables are listed. Most of the variables
presently represent primitive functions; as variables are untyped, the system
cannot supply more information beyond their numeric value.

Here are some examples of expressions you can type:

        hi               --> you are greeted from a primitive function
        print hi         --> you get the integer (pointer) value of 'hi'
        print hi 42      --> print variable number of args
        print (+ 1 2)    --> nested expression support
        42               --> will try to evaluate value as function(!)
        if 1 {print 42}  --> conditionaly execute block
        print {print 42} --> print the address of block (within buffer)
        define "x" 42    --> define variable x

This demonstrates the preliminary framework for variables, primitives, and
conditionals, as well as their compilation into bytecode and subsequent
evaluation. It also demonstrates the quirk that, without types, it is easy to
confuse values and function references.

## Native function definition
Native functions are code blocks that optionally also take arguments:

        define "foo" {
          args "x";
          ls
        }
        foo 42
        ls

This demonstrates how 'x' is defined within the function only.

It is even possible to run an anonymous variant:

        { args "x"; ls } 42; ls

## Simplifications
Respective to (modern) LISP, Selfless introduces many simplifications, aiming
at small-footprint execution:
- All values are untyped word values (i.e. integers or pointers)
- Single, shadowable, dynamic variable list (= dynamic scoping)
- No lexical scoping means no binding of environments to lambdas required
- No garbage collection; instead, directly access all memory
- No macros or parsing into linked lists; instead use 1-on-1 compilation
- Zero backtracking during parsing
- No 'special forms'; instead, explicitly use "strings", { blocks }
- Static block definition; no runtime transformation of data into code
- Also employ function syntax for 'let' scoping: ({args "x" (+ x x)} 21)

## Compiling
Code is compiled into a 4-command bytecode:
- PUSH adds the argument integer value to the argument stack. The value may
  represent either an integer literal from source code or a reference to a
  string literal.
- REF looks up the argument integer value in the list of variables. The value
  is a string reference representing the variable name.
- SKIP marks the start of an inline block. The PC is set forward by the given
  amount, and the old PC value is pushed on the argument stack.
- EVAL indicates the number of values that should now be on the argument stack
  and that together form the next evaluable expression.

The argument value to each instruction is either contained in the remainder of
the instruction word, or for larger values it encompasses a subsequent word:
- nn xxxxxx = core command nn, arg xxxxxx (e.g. a value from 0-252);
- nn 111101 = byte arg follows
- nn 111110 = word arg follows (16-bit)
- nn 111111 = long arg follows (32-bit)

## Fixing it in Post
The interpreter evaluates input stream items successively, whether they are
function references, inline definitions, or function arguments. The closing of
an expression is marked by an 'EVAL n' instruction. This way, code is easily
parsed in the order in which it is written (prefixed), but still essentially
evaluated in a postfixed fashion.

One consequence of this (semi)postfixed evaluation is that all arguments are
already evaluated by the time we get to evaluate their enclosing expression,
and so lazy evaluation can only be performed by explicitly defining lazy
arguments as blocks (or by having the compiler make this translation under
water).

(One envisioned workaround is to split the arguments to EVAL into two numbers,
the second one marking further arguments still to follow in the input stream,
which may be skippable, similar to current blocks. The origins of this truly
'mixed-fixed' approach to conditional evaluation lie with the Forth programming
language, which is considered to be famously postfixed -- go figure.)

## Argument syntax
LISP's lambda syntax is aimed at runtime function creation; in Selfless it
is both easier and more evident to make the syntax match the fact that the
function already exists at compile time, and the only runtime job left is to
take arguments from the call stack, and name them. This looks as follows:

        define "mul" { args "x" "y"; * x y }

At the end of any block, the interpreter resets the variable list to the
position before execution, invalidating all arguments.

## Primitives
Native and primitive functions are differentiated by a starting marker. For a
primitive-referencing function, only a primitive number follows. This indirect
administration allows us to reference primitives just like any other function.
On startup, the interpreter not only adds these primitive stubs, but also names
them through variables.

Presently all primitives are switched over in one big case statement. In the
future we may want to allow at least one more user-definable callback to allow
expansion of the language when linked into other projects as a library.

### 32-bit support
Although the envisioned environment for this language may be that of a home
computer style fantasy console, we must not forget its potential on embedded
systems. In this case, it would be weird to to artificially limit adressable
memory.

### (Tail) recursion
...is not implemented yet, as in this stadium we once more rely on the C stack
for nested invocations.
