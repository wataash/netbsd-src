#include <sys/mbuf.h> // struct mbuf

// -----------------------------------------------------------------------------
// misc API

void wataash_m_print(const struct mbuf *m);
void wataash_timer_utils(void);

// -----------------------------------------------------------------------------
// wataash_sandbox
void wataash_sandbox(void);
