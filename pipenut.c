
#include "coconut.h"


/* Communication between pipe nuts goes through a number of phases.  The process
 * has been carefully designed to provide transactional certainties due to
 * well-coordinated sides, while supporting both variable-sized and fixed-sized
 * content; the latter being the more trivial form because minimum, maximum and
 * actual sizes relayed will usually be the same.
 *
 * INITIAL: No connection to another pipe nut has been made, the queue is empty.
 *	There is no buffer, and no errno value yet.
 *
 *      Test: buf == NULL, errno == ?, me->peer == NULL
 *	Actions: "get connected (TODO:HOW?)" --> CONNECTED
 *
 * CONNECTED: Another pipe nut is installed as the remote, and at some point
 *	this is hoped to be mutual.  There is no buffer, and no errno value yet.
 *	When we disconnect, or connect to the next peer, we drop the conncetion.
 *	This is possible in the current state because we are not exchanging.
 *
 *	Test: buf == NULL, errno == 0, me->peer != NULL, max == 0
 *	Actions: conet_setupbuf() --> READY
 *
 * READY: The buffer and its maximum size has been setup, as well as an offset
 *	and a direction (read or write).  Communication may be initiated by
 *	the other side to which we have connected.  It may still be the case
 *	that the other side has not connected to us.  But if it does then we
 *	won't hold back.  It's too late now to reconnect.  (TODO:BAILOUT?)
 *
 *	Test: buf != NULL, errno == 0, me->peer != NULL, 0 < max < min
 *	Action: _conut_sync() with return == 0   --> EOF
 *	Action: _conut_sync() with return  < 0   --> ERROR
 *	Action: _conut_sync() with return  < max --> SYNCING
 *	Action: _conut_sync() with return == max --> COMPLETE
 *	Action: conut_full()  --> _conut_sync() --> EOF
 *	Action: conut_error() --> _conut_sync() --> ERROR
 *
 * SYNCING: The pipe nut is dedicated to getting the buffer contents passed
 *	from writer to reader.  The coro will return when the other side is
 *	not ready yet, as indicated by an EAGAIN return value from syncing,
 *	 and signals may be used to wake up a coro that has gotten sidetracked
 *	in the scheduler.  Syncing provides a minimum desired length.  The
 *	errno value is 0 on both ends.  Note that SYNCING and READY only
 *	differ in their minimum value; the setup during READY would never
 *	work out to report, but once we lowered to min <= max it becomes
 *	possible to actually trigger us if the remote delivered something
 *	while we were not looking.
 *
 *	In this state, you cannot run conut_setupbuf() or conut_resetbuf()
 *	because the end points may not be in lockstep.  You may however
 *	initiate passing through ERROR or EOF to get there, but then you
 *	should continue processing data until the EOF or ERROR condition
 *	is acknowledged to you -- in lockstep with the other side.  This
 *	is all very simple when you exchange fixed sizes and min==max,
 *	but things are not always that easy.
 *
 *	Test: buf != NULL, errno == 0, me->peer != NULL, min <= max
 *	Action: _conut_sync() with return == 0   --> EOF
 *	Action: _conut_sync() with return  < 0   --> ERROR
 *	Action: _conut_sync() with return  < max --> SYNCING
 *	Action: _conut_sync() with return == max --> COMPLETE
 *	Action: conut_full()  --> _conut_sync() --> EOF
 *	Action: conut_error() --> _conut_sync() --> ERROR
 *
 * COMPLETE: Syncing was successful, a full buffer max size has been exchanged
 *	and the buffer blocked because ofs==max; it is however possible to setup
 *	a new buffer or reset the current one for another pass.
 *
 *	Test: buf != NULL, errno == 0, min <= got <= max
 *	Action: conut_setupbuf() --> READY
 *	Action: conut_resetbuf() --> READY
 *
 * EOF: While syncing, the sender provided a zero-length write, which counts
 *	as the end of file marker.  Both ends now have errno set to EPIPE,
 *	which is a signal that EOF ought to be delivered locally.  To the
 *	reading end, this means receiving an explicit 0 length.
 *
 *	Test: buf != NULL, errno == EPIPE
 *	Action: conut_setupbuf() --> READY
 *	Action: conut_resetbuf() --> READY
 *
 * ERROR: An errno value unequal to EPIPE is setup locally.  Errors are set
 *	equally on both ends, to achieve coordination on the state and have
 *	eventual delivery of the error.  Each error is delivered once.  It
 *	is possible to reset the buffer and continue communicating, because
 *	both ends will have received the error.  Note that ERROR can already
 *	be caused during buffer setup, namely when the sides both want to
 *	write, or both want to read.
 *
 *	Test: buf != NULL, errno != 0, errno != EPIPE
 *	Action: conut_setupbuf() --> READY
 *	Action: conut_resetbuf() --> READY
 *
 * The mechanism has been designed under a few implementation assumptions:
 *  - the sides cooperate, behaving well while accessing each other's data
 *  - no two threads access communicating pipenuts at the same time
 */



/* The structure for a "pipe nut" is the endpoint of a pipe between twee coros.
 * A peer must be claimed by opening a connection to them, then transmission
 * takes place until EOF or an error condition is reported to both ends.
 *
 * Communication is one-way, except for the synchronous nature of data transfer.
 * Transfers may be set to a minimum (only overridden during EOF) and to a
 * maximum; partially done transmissions may be continued after a coyield().
 *
 * These structures are meant to be stored as an array in each coro; they can
 * be reset to all zeroes, and will be opened over a "meta" connection.
 */
typedef struct coconut_pipenut {
	struct coconut_coro_t coro;	// Current related peer's coro
	struct coconut_pipenut_t rnut;  // Current related peer's pipenet
	char *buf;			// Read/write buffer, or NULL if none
	size_t ofs, min, max;		// Buffer offset and min/max desired transfer
	unsigned short errno;		// Error to report locally (EPIPE for EOF)
	coconut_coro_t queue;		// Others queueing up for service
	bool writer, reader;		// Flags for our roles (both may be false)
};


/* Trigger an event with a conut in another coro.  This may even be run from
 * another pthread, so it is the one thing that enables thread crossover
 * communication.  Note that this does assume that the activated flags are
 * "somewhat atomic", in the sense that another thread operating on it will
 * not be slower than setting the flag and reading back a value independently.
 */
void conut_trigger (uint8_t conut, coconut_coro_t target) {
	uint32_t flag = 1UL << conut;
	if (flag == 0) {
		return;
	}
	while (!(target->activated & flag)) {
		target->activated |= flag;
	} 
}


/* Return the triggered event with the highest priority.  If none is active,
 * return -1 instead.
 *
 * I could not resist the temptation of making this a binary search which
 * is probably more efficient than searching linearly through the bits; the
 * complexity lies in the larger number of shifts, but that is quite easy
 * to implement in one computing cycle, as it basically constitutes
 * 2log(#bits) switches in a hardware implementation.
 */
int8_t _conut_active (uint32_t *activity) {
	uint32_t act = *activity;
	if (act == 0) {
		return -1;
	}
	register uint32_t mask = ~0UL;
	register uint8_t shift = sizeof (*act) * 4;
	register int8_t bitnr = 0;
	do {
		if (act & mask) {
			// Choose the lower half
			mask &= (mask >> shift); 
		} else {
			// Choose the upper half
			mask &= (mask << shift); 
			bitnr += shift;
		}
		shift >>= 1;
	} while (shift);
	*activity = act & ~mask;
	return bitnr;
}


/* The most brutal and direct manner of connecting two conuts to form a pipe is
 * to skip all negotiation and self-control.  This should not be done with coros
 * that have been initialised, but when they have merely been allocated this is
 * quite possible.  This function is intended for use in coronet factories only.
 * Brutal as this may be, it has its place and is much more efficient than the
 * complete negotiation required for conut_connect() / conut_accept() or the
 * symmetric pair conut_connect() / conut_connect ().
 */
void conut_makepipe (coconut_pipenut_t a, coconut_pipenut_t b) {
	assert (a->peer == NULL);
	assert (b->peer == NULL);
	assert (a->queue == NULL);
	assert (b->queue == NULL);
	a->peer = b;
	b->peer = a;
}


/* The _conut_accept() accepts any remote peer's attempt to conut_connect().  To
 * that end, it takes the first entry off of the queue and installs it as its
 * current peer.  If no such entry is found, the routine returns for coyield().
 */
bool _conut_accept (coconut_pipenut_t me) {
	assert (me->peer == NULL);
	coconut_pipenut_t newpeer = me->queue;
	if (newpeer == NULL) {
		return 1;
	}
	me->queue = newpeer->qnext;
	newpeer->qnext = NULL;
	me->peer = newpeer;
	conut_trigger (me, newpeer);
}


/* The proper method for conut_connect() can be used when the conut is in INITIAL
 * mode.  It first finds if the sought remote peer is in the queue awaiting a
 * connection and if so, removes it and continues like conut_accept().  Otherwise,
 * it will enqueue in the remote conut's queue and use coyield() to await being
 * _conut_accept()ed.
 */
bool _conut_connect (coconut_pipenut_t me, coconut_pipenut_t newpeer) {
	assert (me->peer == NULL);
	coconut_pipenut_t *qp = &me->queue;
	while (*me) {
		if (*me == newpeer) {
			// Already requested; act more or less like conut_accept()
			*me = newpeer->qnext;
			assert (newpeer->peer == me);
			me->peer = newpeer;
			// We are connected, and may continue.
			// The other side will be triggered.
			conut_trigger (me, newpeer);
			return 0;
		}
		qp = & (*qp)->qnext;
	}
	// The peer is not in the queue, so we sign up with it
	qp = &newpeer->queue;
	while (*qp) {
		qp = & (*qp)->qnext;
	}
	newpeer->qnext = NULL;
	*qp = newpeer;
	conut_trigger (me, newpeer);
	return 1;
}


/* Setup a pipenut buffer for communication, with a maximum length.  Also indicate
 * whether we will be reading or writing this round.  This can be modified later on.
 * It is assumed that a connection has been made to a remote.
 */
void conut_setupbuf (coconut_pipenut_t pnut, bool wr, uint8_t *buf, size_t maxlen) {
	assert (pnut->buf == NULL);
	assert (pnut->coro != NULL);
	assert (pnut->rnut != NULL);
	pnut->buf = buf;
	pnut->max = maxlen;
	conut_resetbuf (pnet, wr);
}

/* Reset a pipenut buffer for communication, assuming that buf and max have already
 * been setup by conut_setupbuf() before.
 * TODO: could there be additional traffic that we missed?
 */
void conut_resetbuf (coconut_pipenut_t pnet, bool wr) {
	assert (pnut->buf != NULL);
	assert (pnut->coro != NULL);
	assert (pnut->rnut != NULL);
	pnut->writer = (wr != 0);
	pnut->reader = (wr == 0);
	pnut->min = pnut->max + 1;	// err on the safe side
	pnut->ofs = 0;
	pnut->errno = 0;
	if ((pnut->writer && pnut->rnut->writer) ||
	    (pnut->reader && pnut->rnut->reader)) {
		pnut->rnut->errno = pnut->errno = EPROTO;
	} else {
		errno = 0;
	}
}


/* After buffers have been setup, or possibly reset, the communication can be
 * started.  Whether this is possible depends on the availability of the
 * buffer on the other side, but even if the other side acknowledges us as their
 * current communication peer.
 *
 * As part of the call, the minimum desired length is provided.  This is used
 * as a constraint for the delivery.  When investigation of the beginning
 * indicates that more data is needed, it is possible to call conut_sync()
 * again, and request more data.
 *
 * When done, the actual communicated length is returned to the caller, and it
 * is at least the minimum length, or 0 for end-of-file.  The return value
 * is this same value upon success, or -errno if an error is detected.
 * Communicated lengths over 0 but under the minimum length are reported as
 * -EPROTO, a protocol error.  Note that this error is also returned when
 * read/write coordination was not properly coordinated between the peers.
 * Finally, -EAGAIN is returned if the sync could not currently be achieved.
 */
int _conut_sync (coconut_pipenut_t me, size_t minlen) {
	assert (me->buf != NULL);
	int retval = me->errno;
	// First, in case of EOF or an error, return that status
	if (retval != 0) {
		*minlen = 0;
		if (retval != EPIPE) {
			// If this is ECONNRESET, we must now disconnect
			if (retval == ECONNRESET) {
				me->peer = NULL;
			}
			// Any non-EOF error will be reported immediately
			return -retval;
		} else if ((me->ofs > 0) && (me->ofs < minlen)) {
			// EOF but we did receive data, just not enough
			assert (me->peer->peer == me);
			me->peer->errno = me->errno = retval = EPROTO;
			return -retval;
		} else {
			// EOF or we received enough data, so report me->ofs
			return me->ofs;
		}
	}
	// Second, we test if the peer is ready to transfer data with us
	if (me->peer->peer != me) {
		// The peer is not acknowledging us as its peer... yet
		return -EAGAIN;
	}
	if (me->peer->errno != 0) {
		// The peer is in a state of (t)error and may be reconsidering us
		// (We should have processed the same error)
		return -EAGAIN;
	}
	// Third, determine the roles of reader and writer
	assert (me->reader != me->writer);
	assert (me->writer || me->peer->writer);
	assert (me->reader || me->peer->reader);
	if (me->writer) {
		w = me;
		r = me->peer;
	} else {
		r = me;
		w = me->peer;
	}
	//
	// Fourth, move as much information as possible from writer to reader
	len = w->max - w->ofs;
	if (len > r->max - r->ofs) {
		len = r->max - r->ofs;
	}
	if (len > 0) {
		memcpy (r->buf + r->ofs, w->buf + w->ofs, len);
		r->ofs += len;
		w->ofs += len;
	}
	//
	// Fifth, send a signal to our peer (which was apparently waiting for us)
	conut_trigger (me->rnut, me->peer);
	//
	// Sixth, harvest our personal results
	//TODO:STOPMYSIGNAL:AVOIDRACE// me->activity &= ~ (1UL << me->rnut);
	if (me->act < minlen) {
		// Not enough; please keep calling us, and/or we'll call you!
		return -EAGAIN;
	}
	return me->act;
}

