/***** ALL THIS IS ONLY NEEDED WHEN PTHREADS CAN DIFFER BETWEEN PIPE NUTS *****/

THIS IS TERRIBLE, NOTHING IS SAFE FROM RACE CONDITIONS BUT UNFAIR LIFO SCHEDULING?!?

Maybe we can use that the removal from the list is done by the endpoint itself,
while addition to the list may be done by others in parallel.

At the very least that simplifies the complexity of getting in the connecter
peer field.

// Add "my" to the end.

	void **qentry;
	do {
		/* Wait until we're released, to avoid race conditions needed */;
	} while (apr_atomic_casptr (&my->qnext, NULL, NULL) == NULL);
	do {
		qentry = &pipenut->head;
		while (*qentry != NULL) {
			qentry = & (*quentry)->qnext;
		}
	while (apr_atomic_casptr (qentry, my, NULL) == NULL);

//NOTLIKETHIS// // Spin into the queue (at the head)
//NOTLIKETHIS// 
//NOTLIKETHIS// 	void *qhead;
//NOTLIKETHIS// 	do {
//NOTLIKETHIS// 		qhead = pipenut->head;
//NOTLIKETHIS// 		my->qnext = qhead;
//NOTLIKETHIS// 	} while (apr_atomic_casptr (&pipenut->head, my, qhead) == qhead);
//NOTLIKETHIS// 	// my is now inserted before qhead (which may be NULL, but who cares)

// Get the head of the queue

	void *qhead;
	do {
		qhead = pipenut->head;
		if (qhead == NULL) {
			break;
		}
	} while (apr_atomic_casptr (&pipenut->head, qhead->next, qhead) == qhead);
	// qhead may be NULL if there was no head
	if (qhead == NULL) {
		continue;
	}
	//TODO// Enqueuer may still set the qhead->qnext field?!?

// Better add to the end of the queue

	void *qtail;
	do {
		// Spin until our qnext is NULL
		;
	} while (apr_atomic_casptr (&me->qnext, NULL, NULL) == NULL);
	do {
		// Spin until we replace qtail with our address
		qtail = pipenut->tail;
	} while (apr_atomic_casptr (&pipenut->tail, my, qtail));

// May keep a tail pointer as well; NULL when nothing inserted and replaced with
	apr_atomic_casptr (&pipenut->tail, newentry, NULL);  // replaced if NULL

// When removing the tail entry, walk down the chain until the one pointing to
// head is found.  Then substitute the tail pointer.  Then remove the end.
// Other changes to the qnext must assume that it is NULL, especially new entry.

// Alternatively place new entry at the tail, by swapping NULL with the new entry.





/////////// SWAP LENGTHS IN A THREAD-SAFE MANNER //////////


// Assume 32-bit max, 32-bit act
// Clean max, increment it, then memcpy and only then signal?  Spuriouses :-S
// Instead, have an available and written actual,
//  1. Claim act_prep by incrementing it
//  2. Perform memcpy to ptr + act_prep
//  3. Move act_prep to act_done
// Note that max is frozen during this time

// Alt: 16-bit act_prep and act_done
// Same pattern, change only halves at any one time

int32_t act_old, act_new;
do {
	act_old = apr_atomic_read32 (&my->peer->act_prep);
	act_new = act_now ...increment act_prep but not act_done if also loaded...
} while (apr_atomic_cas32 (&my->peer->act_prep, act_new, act_old) != act_old);
memcpy (..., ..., act_new - act_old);
if (apr_atomic_cas32 (&my->peer->act_done, act_new, act_old) != act_old) {
	assert (0);
}

THIS DOES SEEM TO BE A SOLID PROCEDURE -- PFEW, AT LEAST SOMETHING WORKS

