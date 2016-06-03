
#include "coconut.h"

void _codestroy (coconut_coro_t selfp) {
	unsigned long flag = ~0;
	int cleaner = -100000 + 1 - 8*(int)sizeof (flag);
	flag = flag ^ (flag >> 1);
	if (_co.resopen) {
		do {
			if (_co.resopen & flag) {
				_co.coswitch = cleaner;
				_co.cleanpost = -99999;
				_co.corofun (&_co);
			}
		} while (cleaner++, flag >>= 1);
	}
}

