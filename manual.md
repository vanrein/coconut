# Coconut Programmer's Manual

> *Coconut is "Coroutines Communication through Pipents".  It is a set of
> primitives for programming in C or C++ that greatly enhance the flexibility
> of certain programs.  They are very suitable for the implementation of
> complex network interactions.*

## Introduction

The C and C++ programming languages are normally used by us in a highly structured
manner.  The structure helps us to control our understanding of the program, and
thus to evade bugs.

The language also has a few dark corners, that we may or may not use at some times.
What Coconut does is based on such dark corners, by turning them into programming
language concepts of an arguably higher level, which effectively raise the level
at which we use those dark corners, so they are more suppotive of our understanding
of the language.

This also means that the concepts' implementations are sometimes very ugly.  They
are perfect standard C, making heavy use of clever compiler optimisation, but
they are in the end rather ugly constructs based in part on macros that will
upset you.  Please keep in mind that the idea of Coconut is to contain the
uglyness inside those macros and present the programmer with higher-level concepts.
We have done our best to get rid of the side-effects of macros, and this may
lead to very ugly macros.

An example is `copoll (evt) { statements... }` which specifies a handler for the
event `evt`.  These events may be triggered by external C code, including
coroutines, and whenever the coroutine holding this declaration runs, it will
trigger the `statements...`; when these are through, control returns to the
event handler and when the last has been processed, the coroutine yields to
other coroutines.  Events without a handler will be silently ignored.

The macro defining this is horrible, but seeing what useful concept it builds,
we think it is a useful addition to the programmer's pallette.  The code doing
this is

    #define copoll(e) if (0) while (1) if (1) goto _coprocess; else case _colabel(evt):

You would never write down this code, because it is unreadable.  But given that
it is encapsulated in a macro, you might consider it readable enough for your
programs.  Here is how it works:

  * The `if (0)` causes this code to be skipped when the program flow ends up
    at the `copoll()` declaration.  This helps you to place this code wherever
    you feel comfortable with it.  Your compiler is quite likely to optimise in
    the trivial manner.
  * The `while (1)` assures that the `statements...`, when they are done, return
    control to the macro.  It is a trick to add some code after what C thinks
    is a statement -- be it a single or compound statement.  Trust your compiler
    to not make a fuss over condition evaluation, and optimise it out.
  * The `if (1)` means that the `else` branch is not executed when looping back
    into this code.  Again, your compiler will optimise out the condition.
  * The `goto _coprocess` jumps back to the event handler loop; using `goto` is
    ill-advised, but programming constructs such as `while` are also made from it
    and new structuring concepts may use it too, provided that it is not as
    unbounded as `goto` itself.
  * The `_colabel(evt)` is a constant expression based on the constant value `evt`
    which was defined in an `enum` declaration.  The integer value of this constant
    expression is computed at compile-time.
  * Finally, `else case _colabel(evt):` is a branch that normally does not run,
    but the `case` label is part of an encapsulating `switch()` that controls
    much of the control logic of Coconut; the dark corner of C used here is the
    portable (and formally standardised) ability to jump into inner structures
    using `switch()`, causing control to end up inside the `if (0)`, and the
    `else` side of `if (1)`.

Much of this is thinking at an assembler language, except of course that it is
portable C.  The one thing to keep in mind is a simple rule:

> *Avoid using `switch() case:` in a coroutine.*


## History and Related Concepts

The dark corner of C that is exploited by Coconut was a discovery by Tom Duff,
named [Duff's Device](http://www.lysator.liu.se/c/duffs-device.html)
which is a copying routine with a partially unrolled loop; the original
routine would tick-tock between a singly word copy and a conditional jump;
the optimised routine does 8 copies between conditional jumps.  The remaining
problem was to deal with up to 7 extra copies, which Tom cleverly solved by
jumping into the loop to just the right point:

    send(to, from, count)
    register short *to, *from;
    register count;
    {
            register n=(count+7)/8;
            switch(count%8){
            case 0: do { *to = *from++;
            case 7:      *to = *from++;
            case 6:      *to = *from++;
            case 5:      *to = *from++;
            case 4:      *to = *from++;
            case 3:      *to = *from++;
            case 2:      *to = *from++;
            case 1:      *to = *from++;
                    } while (--n > 0);
            }
    }

Based on this, Simon Tatham famously invented the first useful concept, namely
[coroutines in C](http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html)
which allowed him to program a number of independent program flows in
"coroutines" that collaborate, but not necessarily by nesting function calls.
Coroutines are a well-known concept in various other languages; they are a bit
like multithreaded programming, but with one flow of control, and thus one stack.
Simon uses this concept in his widely appreciated
[PuTTY client for OpenSSH](http://www.chiark.greenend.org.uk/~sgtatham/putty/),
as wel as for "lazy evaluation" in his
[Spigot arbitrary-precision calculator](http://tartarus.org/~simon-git/gitweb/?p=spigot.git;a=blob;f=trig.cpp)
so it is safe to say that the concept is stable, portable and therefore safe to use.

This concept lends itself wondefully for things like network stacks, where layers
of the network stack each have their own logical control flow, but nesting of
programs entangles the programmer into much more complexity than just that flow.
This was taken to an extreme form by Adam Dunkels, who created the
[uIP TCP/IP stack](http://github.com/adamdunkels/uip)
which is a
[full stack for 8-bit machines](http://dunkels.com/adam/mobisys2003.pdf).
In tens of kB he achieved a truely marvelous for the most constrained environments,
leaning heavily on coroutines.

There are similar traces in traditional C and C++ programming as well:

  * An [iterator](https://en.wikipedia.org/wiki/Iterator)
    which enables the programmer to facilitate his program with the outcome of a
    flow of control devised elsewhere.
  * The [callback](https://en.wikipedia.org/wiki/Callback_(computer_programming))
    concept is often used to provide a programmer's own plugins into a generic
    piece of software.
  * Using [threads](https://en.wikipedia.org/wiki/Thread_(computing))
    can free a programmer from unfolding all possible flows of control, though
    it comes at the expense of multiple stacks and the potential of
    [race conditions](https://en.wikipedia.org/wiki/Race_condition);
    when not used to improve efficiency through
    [concurrent processing](https://en.wikipedia.org/wiki/Concurrency_(computer_science))
    it may be advantageous to consider coroutines instead.
  * The [cleanup goto](https://vilimpoc.org/research/raii-in-c/) is the one
    situation in which most programmers would agree that the `goto` is helpful,
    to declutter code.
  * The [state machine](http://johnsantic.com/comp/state.html) is another
    programming concept that is often undesirable, but it does have its uses,
    for instance in parsers or networking code; these are situations in which
    gradually consumed information determines the continuation of the program
    flow, and where the logic does not necessarily follow a hierarchical
    structure.  Take a look at the
    [TCP state diagram](https://tools.ietf.org/html/rfc793#page-23) for an
    excellent example.

Other programming languages provide some exciting concepts that we wanted
to introduce into C with Coconut.

  * [Coroutines](https://en.wikipedia.org/wiki/Coroutine)
    are useful in Ada, and light-weight threads are common in languages such as
    Erlang and Occam.
  * [RAII](https://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization)
    is a very useful style of programming in C++.
  * [RendezVous](https://en.wikipedia.org/wiki/Barrier_(computer_science))
    is a synchronisation concept in Ada and Occam, where two program portions
    take a step in their execution in joint lockstep.  It is simple yet powerful,
    especially when implied by a communication primitive.

## Code Structuring Conventions

  * cobegin / coend ; declarations?
  * parameter naming, self
  * storing variables
  * allocating, destroying
  * running
  * scheduling?
  * avoid switch / case
  * include file; pthreads first

## Coroutines (coros)

The coroutine, henceforth abbreviated to **coro** with plural form coros, is a
thread of control that can be stopped to let another flow of control take over.
When a coroutine calls `coyield()`, it permits other coroutines to run, and when
control returns it will continue after the yielding statement.

Coroutines are implemented on one stack, which makes them different from threads.
It also makes them much easier to work with, in the sense that there is no danger
of other threads taking over at an uncontrolled point in time.  As a result,
far less attention for race condition and locking schemes is desired.  Do note
the downside -- coroutines may help the programmer to structure his program,
but they do not help to unleash the computational potential of a multi-core
processor.  Below we provide more detail on mixing pthreads with Coconut.

Coroutines do not return data to their callers; there merely run and if any data
is to be exchanged, then this is done over the Pipe Nuts presented below.

The API used inside of a coro is:

  * `cobegin();` marks the point where a coro switches to its last point of
    control.  This should be used at the beginning of a function that implements
    coro functionality.

  *  `coend();` marks the end of a coro; when control continues to this point,
     it will result in an implied version of `codone()`, specified below.

  * `coyield();` to return control from the coro to another, or more accurately
    said, to its caller that may have used `cogo()` to run the coro, or may be a
    scheduler doing this on coroutines in a cycle.

  * `codone();` is called when a coroutine is ready with its work.  It will
    cleanup any open resources and return control with the intention of the
    caller cleaning up the coro.

  * `cosub(c);` is used to call a coro `c` from within another.  When the subcoro
    invokes `coyield()`, it will also make the calling coro yield, and when the
    calling coro is given back control it will make the subcoro continue.

  * `coinitialiser stat` declares an intialisation statement `stat` for the
    coro; it is skipped when code execution hits upon it.  It is meant to be run
    only once for each coro.  It is possible, and indeed advisable for program
    clarity, to define `stat` as a compound statement.  When not declared, then
    no provisions will be made for program initialisation.
    TODO: place initialisation code between the C declaration and `cobegin()`.

  * `cofinaliser stat` declares code that should run when the coro runs
    `codone()` or when the flow of control reaches `coend()`.  It is meant to be
    run only once for each coro.  Resource cleanup is done after finalisation,
    as part of `codone()` or `coend()`.  It is possible, and indeed advisable for
    program clarity, to define `stat` as a compound statement.  When not declared,
    then no provisions will be made for program finalisation but resource cleanup
    still runs.

There is an additional API that aims for management of coro's from a "normal"
C programming context:

  * `coinit(TODO);` is used to instantiate a coro of the given TODO:class/function,
    and invoke its `coinitialiser`.

  * `conew(TODO)` allocates a new coro instance and runs `coinit(TODO)` on it.
    TODO: When no memory is available for the coro, a execution halts with an error
    message and `exit(1)`.

  * `codestroy(c);` is used to cleanup a coro, by invoking its finalisation code
    and cleaning up any open resources.  The coro should not yield but instead
    cause `codone()` or have the flow of control run into `coend()`.  Note that
    `codestroy()` could be called when the coro is in the middle of its flow;
    you should be careful to handle such cases properly in your `cofinaliser`,
    especially when resources are being exchanged between coros.

  * `cofree(c);` invokes `codestroy(c)` and frees the memory that was allocated
    by `conew()`.

  * `cogo(c);` is used to call a coro `c` from a "normal" C programming context.
    The coro will return a boolean value, namely 1 when it needs to run some
    more at a later time or 0 when it has completed execution.  Clearly, the
    return value 1 comes from `coyield()` and 0 comes from `codone()` or `coend()`.


## Resources

A resource is something that a program grabs holds of, with the intention of
freeing it sometime later.  Examples are allocated memory, file handles, but also
library object instances including coros.

In the course of a "normal" C function, resources are collected, and when an
exceptional situation occurs (such as a file that does not open), there is the
responsibility for the programmer to cleanup all resources collected up to the
point of failure, and return an error value.  Since program control splits,
this leads to much repeated code.

With Coconut's resource facility, resources can be collected freed much simpler.
For one, the code that frees a resource immediately follows the code that grabs
it.  A program would be structured as follows:

    resource = grabit ();
    ...check of resource is proper...
    cocleantodoaction (RESOURCE) {
            ....cleanup resource...
    }

The cleanup code is a declaration, and will be skipped, except that the resource
labelled as `RESOURCE` will be marked for future cleanup, for instance when an
exceptional situation occurs or simply when the coroutine terminates.

Resources are implemented as coro flag bits in a type that defaults to an
`unsigned long`.  This limits the number of resources available to a coro.

The following API is available for using resources within a coro:

  * `coresources { RES1, RES2, RES3 };` is an example declaration for resources
    internal to the containing coro.  The names `RES1`, `RES2` and `RES3` are
    defined as labels to refer to each resource (in fact through an `enum` type).

  * `cocleantodo(RES1);` is a statement that defines resource `RES1` as a future
    resource to clean.  This can be used after obtaining a resource.

  * `cocleandone(RES1);` is a statement that causes resource `RES1` to no longer be
     a future resource to clean.  This can be used in preparation of a success,
     where `RES1` is delivered to another part of the program and, along with it,
     the responsibility for its eventual cleanup.

  * `cocleanaction(RES1) stat` defines the cleanup action for resource `RES1`
    to be `stat`.  This must occur precisely once for every resource declared.
    The declaration itself is skipped when the program flow hits upon it, so it
    can occur anywhere, including right after a resource being claimed.  This
    should help the programmer with localisation of attention.  It is possible,
    and indeed advisable for program clarity, to define `stat` as a compound
    statement.

  * `cocleantodoaction(RES1) stat` combines `cocleantodo(RES1)` which marks
    the resource as a future cleanup responsibility, with the
    `cocleanupaction(RES1) stat` declaration for the cleanup itself.  The
    combination is a single statement to the C syntax.

  * `cocleanwhen(RES1);` is a manual invocation of the cleanup routine for `RES1`
    and implies `cocleandone(RES1);`.  This code is usable in explicit code to
    handle exceptional situations, which may be helpful to back up and try again,
    rather than giving up completely.

  * When the program flow hits upon `coend()` or explicitly runs `codone()`,
    then `cocleanwhen()` is run on all resources in use.  When a `cofinalizer`
    is available, it will be run prior to this cleanup of resources.

  * TODO: Explicit responsibility-takeover when resources are passed over pipe nuts.
    something like filling an offer with `cooffer()` and accepting it with
    `coaccept()` in the recipient, or possibly `coreject()`.  Implementation may
    be to defer cleanup, and/or to provide an invocation from the outside in the
    case of `coreject()`.  Note that any dependency on `coreject()` implies that
    we are dependent on communication transactionality.  Ideally, the only thing
    that is explicitly done is `coaccept()` which might clear the resource in the
    sender; this may even return whether it was already cleared (atomic needed)
    so the recipient can be certain about having access to the resource(s); this
    can be helpful with cleanup in the sending coro (which of course uses
    cocleanwhen, which might also have to be turned into an atomic operation if
    we do this).  It may in fact be simpler to rely on the clearly marked errors
    of conuts, which are observed on both ends, and process that accurately in
    order to come to a clear decision on who should set its local resource flag
    for the resource.


## Exceptions

An exception is raised when program flow hits upon an unexpected or undesired
situation, such as a failure to grab a resource.  When exceptions occur, it is
often instrumental to the readability of a program to transfer the flow to a
dedicated exception handler, especially when the same handler code can be used
for multiple situations, and/or when control can migrate from a specific handler
to more generic ones.

Because Coconut offers both resource management and exception handling, a very
attractive situation results, where exception handlers can cleanup any resources
collected prior to the occurrence of the exception.  Especially `cocleanwhen()`
is instrumental in keeping the exception handlers cautious, by not invoking the
cleanup code for any resources that have not been collected.  This can also help
to make exception handlers more general.

Exceptions are implemented with `goto` statements, which means that they are
very efficient.  In fact, they cut through the normal program flow, which is
precisely what is desired in exceptional situations.  What Coconut adds is a
notation of the concept, the sweetest syntactic sugar and useful continuation
after an exception handler in a predefined manner.

  * Declare exceptions with `coexceptions { EXC1, EXC2, EXC3 };` for use within
    the coro.  These are purely ornamental; at least in the implementation based
    on `goto`, we do not actually use these labels in the implementation of
    exceptions.

  * Use `cocatch (EXC1,EXC2) stat` to declare an exception handler for `EXC1` in
    statement `stat`.  When code execution hits upon this declaration it will
    pass through the code.  You are required to have an exception handler for
    every exception that you raise.  When done with `stat`, the handler will
    raise the additional exception `EXC2`.

  * Use `cocatch_done (EXC1) stat` as a variation on `cocatch()` which continues
    as `codone()` after `stat` completes.

  * Use `cocatch_continue (EXC1) stat` as a variation on `conut_catch()` which
    continues as `coprocess()`, the event handler loop, after `stat` completes.

  * Use `cocatch_fatal (EXC1) stat` as a variation on `cocatch()` which runs
    `exit(1)` to terminate the entire program, including all coros, after
    the handler code in `stat` completes.

  * Use `coraise (EXC1);` to raise exception `EXC1`, unconditionally.

  * Use `coraise_if (EXC1, COND);` to raise `EXC1` when `COND` is true or
    `coraise_unless (EXC1, COND);` to raise `EXC1` when `COND` is false.

  * Use `coraise_errno (EXC1);` to raise `EXC1` when `errno != 0`.

  * Use `coraise_zero (EXC1, VAL);` or `coraise_nonzero (EXC1, VAL);` to raise
    `EXC1` when `VAL` is zero or non-zero, respectively.  The macro `coraise_eof`
    has the same effect as `coraise_zero`, but is easier to read when applied to
    the results of a `read()` or `write()` operation, including its variants on
    pipe nuts defined below.

  * Use `coraise_null (EXC1, VAL);` to raise `EXC1` when `VAL` is `NULL`.

  * Use `coraise_neg (EXC1, VAL);` or `coraise_min1 (EXC1, VAL);` to raise `EXC1`
    when `VAL` is negative or -1, respectively.

  * Use `coraise_ckrv (EXC1, VAL);` to raise `EXC1` when `VAL` differs from
    `CKR_OK`; this is particularly useful with PKCS #11 programming, a security
    API that is commonly used in applications that demand tight resource control
    and exception handling.


## Pipe Nuts

Pipe nuts are the primary communication mechanism for coros.  Where "normal" C
functions exchange information through parameters and return values, there is a
need for more flexible communication between coros.  We follow the model that
Occam and Ada have, by exchanging data in synchronous communication, that is,
involving lock-step by two coprocesses.

What we have defined is a bit more general, in fact.  When used with the same
message sizes on both ends the operations are indeed in lock-step; but there
is a buffer setup on each end, and data is passed between those buffers with
a given minimum length on each end; when those minimum lengths differ, it may
happen that one end continues while the other is still waiting.  There are
also maximum lengths setup, and these are what determine the actual sizes
exchanged, namely the maximum length that both can agree with.

The code for pipe nuts is largely contained in a library, with just minimal
definitions in macros.  This helps to reuse code; the definitions are mostly
needed for the coro aspects.

There is an event mechanism, defined below, related to each conut.  This is
used to notify the other core (the other nut on our pipe) of new developments
worthy of their investigation.  Events are merely hints to wakeup and inspect,
they may be sent too often but not forgotten or dropped casually.

The API for operating on a Coconut pipe nut, or conut for short, consists of:

  * Declare conuts with `copipenuts { MASTER, SLAVE1, SLAVE2, SLAVE3 };` which
    will assign a local symbol to the number of the conut within the current
    coro.  The symbolic name can be used in the calls below.

  * `conut_connect(TODO)` contacts another coro's given pipe nut.  It does this
    by registering in the remote pipe nut's queue for pending connections and
    then trigger the event for that remote pipe nut.  After this, `coyield()`
    is used until the remote conut accepts (through a conut event for the local
    end point).

  * `conut_disconnect(pnut)` disconnects the local coro's connection.  This is
    not always possible; while a buffer is activated this should not be done.
    There will be no current remote conet after this call.

  * `conut_accept(TODO)` indicates that a new pipe nut is taken off the queue
    and accepted as the remote conut for upcoming communication.  If there is
    a current remote conut, it will first pass through `conut_disconnect()`
    and this operation is only possible when `conut_disconnect()` is also
    possible.  The old and new remote conut each receive a conut event.

  * `conut_read(pnut,buf,buflen)` reads buffer bytes into `buf` from
    the connected remote conut.  The minimum lenght returned is 1, the maximum
    is `buflen`.  Only during end-of-file will a lenght 0 be returned, and only
    in an error condition will a negative value be returned, namely a negated
    error code from `<errno.h>`

  * `conut_write(pnut,buf,buflen)` writes buffer bytes from `buf` to the connected
    remote conut.  The minimum length written is 1, the maximum is `buflen`.  Only
    when `buflen` is set to 0 will an end-of-file be sent.  When an error occurs,
    the return value will be a negated value from `<errno.h>`

  * Note that setting the same value for `buflen` in `conut_read()` and in
    `conut_write()` will result in actually passing that many bytes, and both
    operations should continue to work in lock-step.  This is how the
    RendezVous mechanism works.  Upon end-of-file or an error condition will the
    buffers be realigned.  Error conditions are observed on both the connected
    conuts.

  * The transfer of data from writer to reader is done in lock-step.  This means
    that the first conut to attempt a write will have to wait for the second one
    to get ready.  When the second one has transferred data, a signal is sent to
    its remote conut, so it may see if the minimum amount has been transferred,
    and thus if processing may continue.

  * Variations with modifiable minimum amounts can be used instead of the standard
    ones; they are `conut_read_min(conut,buf,minlen,buflen)` and
    `conut_write_min(conut,buf,minlen,buflen)` where the `buflen` may be interpreted
    as a `maxlen`, symmetrically mirrorring the `minlen` argument.  In situations
    of end-of-file or error, the transferred amount may still be lower than `minlen`.

  * The operations for reading and writing can be split up into smaller parts,
    and processed separately.  In this case, a buffer is first setup and a series
    of synchronisation operations proceeds until the minimum amount has been
    transferred.  One use of this is to first obtain a small header and then, if
    the header indicates a need for extra data, to synchronise towards a higher
    minimum.  Finally, there is a reset operation that prepares the buffer for
    another round of usage.

  * Use `conut_setupbuf(pnut,wr,buf,buflen)` to setup conut `pnut` for writing
    (if `wr` is true) or reading (if `wr` is false).  Provider buffer `buf` with
    length `buflen`.  When writing, the data should be setup.  The other side
    can now start processing the data.  Note that one cnut must be setup for
    writing, and the other for reading.  This role can change with every
    `conut_setupbuf()` invocation, but this should be done on both ends.
    The other end will be signalled of available data.

  * Use `conet_sync(pnut,minlen)` to synchronously copy information from the
    writer conut to the reader conut.  The return respects `minlen` as the
    minimum transferrable length, but will return error conditions as negated
    values from `<errno.h>` and end-of-file as 0.  The routine may give rise
    to internal `coyield()` invocations, so as to permit the other coro to
    do what it takes to finish its work.

  * Use `conet_process()` to cause conut processing in an event-driven manner.
    This is certainly not the only manner of processing data that travel from
    and to conuts, but it may integrate nicely with asynchronous communication
    semantics.

  * Use `conet_push(pnut)` to send an end-of-file indicator at the end of any
    content still in transit.  This must only be done on the writer's side.
    The corresponding reader's action is `conut_pull(pnut)` to wait for no data,
    but just the end-of-file condition.  This is a simple, synchronised
    signalling mechanism.

  * Use `conet_error(pnut,errno)` to set an error condition `errno` on conut
    `pnut` and its connected remote.  This only has an impact if there is no
    error condition yet, and after all the data has been exchanged that is
    currently in transit.  New data will not be accepted anymore.  Note that
    this means that processing of data should continue if transactional
    semantics are desired.  When passing a resource, this may be important to
    establish who holds ownership of the resource, and is responsible of
    cleaning it up.  You are free to shape this as you please, but stay mindful
    about the potential for resource leakage; an explicit acknowledgement such as
    an opposite-direction `conet_push()` and `conet_pull()` is advisable after
    having sent a resource, to be certain that ownership has been passed.
    Errors are not always accepted; `EPIPE` is reserved for end-of-file
    conditions and `ECONNRESET` for disconnection; `EAGAIN` is reserved as an
    advise to `coyield()`.  Finally, if either side already has an error
    standing by for processing, then the error also is not accepted.

  * During `conet_disconnect()`, what happens is sending an error condition to
    the other side, namely setting `ECONNRESET` and once done, detaching from the
    remote conut.  This is considered a signal to back off and disconnect.
    There is no reason why connecting again should be forbidden though.
    TODO: Interaction with outstanding error conditions?  Especially `ECONNRESET`?


## Events

As part of the conut system, there is a facility for signaling events to the
remote conut.  Signals should not be lost, but they might get merged; handling
code is cautious to erase the signal before starting the handler, so no potential
work to be handled needs to be missed.  For handling code this means that all
possible causes for a signal should be addressed.

The API to operate on events is relatively simple:

  * `conut_trigger(pnut,target)` triggers TODO on TODO.  TODO: name spaces.

  * `copoll(pnut)` listens in on events to conut `pnut`.  This is a declaration
    that will be skipped when execution runs into the code; it will be found
    without ever having crossed the processor; these properties combine to allow
    you to place it anywhere you like.  Note that the causes for triggering this
    event can be anything conut-related, ranging from connection requests or
    connection acceptance by a remote to a report that data is available for
    processing and the occurrence of an error condition.  Plus, there may be
    application-specific reasons that caused the invocation.

  * TODO


## Scheduling Coroutines

Each coro may be managed by at most one coro scheduler.  Such a scheduler holds
two lists, one with running and another with waiting coros.  As long as there
are running coros, the scheduler will run `cogo()` on each of them in a round-robin
fashion.  A coro that returns 0 will be moved to the inactive list, from which it
will be revived when an event is sent to it through `cotrigger()`.

A scheduler does not normally return control; the exception is when both of its
lists return empty and it obviously has nothing left to do, and will not get
anything else to do either.

When a coro creates another, the new coro will usually be entered in the same
scheduler, but only after having run `coinit()` on it.  This ensures that only
initialised coros are freely scheduled.  Reversely, a coro that ends its finaliser
will unregister from its scheduler, so it is not scheduled anymore.

The complete lifecycle of a coro is as follows:

 #. The coro is created, but remains a lifeless data block.  It is normally
    setup with all zeroes.  Creation is done through dynamic allocation with
    `conew()` or stack allocation with TODO.

 #. In an optional intermediate phase, the coro may be connected to other coro's.
    Since nothing is running at this point, it is the perfect phase to create a
    network of collaborating coros without running into race conditions.

 #. The coro is initialised through `coinit()`, which may supply it with parameters
    through "normal" C function arguments, which are made available one by one
    through `coarg()`, a wrapper for variable argument lists.  Clearly, this is
    an untyped phase and clearly there may be variations to the initialisation
    process.  The initialisation code is anything before the `cobody`.  There
    are likely to be declarations in this section, which will be skipped as always.

 #. After initialisation, the calling environment may add the coro to a scheduler,
    which will usually be the same one it is on.

 #. The coro runs the `cobody` for as long as it sees fit.  It will ignore any
    events that arrive.  Resources may be allocated and flagged for future
    removal.

 #. At the end of the `cobody`, the coro switches to event handling by default.
    In this stage, it will respond to incomping events by invoking the respective
    `copoll()` handler.  No two `copoll` handlers run together, and it is quite
    possible to `coyield()` in a handler, so this is a phase where lockups of
    the coro are still possible.  Resources may be allocated and flagged for future
    removal.

 #. Optionally, when event handling is not desired, or when an event handler or
    exception handler calls for it, the `codone()` call may jump to the end of
    the coro.  This is an explicit action; the normal behaviour is to remain in
    the previous phase forever.  Alternatively, the environment may disrupt the
    coro asynchronously through `codestory()`.

 #. After `codone()`, the first action is to run the finalisation code, that is
    with all resources still in tact.  This helps with graceful take-down of the
    coro, which may want to give a farewall kiss to some of its resources and
    conuts.  The coro may still use `coyield()` during this phase of its life.

 #. Finally, after the finalisation code has run, any remaining resources will be
    cleaned up through their designated cleanup handlers.  This is automatically
    done, and may again take some time and include `coyield()` invocations.

 #. After all this, the coro returns its final result, namely the return value 0.
    TODO: Or -1, or another signal that says "please deallocate me"?  Or leave that
    to a creator always?


## Networks of Coroutines

It is not common to find a coro in isolation; it is usually part of a larger whole,
and will be networked in a fixed manner to other coros.  Setting up such networks
can be done in a coro, or in "normal" C code, and constitutes what we will call a
*coronet*.  We call the code that creates a coronet a *coronet factory*.

Setting up a coronet is usually done by explicitly creating connections between
coros that have been created, but not yet initialised.  It is a great help with
getting a complex program setup and running.

Networks are collections of coros and schedulers manage collections of coros, so
it is quite likely that a network has its own scheduler.  To facilitate this
likely and likable scheme, it is possible to initialise a scheduler with a
coronet factory function.

Inside a coronet factory, the functions are the equivalent of invoking
`coconnect()` from one conut and `coaccept()` from the intended peer, except
that it can be done with one call to `conut_makepipe()`.  That call assumes that
the conuts have not yet been initialised.


## (No) Facilitation for POSIX threads

The POSIX threads **do not currently combine well*** with coroutines.
The only things that are safe:

  * Triggering events in other coroutines *will probably work* -- but they cannot be
    guaranteed in general.  Trigger events are probably the first thing that we will
    move to something atomic, because they can be a great help with event-driven I/O,
    which may simply trigger an event and have a coroutine use non-blocking I/O
    on the indicated resource.
  * Access pipe nuts from different threads, but *never access related pipe nuts
    from two threads* at the same time.  This may be overcome by a locking scheme
    or, more attractively, a scheme based on atomic operations, in future versions.
    For now, you are on your own when you do this.
  * Call coroutines from different threads, but *one coroutine may never be run by
    multiple coroutines* at the same time.  You should devise a locking mechanism
    or, much simpler, have separate pools of coroutines run by each thread.  If you
    desire to move coroutines between threads you will need to be clever about
    locking the pools, but that should not be in regular use.

It is the current intention to provide atomic operations to lift the restrictions
on these patterns in future releases.  Such patterns will only be compiled in
when the environment indicates use of pthreads.

A first stab at support of pthreads will be quite coarse-grained:

  * Event triggering can stay as it is.
  * Pipe nuts will be provided with a locking mechanism; note that deadlock
    situations may arise when A talks to B while B talks to A.  We will use
    `pthread_mutex_trylock()` to avoid this problem, and back out without
    result through `coyield()`.
  * We can integrate a mutex on each coroutine, and use `pthread_mutex_trylock()`
    to grab it, and `coyield()` if this is refused.

Note that yielding when a lock cannot be grabbed avoids deadlock problems, but it
may result in an equally unattractive situation of livelock.  This situation is
more likely to resolve automatically due to random perturbance in the system, but
this is not completely certain.  It may be an idea to introduce random delays in
scheduling loops, so as to provide a slight advantage that tips the coincidental
balance.

Atomic operations are not standardised for C as far as I know; but they are
[included in C++](http://www.cplusplus.com/reference/atomic/)
through the `<atomic>` file, and may also be available to C through library
calls and
[linking C++ to C](http://www.objectvalue.com/articles/LinkingCtoCplusplusV02.html).
Note that header-only solutions for C++ are not likely to port back to C.
It may be easier to standardise on
[libapr atomic operations](http://www.red-bean.com/doc/libapr1-dev/html/group__apr__atomic.html).


## Compiler-specific Implementation Alternatives

There are a few opportunities based on compiler-specific behaviour.
These may be helpful to implement the portable behaviour more efficiently.
But, given that Coconut already is highly efficient, it may not add much more
than complexity, so it is not a given that this development will continue.

  * http://blog.codepainters.com/2014/04/23/poor-mans-raii-for-c/
  * https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
  * https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html#g_t_005f_005fatomic-Builtins

