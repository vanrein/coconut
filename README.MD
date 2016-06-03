# Nuts and Bolts for Coroutine Communication (COCONUT)

> *Coroutines allow a programmer to program loops en tests in the most natural
> flow, and connect them through a portable form of "legal abuse" of the C and
> C++ standards.  Coconut extends these facilities with some nuts and bolts
> for communication between processes, namely a sort of unbuffered pipeline.
> It is possible to use these in a RendezVous-style, as defined in Occam and
> Ada.*

**Note:** This work has not been finished; it appears that I was reinventing
much of the work in Go; and it is probably a good design choice to not cram
everything into C syntax (even though it is very interesting to see how it
can be done).  At the moment, I see this publication as code / document of
historic interest.

## Introduction to Coroutines

Coroutines are an impressive
[invention by Simon Tatham](http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html)
inspired on
[Duff's device](http://www.lysator.liu.se/c/duffs-device.html).
Simon uses this concept in his well-known
[PuTTY](http://www.chiark.greenend.org.uk/~sgtatham/putty/)
program, as well as his
[Spigot](http://tartarus.org/~simon-git/gitweb/?p=spigot.git;a=blob;f=trig.cpp)
as-precise-as-you-like calculator.

A coroutine is a function with a body like

    fun() {
        crBegin;
        loop () {
            if () {
                crReturn (0);
            }
            crReturn (1);
        }
        crReturn (2);
        crEnd;
    }

The calls to `crReturn()` return the given value.  Next time a call to `fun()` is
made, it continues after that point.

The advantage of a coroutine is that a program flow can be programmed without being
driven by the need to communicate intermediate results to the outside world.  This
makes routines less dependent on each other, and this can help to simplify code,
making it both smaller and more secure.

The technique has been put to use in an extremely small, portable implementation
of a complete
[TCP/IP stack](http://citeseerx.ist.psu.edu/viewdoc/download?rep=rep1&type=pdf&doi=10.1.1.154.2510)
by Adam Dunkels.

## Pipe Nuts for Coroutines

A "pipe nut" is something you add to the end of a pipe in plumbing.  The term applies
well to the communication structure that we define for coroutines; we therefore
picked the term *Coconut* for the pipe endpoints defined below.

All coconut functions must be called between a begin and end marker:

    cobegin ();
    coend ();

Two easy to understand coconut functions are

    reallen = cowrite (pipe, buf, len);   // in a first coroutine
    reallen = coread  (pipe, buf, len);   // in another coroutine

These read or write `len` bytes of `buf` from or to the given `pipe`.  The task
of the pipe is to bring two such requests together and passing the information
between them.  Assuming that `pipe` is always used for the same sizes on both
ends, the two coroutines perform the communication as though in lockstep.  This
is what is called a Rendez-Vous in Ada and Occam, and it is an easy to use
form of synchronisation.

The form that enforces a minimum length will return `-EPIPE` when less than
`minlen` bytes are available at the end of a communication; only the end of file
will return 0 instead of the error condition.

    reallen = cowritem (pipe, buf, minlen, maxlen);
    reallen = coreadm  (pipe, buf, minlen, maxlen);

Normally, these calls return `reallen` equal to `len`, with the exception of
the end of a pipe communication.  As with POSIX' `read()` it is possible to
receive length 0 as an end-of-file indication.  In addition to POSIX' `write()`
semantics, the sending side can write 0 bytes as well.

It is however possible to continue communication after having sent or receieved
0 bytes, possibly even with `buf` set to `NULL`.  This pattern is used in a
few macros that just pass on synchronisation signals

    copush (pipe);   // in a first coroutine
    copull (pipe);   // in another coroutine

The two operations meet in a Rendez-Vous fashion, exchanging 0 bytes.  The
statements represent the opposite sides of the synchronisation, but are
otherwise compatible.


## Exception handling

Exceptions can be picked up elswhere in the code with a special kind of label:

    cocatch (exc):
        // Process errno

It is possible to raise exceptions in the program as well, namely with:

    coraise (exc);

There are a few special exception-raising events, namely for end-of-file
and for `errno` return values:

    coraise_errno (exc);                    // if errno != 0
    coraise_eof   (exc, reallen);           // if reallen == 0
    coraise_if    (exc, reallen == -EPIPE); // if condition matches
    coraise_null  (exc, ptr);               // if ptr == NULL
    coraise_neg   (exc, val);               // if val < 0
    coraise_min1  (exc, val);               // if val == -1
    coraise_ckrv  (exc, rv);                // if rv != CKR_OK (for PKCS #11)


## Resource Allocation Is Initialisation

A wonderful style of programming in C++ is
[RAII](https://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization)
whose
[counterpart in C](https://vilimpoc.org/research/raii-in-c/)
is not perfect.
Techniques similar to the ones presented here can help out.

After `cobegin()`, we declare a number of resource identifiers for a routine
(taking note of the braces):

    coresources { ABBA, DEEP_PURPLE, KISS };

Having declared these, we can now add and remove these resources in a TODO-list:

    cocleantodo (ABBA);
    cocleandone (ABBA);

Especially `cocleandone()` is useful, as it can be invoked after `cocleantodo()`
when cleanup of the resource is to be avoided, such as in the case of successful
termination of a coroutine's task.

The invocation of the cleanup routines is made with `cocleanup ()` and the
various resources can be cleaned up in a fashion similar to exception handling:

    cocleaner (ABBA):
        // Cleanup code

**TODO:** Move to implementation file.

It is even possible to insert cleanup code on the spot:

    #define cocleaner(R,C) if(1) cocleantodo(R); else { cocleandone(R);case _cleanup_ ## R: C; _cocase = _cleanup; goto _coloop; }
    #define cocleantodo(R) (_cocleanup |= 1 << R)
    #define cocleandone(R) (_cocleanup &= ~ (1 << R))

somewhere in the loop there should be a cleanup target

    _cleanup: if (_cocleanup != 0) { ... _cocase = _cleanup_ ## WHAT; goto _coloop; }

This may be good to insert into `coresrouces()`.

**Problems:** Bit masks are integers, not symbols.  Of course `switch()` can handle integers too.

**Thoughts:** Maybe we can define a resource number and shift it up while adding resources, and shift it down while cleaning up.  But how then to catch the right cleaning actions?

## Multiple Producers and Consumers

Pipes couple producers and consumers, and it is even possible to have multiple
producers (writers) as well as multiple consumers (readers).  If this is done,
then the end-of-file marker is the point where they may change, so not every
individual exchange.

Waiting in line for access to a pipe is just one more form of synchronisation
between coroutines, and may be put to any use deemed worthy.  One interesting
use may be to have one party submitting fragments of work, or perhaps even
just doing `copush()` on a pipe and having multiple readers or `copull()`
listeners.

**TODO**: Move the following to an implementation document.

To be able to do this, the coconut function `cobegin ()` declares a constant
that it retrieves once during startup:

    static const int _cobegin_identity = _cobegin ();

This value is passed along with the coconut functions, and serves to decide
whether the coroutine may work with the pipe, or should first await its turn.
The value is also entered in the main loop.


## Ready-to-run Coroutines

Coroutines may be ready-to-run or not.  When they are, they will be on a queue
that is run by a major scheduler:

    errcode = comainloop ();

This call terminates only when all coroutines have ended, or when a deadlock
occurs.  Check `-errcode` for a value from `<errno.h>` to learn about any
problems.


## Integration with event handling

It is highly interesting to consider the integration of these mechanisms with
event handler loops.  At present, this has not been established.

What is probably needed is an indication of things on which a coroutine is
waiting before it can proceed.  The event handler code can then spark the
coroutine back to life when events poor in.


## Integration with threading

It is highly interesting to consider the integration of these mechanisms with
threading, especially when coroutines may be spread between threads and when
the pipes become synchronisation points.  Ideally, this would be based on an
[atomic operation](https://apr.apache.org/docs/apr/1.5/group__apr__atomic.html)
or a
[spinlock](http://www.alexonlinux.com/pthread-mutex-vs-pthread-spinlock).

The clearly defined API for coconut functions will be helpful in this respect.
What also helps, is that coconut functions continue to serve a given coroutine
until it reaches the end-of-file.


