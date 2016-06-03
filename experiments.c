#include <stdlib.h>
#include <stdio.h>

#include "coconut.h"

bool g (coconut_coro_t selfp) {
cobegin ();
coresources { A, B, C, D };
coexceptions { E, F };
	printf ("++A");
	cocleantodoaction (A,
		printf ("--A")
	);
	printf ("++B");
	cocleantodoaction (B,
		printf ("--B")
	);
	printf ("++C");
	cocleantodoaction (C,
		printf ("--C")
	);
	printf ("keepB");
	cocleandone (B);
	printf ("hah");
	coraise (E);
	printf ("nah");
cocatch (E);
	printf ("dah");
	cocleanwhen (C);
coend ();
}

void h () {
	switch (337) {
	default:
		printf ("Snif\n");
		return;
	case 0:
		if (1) {
			while (1) {
				printf ("Nul op request!\n");
				goto labje;
			}
		} else {
			while (17 > 88) {
					printf ("Ohlala...\n");
				labje:
					printf ("Tadaaa!!!\n");
			}
			if (3 * 3 == 7) {
				case 337:
					printf ("Drie maal drie is zeven\n");
					goto labje;
			}
		}
	}
}

void i() {
	int x = 1, y = 2, z = 3;
	x = x;
	y = y;
	z = z;
	x = y;
	x = x;
	y = y;
	z = z;
	y = z;
	x = x;
	y = y;
	z = z;
}

struct {
	int x;
	int y;
	int z;
} jaja = { 65, 66, 67 };

intptr_t max (intptr_t a, intptr_t b) {
	return (a>b)? a: b;
}

void j() {
	int a = 1, b = 2, c = 3;
	intptr_t last = max (((intptr_t) &a), ((intptr_t) &c));
	intptr_t dist = ((intptr_t) &jaja.z) - last;
	void *p = alloca (dist);
	int x, y, z;
	x = x;
	y = y;
	z = z;
	x = y;
	x = x;
	y = y;
	z = z;
	y = z;
	x = x;
	y = y;
	z = z;
}

int k() {
	goto lab;
	while (0)
		lab: printf ("yes\n");
	goto nope;
	if (0)
		nope: printf ("toch wel\n");
	else
		printf ("eg niej");
	return 0;
}

int main () {
	h();
	i();
	//TOO SCARY// j();
	k ();
	coconut_coro_st co1 = { 0 };
	coinit (co1, g);
	cogo (co1);
	//TODO// codestroy (co1);
	printf ("\n%ld\n", ~0UL ^ ((~0UL) >> 1));
}


