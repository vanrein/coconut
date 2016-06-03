/* Erotasthenes sieve, a simple demonstartion of Coconuts in C.
 *
 * Yes, the code below really compiles on a standard C compiler.  It defines a
 * number of well-chosen macros in <coconut.h> and results in a number of new
 * programming language constructs:
 *  - coroutines (a.k.a. "coros")
 *  - synchronous communication (with endpoints k.a. "pipe nuts")
 *  - events
 *  - exception handling
 *  - resource management
 *
 * From: Rick van Rein <rick@openfortress.nl>
 */


#include <stdio.h>
#include <stdlib.h>

/* Then, the big include file that makes so much possible... */
#include "coconut.h"


/* The sieves will keep a "filternum" set to a multiple of the prime.  Any value
 * under the filternum will be accepted, values over the filternum will lead to
 * adding the prime as often as needed.  The filternum itself is of course
 * suppressed.
 */
struct filter {
	unsigned long prime;
	unsigned long filternum;
};

/* Declare the type "coro_sieve" and "coro_candidate_generator" coroutines
 * that will be implemented below
 */
coroutine_decl (struct filter, 2, sieve);
coroutine_decl (struct filter, 1, candidate_generator);


/* Construct a new prime filter coroutine.  This is split out into a "normal"
 * C function because it is used in two places.  It also demonstrates nicely
 * how "normal" C and coroutines can be mixed at will.  The first coroutine
 * is created with the value "2", after that the prime numbers will themselves
 * create new instances and gradually for a chain of filters to jointly build
 * Eratosthenes' sieve.
 */
coro_sieve *mkfilter (unsigned long p) {
	coro_sieve *retval = conew (coro_sieve);
	coinit (retval, p);	/* TODO: FORM MAY CHANGE? */
	return retval;
}


/* A coroutine named "sieve", whose data is stored in a "struct filter" that can
 * be addressed as "self".  It allocates 2 pipenuts.
 */
coroutine (struct filter, 2, sieve)

	copipenuts { prev, next };
	coexceptions { INPUT_ERROR };
	coresources { NEXT_STAGE };

	/* Initialisation code */
	self.prime = coarg (unsigned long);
	printf ("New prime number: %ld\n", self.prime);
	self.filternum = self.prime;

	/* Handle started for incoming data from the previous sieve filter */
	copoll (prev) {
		unsigned long tmp;
		int result;
		/* Read from the input filter.  This "blocks", in the sense that
		 * the coroutine will yield to other coroutines until a prime
		 * comes in.
		 */
		conut_read (prev, &tmp, sizeof (tmp));
		result = conut_size ();
		raise_neg (INPUT_ERROR, result);
		raise_eof (INPUT_ENDED, result);
		while (tmp < self.filternum) {
			self.filternum += self.prime;
		}
		if (tmp == self.filternum) {
			/* Drop the number, leave the copoll() handler */
			continue;
		}
		/* We have found a new prime number */
		if (!conut_connected ()) {
			coro_sieve *newflt = mkfilter (tmp);
			cocleanuptodo (NEXT_STAGE);
			fprintf ("TODO: CONNECT TO THE NEW INSTANCE\n");
		}
		/* Write to the output filter.  This "blocks", in the sense that
		 * the coroutine will yield to other coroutines until the prime
		 * comes is accepted.
		 */
		conut_write (next, &tmp, sizeof (tmp));
	}

	/* Exception handlers */
	cocatch_done (INPUT_ENDED) {
		fprintf (stderr, "Received EOF from the previous filter stage\n");
	}
	cocatch_fatal (INPUT_ERROR) {
		fprintf (stderr, "FATAL: input error from prior sieve stage\n");
	}

	/* Resource cleanup handler for the NEXT_STAGE resource */
	cocleanupaction (NEXT_STAGE) {
		printf ("Sending EOF to the next filter stage\n");
		conut_push (next);
	}

	/* Begin the main control for this coprocess */
	cobody {

		/* Invoke the event handler loop; all I/O is triggered by events */
		coprocess ();

		/* End the main control for this coprocess; cleanup and delete */
	}

	/* Coroutine finalisation code that runs after input dried up */
	printf ("No longer filtering for %ld\n", self.prime);
	cosuicide ();

coroutine_end


coroutine (struct filter, 1, candidate_generator)

	copipenuts { firstflt };
	coexceptions { OUTPUTFAILURE };
	coresources { FIRST_STAGE };

	/* Initialisation code: Create a filter to handle prime 2. */
	self.prime = 2;
	coro_sieve *firstflt = mkfilter (self.prime);
	cocleanuptodo (FIRST_STAGE);
	self.filternum = coarg (unsigned long);  // where the generator stops
	printf ("TODO: Connect to first filter\n");

	/* Exception handler */
	cocatch_done (OUTPUTFAILURE) {
		fprintf (stderr, "Stopping candidate generator: Could not write to first filter\n");
	}

	/* Resource cleanup handler for the FIRST_STAGE resource */
	cocleanupaction (FIRST_STAGE) {
		printf ("Sending EOF to the first filter stage\n");
		conut_push (firstflt);
	}

	/* This is simply a routine that pumps the values 2, ... into filters
	 */
	cobody {

		/* Run until wrap-around occurs */
		while (self.prime != self.filternum) {
			cowrite (firstflt, &self.prime, sizeof (self.prime));
			self.prime++;
		}

		/* Automatically proceed into finalisation and cleanup */
	}

	/* Finalisation code -- runs after wrap-around */
	fprintf (stderr, "Candidate generator is ending\n");

coroutine_end


int main (int argc, char *argv []) {
	/* Create and initialise an instance of the generator */
	coro_candidate_generater *sieve = conew (candidate_generator);
	coinit (sieve, 100);	// The argument indicates where the generator stops
	/* Run the scheduler on the sieve, with a growing number of coroutines */
	coschedule (sieve);
	printf ("Coroutine scheduler exited properly\n");
	/* The generator does not commit suicide, so we free it explicitly */
	cofree (sieve);
}

