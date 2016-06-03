#ifndef COCONUT_H
#define COCONUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


/* BIG TODO: RESTRUCTURE SWITCH LABEL VALUES
 *
 * Efficient implementations may use a lookup table, for which we need to keep
 * the values close, with few gaps.  So layer them after each other, or over one
 * another (odd numbers for type A, even numbers for type B).  Using __LINE__
 * is still quite possible in this style as well, as they lead to constant
 * computations that can be done at compile time.  This style may however
 * interfere with styles based on fixed numbers, such as external jumps!
 */

/* OTHER OVERHAUL: MOVE FROM _co.coswitch TO _coswitch
 *
 * Only for return statements is there a need to store a value in _co.coswitch,
 * and this can often be done in the def that brings it there.  The coro then
 * starts by loading from _co, as in
 * _coswitch = _co.coswitch; _coloop: switch (_coswitch) ...
 */


/* The structure for a "coconut coroutine" contains the variables that the coconut
 * functions assume to be present, or potentially present, for the coro instance.
 *
 * It is generally assumed that coconut_coro is followed by an array of
 * coconut_pipenut structures, each representing one local communication
 * end point.
 */

typedef struct coconut_coro {
	bool (*corofun) (void *);    // the function implementing the coroutine
	struct coconut_coro *next;   // next in coro queue
	int coswitch;                // the label to jump to inside of _coloop
	int cleanpost;               // the label to jump to after a cleanup step
	uint32_t resopen;	     // bits for each open resource
	uint32_t activity;           // flags for unhandled pipe nut events
	const uint32_t *services;    // one service entry for each following pipe nut
} coconut_coro_st, *coconut_coro_t;


/* Static description parts for "coconut pipes" are useful for creating them,
 * as well as for managing them.
 */
struct coconut_coro_static {
	const int8_t numservices;	// How many pipenut services are defined
	const uint32_t services;	// Service codes handled by pipe nuts
	const char *function;		// Coro function name
	const char *function_type;	// Coro function name and type (if known)
	const size_t datasize;		// Size of coro data structure
};

/* The structure for "coconut pipes" is the glue between two coconut functions.
 * There should never be both a reader and writer waiting to communicate.
 */
typedef struct coconut_pipenut {
	struct coconut_pipenut *peer;   // Current related peer for this pipenet
	uint8_t *buf;			// Read/write buffer, or NULL if none
	size_t ofs, len, todo;		// Buffer offset, length and minimum-to-do
	int16_t errno;			// Error to report locally (EPIPE for EOF)
	coconut_coro_t queue;		// Others queueing up for this port
} coconut_pipenut_st, *coconut_pipenut_t;


/* Initialise a coconut_coro_t so that it starts at the beginning.  The routine
 * _codestroy() iterates over resource bits and frees all.  This can be called
 * from inside or outside the coroutine (!) and will leave those marked with
 * cocleandone(), assuming that those were meant to stay, but all the other
 * cocleantodoaction() are run to free those resources.
 *
 * TODO: Invoke coroutine parts for coinit() and codestroy()?
 *
 * Note how easy it is to setup coinit() and codestroy() in your coroutine.
 * This is not a coincidence, and there is a reason why we defined cosub() too.
 * Go ahead and have a ball -- benefit from resource management and exceptions!
 */
#define coinit(C,F) ((coconut_coro_t)(&(C)))->corofun = (bool(*)(void*)) (F); ((coconut_coro_t)(&(C)))->next = NULL; ((coconut_coro_t)(&(C)))->coswitch = -99997; ((coconut_coro_t)(&(C)))->resopen = 0
#define codeclare(T,C,F) (T) (C); coinit (&(C),(F))
void _codestroy (coconut_coro_t selfp);
#define codestroy(C) _codestroy(&(C))
#define cosuicide(C) free (&_co)

/* Invoke a standard-typed coroutine to make it run a bit more.  This is like
 * coyield(), but targeted at a specific coroutine.  Where coyield() is used
 * from within a coroutine, cogo() is meant to be called from outside one.
 * The same calling convention as for the other outsider macros is used,
 * namely referring to the structure instead of a pointer.
 */
#define cogo(C) (*(C).corofun) (&(C))


/* The beginning and end of a coroutine are marked by cobegin() and coend().
 * It is important to realise that code before cobegin() will be executed
 * on every coroutine entry, and may therefore be less suitable.  A variation
 * coendresources() defined below adds cleanup of open resources.
 *
 * We define coyield() to return 1, to indicate that work remains to be done,
 * whereas coend() returns 0 to indicate that nothing more is left.  Once 0
 * is returned, it will consistently be reported because the coroutine keeps
 * jumping to the coend() case where 0 is returned.  It is however possible to
 * restart a coroutine.  Use codone() to indicate that the coroutine should
 * finish -- that is, proceed towards either coend() or coendresources().
 */
#define cobegin() _coloop: switch (_co.coswitch) { case -99997:
#define coend() case -99998: _codestroy ((coconut_coro_t)selfp); return 0; }

//--OR-- use the form "cobody { ... }" --and-- move switch() to couroutine()

#define cobody va_end (coarg); while (1) if (0) { case -99998: _codestroy ((coconut_coro_t)selfp); return 0; } else case -99997:

#define codone() { _co.coswitch = -99998; goto _coloop; }

#define coyield() { _co.coswitch = __LINE__; return 1; case __LINE__: ; }

/* Coroutines can invoke cosubroutines.  This makes them set that routine, and
 * cause any returns to re-invoke the cosubroutine.  Note that this is different
 * from calling a normal subroutine, which will not break off and can also not
 * be restarted for continuation of its flow.
 *
 * Cosubroutines return 0 when they are done, or 1 when they are still busy.
 * This may in general be a reasonable convention to follow, but it is not yet (TODO)
 * strictly required for coroutines.  The mechanism is fairly efficient because it
 * invokes the coroutines almost directly.
 */
#define cosub(F) _co.coswitch = __LINE__; case __LINE__: if (F) { coyield (); }

/* Exception handling is based on labels that MAY be declared after cobegin(), using
 * coexceptions { EXC_A, EXC_B, EXC_C }; note the braces.  When handling, one
 * exception handler can be declared for each, and it may be raised from various
 * places in the program.
 *
 * Note that this has no bearing on generated code anymore, since we switched to
 * goto-based exceptions; however, the declarations are nice and informative, and
 * have no runtime footprint whatsoever.  The goto labels are prefixed with
 * EXCEPTION_ to make them more likely to be unique.
 */

#define coexceptions enum _coexceptions

/* Catch an exception with a special label value.  Note that previous code will
 * skip this code if it hits upon it (making this a declaration).  When done,
 * exception handling continues to 
 */
#define cocatch(E,F) if (0) while (1) if (1) goto EXCEPTION_ ## F; else EXCEPTION_ ## E:
#define cocatch_done(E) if (0) while (1) if (1) codone (); else EXCEPTION_ ## E:
#define cocatch_continue(E) if (0) while (1) if (1) conut_process (); else EXCEPTION_ ## E:
#define cocatch_fatal(E) if (0) while (1) if (1) exit (1); else EXCEPTION_ ## E:

/* Raise exceptions by jumping to special label values.
 */
#define coraise(E) goto EXCEPTION_ ## E
#define coraise_if(E,C) if (C) coraise (E) else { }
#define coraise_errno(E)       coraise_if (E,errno)
#define coraise_zero(E,V)      coraise_if (E,(V)==0)
#define coraise_nonzero(E,V)   coraise_if (E,(V)!=0)
#define coraise_eof(E,L)       coraise_if (E,(L)==0)
#define coraise_null(E,V)      coraise_if (E,(V)==NULL)
#define coraise_neg(E,V)       coraise_if (E,(V)<0)
#define coraise_min1(E,V)      coraise_if (E,(V)==-1)
#define coraise_ckrv(E,V)      coraise_if (E,(V)!=CKR_OK)


/* Use coresources { A, B, C } to declare resource names; note the braces.
 * The definition installs a cleanup routine that iterates over resource bits
 * and jumps to their respective cleanup operations, which then loop back to
 * the cleanup mechanism.  When nothing remains to be cleaned, the routine
 * jumps to the coend label.  Resources are cleaned in the order in which
 * they are declared in coresources.  TODO: separate routine can do the opposite.
 */
#define coresources _co.resopen = 0; while(0){ case -99999: coyield(); } enum _coresources

/* Define a cleanup todo, to be inserted at the place where the resource is created.
 * There is a variation with cleanup code, and one without.  For each resource, there
 * must be precisely one cleanup place or else the coro logic will fail.
 *
 * We currently do not share resources with other coros.  This means that they can
 * be treated as simple flag registers.  If we decide to export resources and
 * permit others to take responsibility over them, for instance when we pass the
 * resources to another core through a conut, then we need to give other coros the
 * permission to clear them.  Resolving race conditions due to communication errors
 * will then either need clear semantics (based on the mutual recognition of the
 * error) or an atomic procedure for clearing resources.  The clear semantics
 * should translate into a coro clearing its own resource flag, instead of having
 * the other coro do this.
 */

#define cocleantodo(R) _co.resopen |= (1<<(R))
#define cocleandone(R) _co.resopen &= ~ (1<<(R))

#define cocleanaction(R) if (0) while (1) if (1) { _co.coswitch = _co.cleanpost; goto _coloop; } else case -100000-(R): if (cocleandone (R), 1)

#define cocleantodoaction(R) if (1) cocleantodo (R) else while (1) if (1) { _co.coswitch = _co.cleanpost; goto _coloop; } else case -100000-(R): if (cocleandone (R), 1)

/* The cocleanwhen(R) invokes a cleanup action when the given resource is currently
 * open.  This is for example useful in exception handlers that want to assure that
 * certain resources are closed.
 */
#define cocleanwhen(R) if (_co.resopen & (1<<(R))) { _co.cleanpost = __LINE__; _co.coswitch = -100000-(R); goto _coloop; case __LINE__: ; } else { }

/* The coread() and cowrite() macros expand to the more general minimax forms, then
 * invoke subroutines within a suitable context that allows them to leave.
 */
//TODO:HARDLY// #define  coread(P,B,L)  coreadm(P,B,1,L)
//TODO:HARDLY// #define cowrite(P,B,L) cowritem(P,B,1,L)

//TODO:HARDLY// #define  coreadm(P,B,M,L) cosub ( _coreadm (P,B,M,L))
//TODO:HARDLY// #define cowritem(P,B,M,L) cosub (_cowritem (P,B,M,L))


//TODO// Probably declare an enum of pipenuts, as indexes in an array
//TODO// What to do with a convention that 0 == meta?  (for connection requests)
//TODO// Define referenced function prototypes
//TODO// Definitions below are not read/write anymore, that is for connect()
//TODO// Could we mirror the actions?  Perhaps even toss the bytes?!?
//TODO// We might use a variation on poll() here, returning after many fail us
//TODO// Can't we permit single-duplex changes between read/write mode?
// #define comove(P,B,L) comove_minmax(P,B,1,L)
// #define comove_minmax(P,B,M,L) _comoveprepminmax (P,B,M,L); case __LINE__: _coio = _comove_poll (P, &_coio); if (_coio == -EAGAIN) { _co.coswitch = __LINE__; return 1; } /* TODO: release _coio */

#define  conut_read(P,B,L) _coio=1;  conut_read_min ((P),(B),_coio,(L))
#define conut_write(P,B,L) _coio=1; conut_write_min ((P),(B),_coio,(L))

/* Unfortunately we cannot return a pleasant value from coread() and cowrite()
 * because the coro structure makes them statements, not expressions.
 * We will certainly need conut_size() for conut_poll(), so perhaps it's not
 * all bad.
 */
#define conut_size() _coio

#define  conut_read_min(P,B,M,L) conut_setupbuf ((P),0,(B),(L)) case __LINE__: _comove_sync ((P), &_coio); (M) = _coio

#define conut_write_min(P,B,M,L) conut_setupbuf ((P),1,(B),(L)) case __LINE__: _comove_sync ((P), &_coio); (M) = _coio

//TODO// Possible form "comove_poll (P, &sz)) { coraise_neg (BAD,sz) ... continue; }"
//TODO// Alt "comove_poll (P, &sz, TRIGGER); when (TRIGGER) { }; comove_process()
// setupbuf sets actlen and offset to 0, sync updates minlen but will read at least 1
// resetbuf is a shorthand for reset of actlen and offset and reuse of buf, maxlen

void conut_setupbuf (coconut_pipenut_t pnut, bool wr, uint8_t *buf, size_t maxlen);
void conut_resetbuf (coconut_pipenut_t pnet, bool wr);

#define conut_sync(P,sz) case __LINE__: _coio = _conut_sync (P, sz); if (_coio == -EAGAIN) { _co.coswitch = __LINE__; return 1; }

//TODO// Interface to command conut processing (and possibly leaving the coro)
//TODO// Usually, conut_process() is the "active" state of a coro after setup
#define conut_process() goto -11999

/* Return the highest-priority conut that is currently active, or -1 if none is.
 * Reset the flag when returning it.  The parameter is a pointer to the activity
 * flags of a coro.
 */
int8_t _conut_active (uint32_t *activity);

/* Trigger an event with a conut in another coro.  This may even be run from
 * another pthread, so it is the one thing that enables thread crossover
 * communication.
 * TODO: Should we also add a way to pass variables?  Perhaps in each conut?
 * Or could we use conut communication between threads using atomic operations?
 */
void conut_trigger (uint8_t conut, coconut_coro_t target);

/* Fixed conut activity codes, not actually assigned to pipe nuts but used for
 * initialisation and finalisation as requested from the outside.  These are
 * the highest two activity numbers available, and they are hoped not to overlap
 * with declared copipenuts.
 *
 * The values are for program use; they are not used by coconut functions.
 * The reason to fixate them instead of declaring them explicitly is to
 * support their external invocation from constructors and destructors.
 * When they are not defined, the normal action takes place, that is the
 * conut_process() handler.
 *
 * The macros cocatch_initialise() and cocatch_finalise() indicate where
 * control can start working on these events when they are sent.
 * TODO: This looks like they are exceptions.  Are they, really?!?
 */
#define conut_activity_initialise (1UL << 31)
#define conut_activity_finalise   (1UL << 30)

#define cocatch_initialise() case -12000 - 31:
#define cocatch_finalise()   case -12000 - 30:

/* Friendly aliases for a popular dialect.
 */
#define conut_activity_initialize conut_activity_initialise
#define conut_activity_finalize   conut_activity_finalise

#define cocatch_initialize() cocatch_initialise()
#define cocatch_finalize()   cocatch_finalise()


/* Explicitly invoke event processing within the coro.
 */
#define conut_process() goto _coeventloop

/* Define an event handler.  At most one of these is permitted for each event,
 * and it can occur anywhere because it is not part of the normal program flow.
 * Absense of an event handler means that the event is silently ignored.  When
 * done, the coro returns to its event handler loop, and eventually coyield()s.
 *
 * Follow the event handler with a single statement or a compound statement:
 *
 * copoll (evt1) printf ("That tickles!\n");
 *
 * copoll (evt1) { ...handler...; }
 *
 * To leave the handler early, use continue.  This will return control to the
 * event loop, just as is normally done when the end of the handler is reached.
 */
#define copoll(e) if (0) while (1) if (1) goto _coeventloop; else case -12000-(e):

/* Specify what conuts will be used in this coro.  The symbolic names will be
 * used to identify conuts in the utility functions, as well as to define the
 * processing targets for conut_poll().  Declare them with braces instead of
 * brackets, to accommodate variable sizing, for instance
 *
 * copipenuts { MASTER, SLAVE1, SLAVE2, SLAVE3 };
 *
 * The earlier-mentioned pipe nuts take precedence over the later ones.
 *
 * This also declares the temporary variable for storage of conut_size() which
 * is therefore not retained across coro invocations.  TODO: Is it a good idea
 * to continue to be able to retrieve that outcome from the conut?
 */
#define copipenuts size_t _coio = -EPIPE; while(0) { default: case -11999: _coeventloop: _co.coswitch = -12000 - _conut_active (&_co.activity); if (_co.coswitch == -11999) return 1; } goto _coloop; enum _copipenuts


//TODO// Interface to welcome queued parties trying to connect; enqueue cur peer?
//TODO// Are these blocking calls?
#define conut_next(P) _comovenext(P)
#define conut_reconnect(P)

void conut_error(P);

/* The copush() and copull() macros also expand to the longer macros, setting 0
 * for the maximum length and 1 for the minimum length; the only way that will
 * end is in an error, which usually is the EOF marker.
 */
#define conut_push(P) cowrite_min ((P),NULL,1,0)
#define conut_pull(P)  coread_min ((P),NULL,1,0)

/* A naming convention: call with a coconut_coro_t or a struct that can be casted
 * to one (because its first field is that) and name it "selfp".  Then, in the
 * course of the coroutine, refer to its fields as "self" and to the coroutine
 * administration as "_co".  You can use the forms "self.x" and "_co.y"
 * in your code.  The coconut function definitions rely on this convention.
 */
#define self (*selfp)
#define _co (*(coconut_coro_t)selfp)
#define coro(name,type) bool coro_ # name (type selfp) { cobegin();
#define corodone() coend(); }

/* Static aspects of a coro are called its coclass.  Every instance points here,
 * and it is also referenced from conew() and similar operations.  It is usually
 * declared as a constant global variable named coro_NAME_class.
 */
typedef struct {
	char *coroname;
	bool (*corofun) (void *, ...);
	uint8_t conutcount;
	size_t datasize;
} coclass_st, *coclass_t;

#define coroutine(T,N) bool (N) ((T) *selfp, ...) { if (_co.coswitch != 0) goto _coloop; else
#define coroutine_end }
//--OR-- use 0 for the initialiser, and setup va_arg stuff for it
#define coroutine(T,C,N) const coclass_st coro_ ## (N) ## _class = { # N ,  coro_ ## (N) ## _fun, (C), sizeof (coro_ ## (N)) }; bool coro_ ## (N) ## _fun ((T) *selfp, ...) { switch (_co.coswitch) { case 0: _co.coswitch = -99997; va_list coarg; va_start (coarg, selfp);
#define coroutine_end }

//--ALT-DECL--
#define coroutine_decl(T,C,N) bool (N) ((T) *selfp, ...); typedef coro_ ## (N) { coconut_coro_st coro; coconut_pipenut_st pipes [(C)]; user (T); }; extern coclass_st coro_ ## (N) ## _class;
#define self (selfp->user)
#define _co (selfp->coro)


#endif /* COCONUT_H */
