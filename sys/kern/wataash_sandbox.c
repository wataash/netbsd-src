#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <sys/cdefs.h>

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>     // atomic_add_32()
#include <sys/condvar.h>    // cv_init()
#include <sys/cpu.h>        // cpu_intr_p()
#include <sys/intr.h>       // softint_establish()
#include <sys/kmem.h>       // kmem_zalloc()
#include <sys/kthread.h>    // kthread_create()
#include <sys/localcount.h> // localcount_init()
#include <sys/mbuf.h>       // struct mbuf
#include <sys/mutex.h>      // mutex_init()
#include <sys/pserialize.h> // pserialize_read_enter()
#include <sys/psref.h>      // psref_class_create()
#include <sys/ras.h>        // ras_lookup()
#include <sys/rwlock.h>     // rw_init()
#include <sys/stdbool.h>    // bool
#include <sys/syslog.h>     // log()
#include <sys/systm.h>      // printf()
#include <sys/vmem.h>       // vmem_create()
#include <sys/workqueue.h>  // workqueue_create()
#include <sys/xcall.h>      // xc_broadcast()

#include <kern/wataash_sandbox.h> //
#include <lib/libkern/libkern.h>  // CTASSERT()
#include <net/if_ether.h>         // struct ether_header
#include <net/radix.h>            // struct radix_node_head
#include <netinet/ip.h>           // struct ip
#include <netinet/udp.h>          // struct udphdr
#include <netipsec/ipsec.h>       // m_makespace()

#ifdef DDB
#include <machine/db_machdep.h> // to #include <ddb/db_command.h>
//
#include <ddb/db_command.h> // db_command_loop()
#include <ddb/db_output.h>  // db_printf()
#endif                      /* DDB */

// -----------------------------------------------------------------------------
// misc API

void
wataash_m_print(const struct mbuf *m)
{
#ifdef DDB
	printf("m (%p):\n", m);
	m_print(m, "cd", printf_nostamp);
	printf_nostamp("\n");
#endif
}

static void ddb(void);

void
wataash_timer_utils(void)
{
	ddb();
}

// break signalでddbに入れないとき用（あるか？）
static void
ddb(void)
{
#ifdef DDB
	volatile bool x = false;
	if (!x) // gdb: break if x = 1
		return;
	char c = db_cmd_on_enter[0];
	db_cmd_on_enter[0] = '\0';
	db_command_loop();
	db_cmd_on_enter[0] = c;
#endif /* DDB */
}

// -----------------------------------------------------------------------------
// wataash_sandbox

static void kthread_workqueue(void);
static void locking(void);
static void macros(void);
static void mbuf_(void);
static void mem(void);
static void net(void);
static void softint(void);
static void sysctl_(void);
// x_*(): executed after system initialization, called by TODO
static void x_net(void);

void
wataash_sandbox(void)
{
// @wataash:debug-init:v7 c2_wataash_sandbox
#if 0
for (;;) {
	volatile bool break_ = false;
	__asm__("nop"); // (gdb) break if break_ = 1
	if (break_)
		break;
	__asm__("nop");
}
#endif

	printf("wataash_sandbox(): begin\n");

	kthread_workqueue();
	locking();
	macros();
	mbuf_();
	mem();
	net();
	softint();
	sysctl_();

	// TODO
	(void)x_net;

	printf("wataash_sandbox(): end\n");
}

// -----------------------------------------------------------------------------
// wataash_sandbox - kthread_workqueue

static void
kthread_workqueue_kth_thread(void *arg)
{
	kthread_exit(0);
	__asm__("nop");
}

static void
kthread_workqueue_kth(void)
{
	// https://man.netbsd.org/kthread.9

	lwp_t *thread;

	// clang-format off
	int error = kthread_create(PRI_NONE, KTHREAD_MPSAFE | KTHREAD_MUSTJOIN, /* ci */ NULL, kthread_workqueue_kth_thread, (void *)0x42U, &thread, "sandbox_thread");
	if (error) {
		log(LOG_ERR, "unable to create kernel thread: error %d\n", error);
		return;
	}
	(void)kthread_join(thread); // 0
	// clang-format on

	__asm__("nop");
}

static struct workqueue *wq;
static struct work wk;

static void
kthread_workqueue_wq_cb(struct work *wk_, void *arg)
{
	__asm__("nop");
}

static void
kthread_workqueue_wq(void)
{
	// clang-format off
	// workqueue_size(): COHERENCY_UNIT 64 の倍数に繰り上げ + 余分な coherency_unit (COHERENCY_UNIT)
	//   を kmem_zalloc() して、coherency_unit にalignmentするように roundup (そのための ↑ + 余分な coherency_unit)
	// wq->wq_prio = PRI_NONE
	// wq->wq_func = workqueue_cb;
	// wq->wq_arg = (void *)(0x42);
	// WQ_MPSAFE -> KTHREAD_MPSAFE
	// PRI_NONE < PRI_KERNEL -> KTHREAD_TS
	//
	int error = workqueue_create(&wq, "sandbox_workq", kthread_workqueue_wq_cb, (void *)(0x42U), PRI_NONE, IPL_NONE, WQ_MPSAFE /* | WQ_PERCPU でCPU毎にqueueを作成 */);
	if (error != 0) {
		log(LOG_ERR, "failed to create workqueue: %d\n", error);
		return;
	}
	// clang-format on

	workqueue_enqueue(wq, &wk, NULL);
	workqueue_wait(wq, &wk);
	workqueue_destroy(wq);

	__asm__("nop");
}

static void
kthread_workqueue(void)
{
	kthread_workqueue_kth();
	kthread_workqueue_wq();
}

// -----------------------------------------------------------------------------
// wataash_sandbox - locking
// TODO: https://www.netbsd.org/gallery/presentations/riastradh/asiabsdcon2017/mp-refs-paper.pdf

static void
locking_atomic_ops(void)
{
	// clang-format off

	// atomic_ops(3)
	// https://man.netbsd.org/atomic_ops.3

	// https://man.netbsd.org/atomic_add.3
	volatile unsigned int	u_1 = 0,	u_2;
	volatile unsigned long	ul_1 = 0,	ul_2;
	volatile uint32_t	u32_1 = 0,	u32_2;
	volatile uint64_t	u64_1 = 0,	u64_2;
	volatile size_t		p_1 = 0x0,	*p_2; // size-of-pointer type; size_t でいいのか？ ptrdiff_t のように ptrsize_t みたいの無いか？

	atomic_add_32(&u32_1, (int32_t)1);		// ALIAS(atomic_add_32,_atomic_add_32)		ENTRY(_atomic_add_32)		ALIAS(atomic_add_int,_atomic_add_32)		-> symbol: atomic_add_int
	atomic_add_int(&u_1, 1);				// ALIAS(atomic_add_int,_atomic_add_32)		ENTRY(_atomic_add_32)
	// => 0xffffffff81801d20 <+0>:	f0 01 37	lock add %esi,(%rdi)	// *rdi *&u32_1 += esi 1

	u32_2 = atomic_add_32_nv(&u32_1, (int32_t)1);	// ALIAS(atomic_add_32_nv,_atomic_add_32_nv)	ENTRY(_atomic_add_32_nv)	ALIAS(atomic_add_int_nv,_atomic_add_32_nv)	-> symbol: atomic_add_int_nv
	u_2 = atomic_add_int_nv(&u_1, 1);		// ALIAS(atomic_add_int_nv,_atomic_add_32_nv)	ENTRY(_atomic_add_32_nv)
	// => 0xffffffff81801d30 <+0>:	89 f0	mov    %esi,%eax	// eax = esi 1
	//    0xffffffff81801d32 <+2>:	f0 0f c1 07	lock xadd %eax,(%rdi)	// *rdi *&u32_1 += eax 1
	//    0xffffffff81801d36 <+6>:	01 f0	add    %esi,%eax	// eax 1 += esi 1; return eax 2

	atomic_add_64(&u64_1, (int64_t)1);		// ALIAS(atomic_add_64,_atomic_add_64)		ENTRY(_atomic_add_64)		ALIAS(atomic_add_ptr,_atomic_add_64)		-> symbol: atomic_add_ptr
	atomic_add_long(&ul_1, 1L);			// ALIAS(atomic_add_long,_atomic_add_64)	ENTRY(_atomic_add_64)		ALIAS(atomic_add_ptr,_atomic_add_64)		-> symbol: atomic_add_ptr
	atomic_add_ptr(&p_1, (ssize_t)0x1);		// ALIAS(atomic_add_ptr,_atomic_add_64)		ENTRY(_atomic_add_64)
	//    => 0xffffffff81801df0 <+0>:	f0 48 01 37	lock add %rsi,(%rdi)

	u64_2 = atomic_add_64_nv(&u64_1, (int64_t)1);	// ALIAS(atomic_add_64_nv,_atomic_add_64_nv)	ENTRY(_atomic_add_64_nv)	ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)	-> symbol: atomic_add_ptr_nv
	ul_2 = atomic_add_long_nv(&ul_1, 1L);		// ALIAS(atomic_add_long_nv,_atomic_add_64_nv)	ENTRY(_atomic_add_64_nv)	ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)	-> symbol: atomic_add_ptr_nv
//	p_2 = atomic_add_ptr_nv(&p_1, (ssize_t)0x1);	// ALIAS(atomic_add_ptr_nv,_atomic_add_64_nv)	ENTRY(_atomic_add_64_nv)
	// => 0xffffffff81801e00 <+0>:	48 89 f0	mov    %rsi,%rax
	//    0xffffffff81801e03 <+3>:	f0 48 0f c1 07	lock xadd %rax,(%rdi)
	//    0xffffffff81801e08 <+8>:	48 01 f0	add    %rsi,%rax

	// https://man.netbsd.org/atomic_and.3
	// ENTRY(_atomic_and_32)
	// 	LOCK
	// 	andl	%esi, (%rdi)

	// https://man.netbsd.org/atomic_cas.3
	// ENTRY(_atomic_cas_32)
	// 	movl	%esi, %eax
	// 	LOCK
	// 	cmpxchgl %edx, (%rdi)

	// https://man.netbsd.org/atomic_dec.3
	// ENTRY(_atomic_dec_32)
	// 	LOCK
	// 	decl	(%rdi)

	// https://man.netbsd.org/atomic_inc.3
	// ENTRY(_atomic_inc_32)
	// 	LOCK
	// 	incl	(%rdi)

	// https://man.netbsd.org/atomic_or.3
	// ENTRY(_atomic_or_32)
	// 	LOCK
	// 	orl	%esi, (%rdi)

	// https://man.netbsd.org/atomic_swap.3
	// ENTRY(_atomic_swap_32)
	// 	movl	%esi, %eax
	// 	xchgl	%eax, (%rdi)

	// clang-format on
	__asm__("nop");
}

static void
locking_membar_ops(void)
{
	// membar_ops(3)
	// https://man.netbsd.org/membar_ops.3
	// https://github.com/rmind/stdc/blob/master/c11memfences.md
	// @ref:qc-netbsd-locking-membar

	// all nop (ret) in x86_64
	membar_acquire();	// ENTRY(_membar_acquire)	ALIAS(membar_acquire,_membar_acquire)	ALIAS(membar_consumer,_membar_acquire)	-> symbol: membar_consumer
	membar_release();	// ENTRY(_membar_release)	ALIAS(membar_release,_membar_release)
	membar_consumer();	// ENTRY(_membar_acquire)	ALIAS(membar_consumer,_membar_acquire)
	membar_producer();	// ENTRY(_membar_release)	ALIAS(membar_producer,_membar_release)	ALIAS(membar_release,_membar_release)	-> symbol: membar_release
	(void)membar_datadep_consumer();	// !__HAVE_MEMBAR_DATADEP_CONSUMER -> (void)0
	membar_sync();		// ENTRY(_membar_sync)	ADDQ is cheaper than MFENCE,
	// => 0xffffffff81817850 <+0>:	f0 48 83 44 24 f8 00	lock addq $0x0,-0x8(%rsp)
}

static void
locking_atomic_loadstore(void)
{
	// atomic_loadstore(9)
	// https://man.netbsd.org/atomic_loadstore.9

	int i, i2;

	i2 = atomic_load_relaxed(&i);
	i2 = atomic_load_acquire(&i); // -> membar_acquire()
	i2 = atomic_load_consume(&i); // -> membar_datadep_consumer()
	i2 = atomic_load_relaxed(&i);
	atomic_store_relaxed(&i, 1);
	atomic_store_release(&i, 1); // -> membar_release()

	__asm__("nop");
}

static void
locking_rwlock(void)
{
	// rwlock(9)
	// https://man.netbsd.org/rwlock.9

	krwlock_t rw;

	rw_init(&rw);	// rw->rw_owner = 0

	(void)rw_lock_held(&rw);	// 0

	rw_enter(&rw, RW_READER);	// rw_vector_enter: rw.rw_owner -> RW_READ_INCR 0x20
	// (void)RW_COUNT(rw) >> RW_READ_COUNT_SHIFT;	// 1 ((N..5] owner or read count)
	(void)rw_lock_held(&rw);	// true (1) ((rw->rw_owner & RW_THREAD) != 0)
	(void)rw_read_held(&rw);	// true (1) (owner & RW_WRITE_LOCKED) == 0 && (owner & RW_THREAD) != 0 -> 1
	(void)rw_write_held(&rw);	// 0

	// rw_enter(&rw, RW_READER);	// locking against myself

	rw_exit(&rw);			// rw.rw_owner = 0

	rw_enter(&rw, RW_WRITER);	// rw.rw_owner = l & RW_WRITE_LOCKED 0x04
	(void)rw_lock_held(&rw);	// 1
	(void)rw_read_held(&rw);	// 0
	(void)rw_write_held(&rw);	// 1

	rw_exit(&rw);			// rw.rw_owne = 0

	(void)rw_tryenter;
	(void)rw_tryupgrade;
	(void)rw_downgrade;

	rw_destroy(&rw);

	__asm__("nop");
}

static void
locking_mutex(void)
{
	// mutex(9)
	// https://man.netbsd.org/mutex.9

	kmutex_t mutex;

	// see also: sys/arch/amd64/amd64/lock_stubs.S
	mutex_init(&mutex, MUTEX_DEFAULT, IPL_VM);	// IPL_VM -> MUTEX_INITIALIZE_SPI ((void)mutex.mtx_owner MUTEX_BIT_SPIN; MUTEX_ADAPTIVE_P false, MUTEX_SPIN_P true), -> MUTEX_SPINBIT_LOCK_INIT: ENTRY(__cpu_simple_lock_init)
	mutex_enter(&mutex);	// mutex_vector_enter() -> __C: ENTRY(__cpu_simple_lock_try): lock cmpxchg %ah,(%rdi); 失敗したら SPINLOCK_BACKOFF -> SPINLOCK_BACKOFF_HOOK: x86_pause()
	// mutex_enter(&mutex);	// panic
	mutex_exit(&mutex);
	// mutex_exit(&mutex);	// panic
	mutex_spin_enter(&mutex);	// same as mutex_enter; スピンロックであると明示したいとき用？
	mutex_spin_exit(&mutex);	// same as mutex_exit; スピンロックであると明示したいとき用？
	mutex_destroy(&mutex);

	mutex_init(&mutex, MUTEX_DEFAULT, IPL_NONE);	// IPL_NONE -> MUTEX_INITIALIZE_ADAPTIVE ((void)mutex.mtx_owner MUTEX_BIT_SPIN なし; MUTEX_ADAPTIVE_P true, MUTEX_SPIN_P false)
	mutex_enter(&mutex);	// mutex_vector_enter() -> MUTEX_ACQUIRE -> MUTEX_CAS: atomic_cas_ulong: lock cmpxchg %rdx,(%rdi)
	// mutex_enter(&mutex);	// panic
	mutex_exit(&mutex);
	// mutex_exit(&mutex);	// panic
	mutex_destroy(&mutex);

	__asm__("nop");
}

static void
locking_condvar(void)
{
	// condvar(9)
	// https://man.netbsd.org/condvar.9

	kmutex_t mutex;
	kcondvar_t cv;

	mutex_init(&mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_enter(&mutex);

	cv_init(&cv, "sandbox_cv");
	cv_has_waiters(&cv);	// false
	// cv_wait(&cv, &mutex);	// pause forever (-> sleepq_block -> mi_switch -> cpu_switchto never return back)
	cv_signal(&cv);		// TODO: mutex_enter() してないとだめかも
	cv_broadcast(&cv);	// TODO: mutex_enter() してないとだめかも
	// cv_wait(&cv, &mutex);	// pause forever
	cv_destroy(&cv);

	mutex_exit(&mutex);
	mutex_destroy(&mutex);

	__asm__("nop");
}

// xc_thread ->
static void
xc_(void *a, void *b)
{
	__asm__("nop");
} // -> cv_broadcast

static void
locking_xcall(void)
{
	// xcall(9)
	// https://man.netbsd.org/percpu.9

	// clang-format off
	uint64_t xc = xc_broadcast(0 /* or XC_HIGHPRI or XC_HIGHPRI_IPL(ipl) */, xc_, (void *)0x42U, (void *)0x43U);	// -> cv_signal
	xc_wait(xc);	// -> cv_wait
	// clang-format on

	xc_barrier(0 /* or XC_HIGHPRI or XC_HIGHPRI_IPL(ipl) */);

	__asm__("nop");
}

// vpoolp vokp
static void
locking_percpu_cb(void *v1, void *v2, struct cpu_info *ci)
{
	// v1: ?
	// v2: 0x42
	__asm__("nop");
}

static void
locking_percpu(void)
{
	// percpu(9)
	// https://man.netbsd.org/percpu.9

	percpu_t *sandbox_percpu = percpu_alloc(sizeof(int));
	(void)percpu_create;

	// clang-format off
	percpu_foreach(sandbox_percpu, locking_percpu_cb, (void *)0x42U);	// runs in the current thread
	percpu_foreach_xcall(sandbox_percpu, XC_HIGHPRI_IPL(IPL_SOFTNET), locking_percpu_cb, (void *)0x42U);	// executes on the remote CPUs
	// clang-format on

	int *tmp = percpu_getref(sandbox_percpu);
	(void)kpreempt_disabled();	// 1
	percpu_putref(sandbox_percpu);

	percpu_free(sandbox_percpu, sizeof(int));

	__asm__("nop");
}

static void
locking_ras(void)
{
	// ras(9)
	// https://man.netbsd.org/ras.9

	struct proc *p = curproc;
	void *tmp = ras_lookup(p, NULL);
	(void)ras_fork;
	(void)ras_purgeall;
}

static void
locking_pserialize(void)
{
	// pserialize(9)
	// https://man.netbsd.org/pserialize.9


	int s = pserialize_read_enter();	// splsoftserial() した結果、pserialize_perform() -> xc_barrier() のnopが実行されなくなる 多分
	pserialize_in_read_section();		// true
	pserialize_not_in_read_section();	// false
	pserialize_read_exit(s);		// splx() で xc_barrier() のnopが実行される 多分

	pserialize_t psz = pserialize_create();	// actually not needed -- just for compatibility

	pserialize_perform(psz);	// -> xc_barrier(XC_HIGHPRI)

	s = pserialize_read_enter();
	// pserialize_perform(psz);	// xc_barrier(XC_HIGHPRI) -> xc_broadcast() -> ASSERT_SLEEPABLE() panic: assert_sleepable: pserialize caller=0xffffffff81561038
	pserialize_read_exit(s);

	pserialize_destroy(psz);

	__asm__("nop");
}

static void
locking_psref(void)
{
	// psref(9)
	// https://man.netbsd.org/psref.9
	// sleepable pserialize?

	struct psref_class *psref_class_ /* __read_mostly */ =
	    psref_class_create("sandbox_psref", IPL_SOFTNET);
	// -> percpu_alloc()
	// vmem の引越しで重い場合があるので初期化時に1回だけ呼ぶのが良い

	struct psref_target sandbox_psref;
	psref_target_init(&sandbox_psref, psref_class_);

	int bound = curlwp_bind();

	(void)psref_held(&sandbox_psref, psref_class_); // 0

	struct sandbox_softc {
		void *data;
		struct psref psref;
	} sc = {}, sc2 = {};

	// pserialize sleep できない; とはいえしたいこともある

	// rwlock は遅い; cmpxchg で 他のCPUに影響を与える invalidate cache
	// pserialize psref だと cpu local なので速い
	// rwlock の reader の不可を writer に押し付けている
	// readmostly なときに良い

	// cpu0: SLIST psref psref psref...
	// cpu1: a

	int s = pserialize_read_enter(); // sleepしたい ← psrefは ほぼこの用途

	// reader: cmpxchg をする rwlock と違い percpu なので、cache の
	// invalidation をしなかったりして速い (lockless)
	psref_acquire(&sc.psref, &sandbox_psref, psref_class_);
	psref_acquire(&sc2.psref, &sandbox_psref, psref_class_);
	pserialize_read_exit(s);
	// pserialize と違いここから sleep できる; sleep してる間もwriteされない
	ASSERT_SLEEPABLE();
	// sleep してる間に cpu migration を起こさないように curlwp_bind()
	// が必要

	(void)psref_held(&sandbox_psref, psref_class_); // true

	(void)sc.data;  // read
	(void)sc2.data; // read

	s = pserialize_read_enter();
	psref_release(&sc.psref, &sandbox_psref, psref_class_);
	psref_release(&sc2.psref, &sandbox_psref, psref_class_);
	pserialize_read_exit(s);

	(void)psref_held(&sandbox_psref, psref_class_);	// false

	curlwp_bindx(bound);

	// writer
	psref_target_destroy(&sandbox_psref, psref_class_);
	sc.data = (void *)0x42U;
	sc2.data = (void *)0x42U;

	psref_class_destroy(psref_class_);

	(void)psref_copy;
}

static void
locking_pslist(void)
{
	// pslist(9)
	// https://man.netbsd.org/pslist.9

	struct pslist_head sandbox_pslist;

	PSLIST_INIT(&sandbox_pslist);

	// TODO

	PSLIST_DESTROY(&sandbox_pslist);

	__asm__("nop");
}

static void
locking_localcount(void)
{
	// localcount(9)
	// https://man.netbsd.org/localcount.9

	kcondvar_t cv;
	cv_init(&cv, "sandbox_lc_cv");

	kmutex_t mutex;
	mutex_init(&mutex, MUTEX_DEFAULT, IPL_NONE);

	struct localcount lc;
	localcount_init(&lc);

	localcount_acquire(&lc);		// +1
	localcount_release(&lc, &cv, &mutex);	// -1

	mutex_enter(&mutex);
	localcount_drain(&lc, &cv, &mutex);	// wait for 0
	mutex_exit(&mutex);

	localcount_fini(&lc);
	mutex_destroy(&mutex);
	cv_destroy(&cv);

	__asm__("nop");
}

// @ref:qc-netbsd-locking
static void
locking(void)
{
	locking_atomic_ops();
	locking_membar_ops();
	locking_atomic_loadstore();	// depends: membar_ops

	locking_rwlock();
	locking_mutex();
	locking_condvar();	// depends: mutex
	locking_xcall();	// depends: condvar
	locking_percpu();	// depends: kmem vmem rwlock xcall
	locking_ras();		// depends: kmem mutex xcall
	locking_pserialize();	//
	locking_psref();	// depends: kmem mutex condvar xcall percpu pserialize
	locking_pslist();	// depends: atomic_loadstore pserialize
	locking_localcount();	// depends: atomic_ops mutex condvar call percpu
}

// -----------------------------------------------------------------------------
// wataash_sandbox - macros

static bool power_of_two(size_t i) {
	return powerof2(i);

	if (i == 0)
		return true;
	for (;;) {
		if (i == 1)
			return true;
		if (i % 2 != 0)
			return false;
		i /= 2;
	}
	/* NOTREACHED */
}

static void
macros(void)
{
	CTASSERT(1 == 1);

	// roundup() rounddown() roundup2()

	// y:4
	//
	// |--+--+--+--|--+--+--+--|--  x
	// 0  1  2  3  4  5  6  7  8
	//               <->
	//                  roundup(5,4): 8
	// rounddown(5,4): 4

	// roundup(x, y): x を up して y の倍数にする
	// x\y  1  2  3  4  5  6  7  8 .
	// (0)  0  0  0  0  0  0  0  0 .
	// 1    1  2  3  4  5  6  7  8 .
	// 2    2  2  3  4  5  6  7  8 .
	// 3    3  4  3  4  5  6  7  8 .
	// 4    4  4  6  4  5  6  7  8 .
	// 5    5  6  6  8  5  6  7  8 .
	// 6    6  6  6  8 10  6  7  8 .
	// 7    7  8  9  8 10 12  7  8 .
	// 8    8  8  9  8 10 12 14  8 .
	(void)roundup(1, 1);

	// rounddown(x, y): x を down して y の倍数にする
	// x\y   1  2  3  4  5  6  7  8 .
	// (0)   0  0  0  0  0  0  0  0 .
	// 1     1  0  0  0  0  0  0  0 .
	// 2     2  2  0  0  0  0  0  0 .
	// 3     3  2  3  0  0  0  0  0 .
	// 4     4  4  3  4  0  0  0  0 .
	// 5     5  4  3  4  5  0  0  0 .
	// 6     6  6  6  4  5  6  0  0 .
	// 7     7  6  6  4  5  6  7  0 .
	// 8     8  8  6  8  5  6  7  8 .
	(void)rounddown(1, 1);

	// roundup2()
	(void)roundup2(1, 1);
	// valid only if m is {1,2,4,8,16,...}
	//           m   x 0  1  2  3  4  5  6  7  8  9
	// (invalid) 0     0  0  0  0  0  0  0  0  0  0
	//           1     0  1  2  3  4  5  6  7  8  9
	//           2     0  2  2  4  4  6  6  8  8 10
	// (invalid) 3     0  3  4  3  4  7  8  7  8 11
	//           4     0  4  4  4  4  8  8  8  8 12
	// (invalid) 5     0  5  6  7  8  5  6  7  8 13
	// (invalid) 6     0  6  6  8  8  6  6  8  8 14
	// (invalid) 7     0  7  8  7  8  7  8  7  8 15
	//           8     0  8  8  8  8  8  8  8  8 16
	// (invalid) 9     0  9 10 11 12 13 14 15 16  9
	// valid only:
	//  m   x 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
	//  1     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
	//  2     0  2  2  4  4  6  6  8  8 10 10 12 12 14 14 16 16
	//  4     0  4  4  4  4  8  8  8  8 12 12 12 12 16 16 16 16
	//  8     0  8  8  8  8  8  8  8  8 16 16 16 16 16 16 16 16
	// 16     0 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16
	// 32     0 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32
	// 64     0 64 64 64 64 64 64 64 64 64 64 64 64 64 64 64 64
	printf("roundup2()\n");
	printf("valid only if m is {1,2,4,8,16,...}\n");
	printf("          m   x 0  1  2  3  4  5  6  7  8  9\n");
	for (size_t m = 0; m <= 9; m++) {
		if (m != 0 && power_of_two(m))
			printf("         ");
		else
			printf("(invalid)");
		printf("%2zu   ", m);
		for (size_t x = 0; x <= 9; x++) {
			__auto_type tmp2 = roundup2(x, m);
			printf("%3lu", roundup2(x, m));
		}
		printf("\n");
	}
	printf("valid only:\n");
	printf(" m   x 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16\n");
	for (size_t m = 0; m <= 64; m++) {
		if (m == 0 || !power_of_two(m))
			continue;
		printf("%2zu   ", m);
		for (size_t x = 0; x <= 16; x++) {
			__auto_type tmp2 = roundup2(x, m);
			printf("%3lu", roundup2(x, m));
		}
		printf("\n");
	}

	// rounddown2()
	// TODO
	(void)rounddown2(1, 1);

	__asm__("nop");
}

// -----------------------------------------------------------------------------
// wataash_sandbox - mbuf
// https://man.netbsd.org/mbuf.9
// http://www.nerv.org/~ryo/files/mbuf/mbuf.html

static void mbuf_ipsec_mbuf(void);
static void mbuf_mtag(void);

static void
mbuf_(void)
{
	static const unsigned char deadbeef[] = {0xde, 0xad, 0xbe, 0xef};
	struct mbuf *m;

	// https://man.netbsd.org/mbuf.9

	// m_free(m);
	// // http://www.wdic.org/w/TECH/mbuf 前のmbuf領域から次のmbuf領域へのポインターは自動的に変更される。
	//
	// m_freem(m);
	// // http://www.wdic.org/w/TECH/mbuf mbuf領域mから、チェーンの最後までをまとめて解放する。

	// struct mbuf *
	// m_get(int how, int type);
	//
	// MBUF(0) 0xffff8ee3ec07c038
	//   data=0xffff8ee3ec07c070, len=0, type=1, flags=0
	//   data:
	//   owner=0xffffffff80eee498, next=0x0, nextpkt=0x0
	//   leadingspace=0, trailingspace=456, readonly=0
	m = m_get(M_WAIT, MT_DATA); // MGET(m, M_WAIT, MT_DATA);
	if (m == NULL) {}
	wataash_m_print(m);
	m_freem(m);

	// MBUF(0) 0xffff8ee3ec07c038
	//   data=0xffff8ee3ec07c070, len=0, type=1, flags=0x2<PKTHDR>
	//   data:
	//   owner=0xffffffff80eee498, next=0x0, nextpkt=0x0
	//   leadingspace=0, trailingspace=400, readonly=0
	//   pktlen=0, rcvif=0x0, csum_flags=0, csum_data=0x0, segsz=0
	m = m_gethdr(M_WAIT, MT_DATA); // MGETHDR(m, M_WAIT, MT_DATA); // m_get() + M_PKTHDR
	if (m == NULL) {}
	wataash_m_print(m);
	m_freem(m);

	// m_get_n() m_gethdr_n()
	// not documented yet
	// https://github.com/Netbsd/src/commit/f677aab4f1275de25c0401a8258415d5dba7267e
	// m_get_n(int how, int type, size_t alignbytes, size_t nbytes)
	{
		m = m_get_n(M_WAIT, MT_DATA, ETHER_ALIGN /* 2 */ , 3); // len=4 data: 00 00 00 00
		if (m == NULL) {}
		wataash_m_print(m);
		m_freem(m);

		m = m_get_n(M_WAIT, MT_DATA, 0, 3); // len=3 data: 00 00 00
		if (m == NULL) {}
		wataash_m_print(m);
		m_freem(m);
	}

	// struct mbuf *
	// m_gethdr(int how, int type);
	m = m_gethdr(M_WAIT, MT_DATA);
	if (m == NULL) {}

	// struct mbuf *
	// m_devget(char *buf, int totlen, int off, struct ifnet *ifp);

	// struct mbuf *
	// m_copym(struct mbuf *m, int off, int len, int wait);

	// struct mbuf *
	// m_copypacket(struct mbuf *m, int how);

	// void
	// m_copydata(struct mbuf *m, int off, int len, void *cp);
	// m -> buf

	// mbuf(9) では void * だが実際は const void *
	//   TODO: fix
	// void
	// m_copyback(struct mbuf *m0, int off, int len, const void *cp);
	//
	// (empty)
	wataash_m_print(m);
	//
	m_copyback(m, 16, sizeof(deadbeef), deadbeef);
	//  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
	//  de ad be ef
	wataash_m_print(m);
	//
	m_copyback(m, 0, sizeof(deadbeef), deadbeef);
	//  de ad be ef 00 00 00 00 00 00 00 00 00 00 00 00
	//  de ad be ef
	wataash_m_print(m);

	// mbuf(9) では void * だが実際は const void *
	//   TODO: fix
	// struct mbuf *
	// m_copyback_cow(struct mbuf *m0, int off, int len, const void *cp, int how);

	// int
	// m_makewritable(struct mbuf **mp, int off, int len, int how);

	// void
	// m_cat(struct mbuf *m, struct mbuf *n);
	// http://www.wdic.org/w/TECH/mbuf mbufチェーンnを、mbuf領域mの後に挿入連結する。mとnのtypeは同一でなければならない。

	// struct mbuf *
	// m_dup(struct mbuf *m, int off, int len, int wait);

	// struct mbuf *
	// m_pulldown(struct mbuf *m, int off, int len, int *offp);

	// struct mbuf *
	// m_pullup(struct mbuf *n, int len);
	// http://www.wdic.org/w/TECH/mbuf
	// mbuf領域mのデータ領域から、lenバイトを連続させて一つのmbufに格納するようデータをコピーする。mtod()を使うときに使用する。
	// 当然、一つのmbufに収まるサイズでなければならない。具体的には、lenはMHLEN(mbufのデータ部の最大長)未満でなければならない。

	// struct mbuf *
	// m_copyup(struct mbuf *m, int len, int dstoff);

	// struct mbuf *
	// m_split(struct mbuf *m0, int len, int wait);

	// void
	// m_adj(struct mbuf *mp, int req_len);
	// http://www.wdic.org/w/TECH/mbuf lenが正の値ならmbuf領域mを先頭としたmbufチェーンの先頭からlenバイトを取り除き目的位置を頭出しし、さもなくば末尾から切り取る。

	// int
	// m_apply(struct mbuf *m, int off, int len,
	//     int *f(void *, void *, unsigned int), void *arg);

	// struct mbuf *
	// m_free(struct mbuf *m);

	// void
	// m_freem(struct mbuf *m);

	// datatype
	// mtod(struct mbuf *m, datatype);

	// void
	// MGET(struct mbuf *m, int how, int type);

	// void
	// MGETHDR(struct mbuf *m, int how, int type);

	// void
	// MEXTMALLOC(struct mbuf *m, int len, int how);

	// void
	// MEXTADD(struct mbuf *m, void *buf, int size, int type,
	//     void (*free)(struct mbuf *, void *, size_t, void *), void *arg);

	// void
	// MCLGET(struct mbuf *m, int how);

	// void
	// m_copy_pkthdr(struct mbuf *to, struct mbuf *from);

	// void
	// m_move_pkthdr(struct mbuf *to, struct mbuf *from);

	// void
	// m_remove_pkthdr(struct mbuf *m);

	// void
	// m_align(struct mbuf *m, int len);

	// int
	// M_LEADINGSPACE(struct mbuf *m);

	// int
	// M_TRAILINGSPACE(struct mbuf *m);

	// void
	// M_PREPEND(struct mbuf *m, int plen, int how);

	// void
	// MCHTYPE(struct mbuf *m, int type);

	m_freem(m);

	mbuf_ipsec_mbuf();
	mbuf_mtag();
}

// ipsec_mbuf.c
static void
mbuf_ipsec_mbuf(void)
{
	size_t i;
	struct mbuf *m;

	MGETHDR(m, M_WAIT, MT_DATA);
	if (m == NULL) {}
	wataash_m_print(m);
	for (i = 0; i < 256; i++) {
		unsigned char u = (unsigned char)i;
		m_copyback(m, (int)i, 1, (unsigned char *)&u);
	}
	wataash_m_print(m);

	// 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
	// 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f < inserted (uninitialized)
	// 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
	// 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f v stripped
	// 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f ^
	// 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f
	// 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f
	// 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f
	// 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f
	// 90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f
	// a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af
	// b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf
	// c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf
	// d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df
	// e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef
	// f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff

	int off;
	m_makespace(m, 16, 16, &off);
	wataash_m_print(m);

	m_striphdr(m, 64, 16);
	wataash_m_print(m);

	m_freem(m);
}

static void
mbuf_mtag(void)
{
	struct mbuf *m;

	MGETHDR(m, M_WAIT, MT_DATA);

	// not tested; TODO: test
if (0) {
	(void)if_tunnel_check_nesting; // ref
	struct m_tag *mtag;
	int *intval;
	mtag = m_tag_get(PACKET_TAG_TUNNEL_INFO, sizeof(*intval), M_NOWAIT);
	if (mtag == NULL) {}
	m_tag_prepend(m, mtag);
	intval = (int *)(mtag + 1);
	*intval = 0;
	mtag = m_tag_find(m, PACKET_TAG_TUNNEL_INFO); // found
	__asm__("nop");
}

	// https://man.netbsd.org/m_tag.9

	// struct m_tag *
	// m_tag_get(int type, int len, int wait);
	//
	// void
	// m_tag_free(struct m_tag *t);
	//
	// void
	// m_tag_prepend(struct mbuf *m, struct m_tag *t);
	//
	// void
	// m_tag_unlink(struct mbuf *m, struct m_tag *t);
	//
	// void
	// m_tag_delete(struct mbuf *m, struct m_tag *t);
	//
	// void
	// m_tag_delete_chain(struct mbuf *m);
	//
	// struct m_tag *
	// m_tag_find(struct mbuf *m, int type);
	//
	// struct m_tag *
	// m_tag_copy(struct m_tag *m);
	//
	// int
	// m_tag_copy_chain(struct mbuf *to, struct mbuf *from);
	m_freem(m);
}

// -----------------------------------------------------------------------------
// wataash_sandbox - mem

static void
mem(void)
{
	// https://man.netbsd.org/memoryallocators.9

	// pool(9) depends: todo
	// https://man.netbsd.org/pool.9

	struct pool pool_;

	// clang-format off
	// &pool_ は TAILQ pool_head に加えられる (pr_wchan 順にソート)
	// &pool_ は pool_allocator_kmem->pa_list に加えられる
	// prsize sizeof(int) 4 < sizeof(struct pool_item) 24 -> prsize = sizeof(struct pool_item)
	//   pool_redzone_init(): pp->pr_size 24 - requested_size 4 >= POOL_REDZONE_SIZE 2
	//   pool_init_is_phinpage(): pp->pr_size 24 < MIN(pagesize 4096 / 16, PHSIZE 56 * 8) 256 -> true
	//   pp->pr_itemsperpage = itemspace 4040 / pp->pr_size 24 -> 168   size 24 なら1ページあたり168個入る
	pool_init(
	    &pool_,
	    sizeof(int),
	    /* align */ 0, /* -> ALIGN(1) -> 8 */
	    /* ioff */ 0,
	    0, /* or PR_NOTOUCH or PR_PSERIALIZE */
	    "sandbox_pool",
	    NULL, /* -> pool_allocator_kmem */
	    IPL_NONE);
	// clang-format on
	(void)pool_allocator_kmem;
	(void)pool_allocator_nointr;
	(void)pool_allocator_meta;

	// clang-format off
	// without pool_put():
	// (gdb) p -pretty on -- pool_
	//											pool_.pr_itemsperpage - 1	pool_.pr_itemsperpage	pool_.pr_itemsperpage + 1
	// 				init	after 1st pool_get()	2nd			167th				168th			169th
	// item					0xffffa08002b7ffe0	0xffffa08002b7ffc8	0xffffa08002b7f050		0xffffa08002b7f038	0xffffa08002bc0fe8
	// pool_.pr_fullpages.lh_first												0xffffa08002b7f000
	// pool_.pr_partpages.lh_first	NULL	0xffffa08002b7f000								NULL			0xffffa08002bc0000
	// pool_.pr_curpage		NULL	0xffffa08002b7f000								NULL			0xffffa08002bc0000
	// pool.pr_npages		0	1													2
	// pool.pr_nitems		0	167			166			1				0			167
	// pool.pr_nout			0	1			2			167				168			169
	// pool.pr_curcolor		0	8													0
	// pool.pr_nget			0	1			2			167				168			169
	// pool.pr_npagealloc		0	1													2
	// pool.pr_hiwat		0	1													2
	(void)(pool_.pr_curpage == NULL);	// pool_get -> pool_grow -> pool_allocator_alloc -> pool_page_alloc -> uvm_km_kmem_alloc
	(void)(pool_.pr_nitems == 0);
	// clang-format on
	int *item;
	for (size_t i = 0; i < pool_.pr_itemsperpage - 1; i++) {
		item = pool_get(&pool_, PR_WAITOK | PR_ZERO);
		pool_put(&pool_, item);
		__asm__("nop");
	}
	// clang-format off
	item = pool_get(&pool_, PR_WAITOK | PR_ZERO);	// 168th pool_.pr_itemsperpage
	pool_put(&pool_, item);
	item = pool_get(&pool_, PR_WAITOK | PR_ZERO);	// 169th pool_.pr_itemsperpage + 1
	pool_put(&pool_, item);
	// clang-format on

	pool_destroy(&pool_);	// without pool_put(): pp->pr_nout != 0 -> panic

	// pool_cache(9) depends: todo
	// https://man.netbsd.org/pool_cache.9

	// clang-format off
	pool_cache_t sandbox_cache = pool_cache_init(
	    sizeof(int),
	    /* align */ 0, /* -> ALIGN(1) -> 8 */
	    /* align_offset */ 0,
	    /* flags */ 0, /* or PR_NOTOUCH or PR_PSERIALIZE */
	    "sandbox_pool_cache",
	    NULL, /* IPL_NONE && large -> pool_allocator_big[N]; else -> pool_allocator_nointr */
	    IPL_NONE,
	    NULL, /* -> NO_CTOR nullop */
	    NULL, /* -> NO_DTOR nullop */
	    NULL /* passed to ctor/dtor */);	// -> pool_get(&cache_pool, PR_WAITOK);
	// clang-format on
	(void)pool_allocator_kmem;
	(void)pool_allocator_nointr;
	(void)pool_allocator_meta;

	item = pool_cache_get(sandbox_cache, PR_WAITOK | PR_ZERO);
	pool_cache_put(sandbox_cache, item);
	item = pool_cache_get(sandbox_cache, PR_WAITOK | PR_ZERO);
	pool_cache_put(sandbox_cache, item);

	pool_cache_destroy(sandbox_cache);

	// kmem(9) depends: todo
	// https://man.netbsd.org/kmem.9
	// https://man.netbsd.org/malloc.9 obsoleted

	// sizeof(*p) 4 -kmem_roundup_size()-> 8 (KMEM_ALIGN: 8)
	// KM_SLEEP/KM_NOSLEEP -> PR_WAITOK/PR_NOWAIT
	// clang-format off
	int *p = kmem_zalloc(sizeof(*p), KM_SLEEP /* or KM_NOSLEEP */); // -> kmem_intr_zalloc -> kmem_intr_alloc -> pool_cache_get
	// clang-format on
	kmem_free(p, sizeof(*p));
	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	kmem_free(p, sizeof(*p));

	// vmem(9) depends: pool pool_cache kmem mutex condvar
	// https://man.netbsd.org/vmem.9

	struct vmem *sandbox_vmem = vmem_create(
	    "sandbox_vmem",
	    /* base */ 0,
	    /* size */ 1024,
	    /* quantum */ 64,
	    /* allocfn freefn arg */ NULL, NULL, NULL,
	    /* qcache_max */ 0,
	    VM_SLEEP, IPL_NONE);	// vmem_add(vm, 0, 1024, flags)
	(void)vmem_xcreate;	// __FPTRCAST(vmem_import_t *, importfn) (&actualsize), flags | VM_XIMPORT
	(void)vmem_add;
	vmem_addr_t va;
	(void)vmem_alloc(sandbox_vmem, 16, VM_NOSLEEP|VM_BESTFIT, &va);	// va: 0x0
	(void)vmem_free(sandbox_vmem, va, 16);
	(void)vmem_xalloc;
	(void)vmem_xfree;
	(void)vmem_xfreeall;
	vmem_destroy(sandbox_vmem);

	__asm__("nop");
}

// -----------------------------------------------------------------------------
// wataash_sandbox - net

static void net_radix(void);

static void
net(void)
{
	net_radix();
}

// radix.c:61
typedef void (*rn_printer_t)(void *, const char *fmt, ...);

// radix.c:73
static int rn_satisfies_leaf(const char *, struct radix_node *, int);
static int rn_lexobetter(const void *, const void *);
static struct radix_mask *rn_new_radix_mask(struct radix_node *,
					    struct radix_mask *);
static struct radix_node *rn_walknext(struct radix_node *, rn_printer_t,
				      void *);
static struct radix_node *rn_walkfirst(struct radix_node *, rn_printer_t,
				       void *);
static void rn_nodeprint(struct radix_node *, rn_printer_t, void *,
			 const char *);

#define	SUBTREE_OPEN	"[ "
#define	SUBTREE_CLOSE	" ]"

// radix.c:344
static void
rn_nodeprint(struct radix_node *rn, rn_printer_t printer, void *arg,
    const char *delim)
{
	(*printer)(arg, "%s(%s%p: p<%p> l<%p> r<%p>)",
	    delim, ((void *)rn == arg) ? "*" : "", rn, rn->rn_p,
	    rn->rn_l, rn->rn_r);
}

// radix.c:356
static void
rn_dbg_print(void *arg, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

// radix.c:366
static void
rn_treeprint(struct radix_node_head *h, rn_printer_t printer, void *arg)
{
	struct radix_node *dup, *rn;
	const char *delim;

	if (printer == NULL)
		return;

	rn = rn_walkfirst(h->rnh_treetop, printer, arg);
	for (;;) {
		/* Process leaves */
		delim = "";
		for (dup = rn; dup != NULL; dup = dup->rn_dupedkey) {
			if ((dup->rn_flags & RNF_ROOT) != 0)
				continue;
			rn_nodeprint(dup, printer, arg, delim);
			delim = ", ";
		}
		rn = rn_walknext(rn, printer, arg);
		if (rn->rn_flags & RNF_ROOT)
			return;
	}
	/* NOTREACHED */
}

// radix.c:946
static struct radix_node *
rn_walknext(struct radix_node *rn, rn_printer_t printer, void *arg)
{
	/* If at right child go back up, otherwise, go right */
	while (rn->rn_p->rn_r == rn && (rn->rn_flags & RNF_ROOT) == 0) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_CLOSE);
		rn = rn->rn_p;
	}
	if (printer)
		rn_nodeprint(rn->rn_p, printer, arg, "");
	/* Find the next *leaf* since next node might vanish, too */
	for (rn = rn->rn_p->rn_r; rn->rn_b >= 0;) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_OPEN);
		rn = rn->rn_l;
	}
	return rn;
}

static struct radix_node *
rn_walkfirst(struct radix_node *rn, rn_printer_t printer, void *arg)
{
	/* First time through node, go left */
	while (rn->rn_b >= 0) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_OPEN);
		rn = rn->rn_l;
	}
	return rn;
}

static
int
rn_walktree_(
	struct radix_node_head *h,
	int (*f)(struct radix_node *, void *),
	void *w)
{
	int error;
	struct radix_node *base, *next, *rn;
	/*
	 * This gets complicated because we may delete the node
	 * while applying the function f to it, so we need to calculate
	 * the successor node in advance.
	 */
	rn = rn_walkfirst(h->rnh_treetop, NULL, NULL);
	for (;;) {
		base = rn;
		next = rn_walknext(rn, NULL, NULL);
		/* Process leaves */
		while ((rn = base) != NULL) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT) && (error = (*f)(rn, w)))
				return error;
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT)
			return 0;
	}
	/* NOTREACHED */
}

static int
sandbox_walktree_visitor(struct radix_node *rn, void *v)
{
	return 0;
}

#define dump_assert(cond) do { if (!(cond)) { log(LOG_ERR, "assertion failed at line %d: %s\n", __LINE__, #cond); return -1; } } while (0)

static int
dump_rn_leaf(const struct radix_node *n, unsigned int depth)
{
	dump_assert(n->rn_mklist == NULL);
	dump_assert(n->rn_b < 0);
	dump_assert(n->rn_key != NULL);
	const unsigned char *key = (const unsigned char *)&((const struct sockaddr_in *)n->rn_key)->sin_addr.s_addr;
	if (n->rn_mask == NULL)	{
		printf("%*s|b:%d bmask:0x%02x flags:0x%02x key:%d.%d.%d.%d (%p %p) dupedkey:%p self:%p\n", depth * 2, "", n->rn_b, n->rn_bmask, n->rn_flags, key[0], key[1], key[2], key[3], n->rn_key, n->rn_mask, n->rn_dupedkey, n);
		return 0;
	}
	uint32_t mask = ntohl(((const struct sockaddr_in *)n->rn_mask)->sin_addr.s_addr);
	printf("%*s|b:%d bmask:0x%02x flags:0x%02x key:%d.%d.%d.%d mask:0x%08x (%p %p) dupedkey:%p self:%p\n", depth * 2, "", n->rn_b, n->rn_bmask, n->rn_flags, key[0], key[1], key[2], key[3], mask, n->rn_key, n->rn_mask, n->rn_dupedkey, n);
	return 0;
}

static int
dump_rn_node(const struct radix_node *n, unsigned int depth)
{
	dump_assert(n->rn_b >= 0);
	const struct radix_node *l = n->rn_l;
	const struct radix_node *r = n->rn_r;
	if (r->rn_b >= 0)
		dump_assert(dump_rn_node(r, depth + 1) == 0);
	else
		dump_assert(dump_rn_leaf(r, depth + 1) == 0);
	printf("%*s|b:%d bmask:0x%02x flags:0x%02x off:%d mklist:%p self:%p\n", depth * 2, "", n->rn_b, n->rn_bmask, n->rn_flags, n->rn_off, n->rn_mklist, n);
	if (l->rn_b >= 0)
		dump_assert(dump_rn_node(l, depth + 1) == 0);
	else
		dump_assert(dump_rn_leaf(l, depth + 1) == 0);
	return 0;
}

static int
dump_rn_nodes(const struct radix_node nodes[3])
{
	unsigned int depth = 0;
	printf("nodes: %p %p %p\n", &nodes[0], &nodes[1], &nodes[2]);
	printf("dumping nodes[1]... (v left / right ^)\n");
	dump_assert(dump_rn_node(&nodes[1], depth) == 0);
	return 0;
}

// put in radix.h:
// #define RN_DEBUG
static void
net_radix(void)
{
	struct radix_node *rn;
	struct radix_node rn_pair[2] = {};
	struct radix_node_head rnh0;
	struct radix_node_head *rnh = NULL;
	extern char *rn_zeros, *rn_ones; // actually extern static

	// rn_newpair(const void *v, int b (off), struct radix_node nodes[2])
	(void)rn_newpair;

	// clang-format off
	// [0] leaf
	(void)(rn_pair[0].rn_p == &rn_pair[1]);
	(void)(rn_pair[0].rn_b == -1);				// negative なら leaf
	(void)(rn_pair[0].rn_bmask == 0);			// 0 for leaf
	(void)(rn_pair[0].rn_flags == RNF_ACTIVE);		// 4	rn_inithead0() で上書きされる
	(void)(rn_pair[0].rn_u.rn_leaf.rn_Key == (const char *)0x42);	// .rn_key	rn_inithead0() では rn_zeros

	// [1] (non-leaf) node
	(void)(rn_pair[1].rn_p == NULL);			// rn_inithead0() で self になる
	(void)(rn_pair[1].rn_b == (0 | 1 | 2));			// off 0 1 2 ...
	(void)(rn_pair[1].rn_bmask == (char)(0x80U | 0x40U | 0x20U));	// 80 40 20 10 08 04 02 01 (周期8) 80 40 ... __ 1000_0000 (off & 7: 0), 0100_0000 (off & 7: 1), 0010_0000 (off & 7: 2), ...
	(void)(rn_pair[1].rn_flags == RNF_ACTIVE);		// 4	rn_inithead0() で上書きされる
	(void)(rn_pair[1].rn_u.rn_node.rn_Off == (0 | 0));	// off >> 3 TODO
	(void)(rn_pair[1].rn_u.rn_node.rn_L == &rn_pair[0]);	// .rn_l
	(void)(rn_pair[1].rn_u.rn_node.rn_R == NULL);		// .rn_r	rn_inithead0() で &[2] になる
	// clang-format on

	//        [1] (.rn_p: NULL, .rn_b:0/+)
	// .rn_l /
	//     [0] (.rn_p: [1], .rn_b:-)
	//  rn_key: 0x42

	printf("&rn_pair[0] %p	&rn_pair[1] %p\n", &rn_pair[0], &rn_pair[1]);
	for (int off = 0; off < 129; off++) {
		break;
		memset(rn_pair, 0, sizeof(rn_pair));
		rn_newpair((void *)0x42U, off, rn_pair);
		// clang-format off
		printf("off %d	rn_pair[0]	rn_p %p	rn_b %hd	rn_bmask 0x%02x(char)	rn_flags 0x%02x	rn_u.rn_leaf.rn_Key %p	rn_u.rn_leaf.rn_Mask %p	rn_u.rn_leaf.rn_Dupedkey %p	rn_u.rn_node.rn_Off %d(0x%08x)	rn_u.rn_node.rn_L %p	rn_u.rn_node.rn_R %p\n", off, rn_pair[0].rn_p, rn_pair[0].rn_b, rn_pair[0].rn_bmask, rn_pair[0].rn_flags, rn_pair[0].rn_u.rn_leaf.rn_Key, rn_pair[0].rn_u.rn_leaf.rn_Mask, rn_pair[0].rn_u.rn_leaf.rn_Dupedkey, rn_pair[0].rn_u.rn_node.rn_Off, rn_pair[0].rn_u.rn_node.rn_Off, rn_pair[0].rn_u.rn_node.rn_L, rn_pair[0].rn_u.rn_node.rn_R);
		printf("off %d	rn_pair[1]	rn_p %p	rn_b %hd	rn_bmask 0x%02x(char)	rn_flags 0x%02x	rn_u.rn_leaf.rn_Key %p	rn_u.rn_leaf.rn_Mask %p	rn_u.rn_leaf.rn_Dupedkey %p	rn_u.rn_node.rn_Off %d(0x%08x)	rn_u.rn_node.rn_L %p	rn_u.rn_node.rn_R %p\n", off, rn_pair[1].rn_p, rn_pair[1].rn_b, rn_pair[1].rn_bmask, rn_pair[1].rn_flags, rn_pair[1].rn_u.rn_leaf.rn_Key, rn_pair[1].rn_u.rn_leaf.rn_Mask, rn_pair[1].rn_u.rn_leaf.rn_Dupedkey, rn_pair[1].rn_u.rn_node.rn_Off, rn_pair[1].rn_u.rn_node.rn_Off, rn_pair[1].rn_u.rn_node.rn_L, rn_pair[1].rn_u.rn_node.rn_R);
		// clang-format on
		__asm__("nop");
	}

	extern int rn_inithead0(struct radix_node_head * rnh, int off);
	(void)rt_inithead;

	// clang-format off
	// [0] tt leaf
	(void)(rnh0.rnh_nodes[0].rn_p == &rnh0.rnh_nodes[1]);
	(void)(rnh0.rnh_nodes[0].rn_b == (-1 | -2 | -3));			// -(off + 1)
	(void)(rnh0.rnh_nodes[0].rn_bmask == 0);				// 0 for leaf
	(void)(rnh0.rnh_nodes[0].rn_flags == (RNF_ROOT | RNF_ACTIVE));		// 6
	(void)(rnh0.rnh_nodes[0].rn_u.rn_leaf.rn_Key == rn_zeros);		// .rn_key

	// [1] t (non-leaf) node
	(void)(rnh0.rnh_nodes[1].rn_p == &rnh0.rnh_nodes[1]);
	(void)(rnh0.rnh_nodes[1].rn_b == (0 | 1 | 2));				// off
	(void)(rnh0.rnh_nodes[1].rn_bmask == (char)(0x80U | 0x40U | 0x20U));	// 80 40 20 10 08 04 02 01 (周期8) 80 40 ... __ 1000_0000 (off & 7: 0), 0100_0000 (off & 7: 1), 0010_0000 (off & 7: 2), ...
	(void)(rnh0.rnh_nodes[1].rn_flags == (RNF_ROOT | RNF_ACTIVE));		// 6
	(void)(rnh0.rnh_nodes[1].rn_u.rn_node.rn_Off == 99999);			// off >> 3 TODO
	(void)(rnh0.rnh_nodes[1].rn_u.rn_node.rn_L == &rnh0.rnh_nodes[0]);	// .rn_l
	(void)(rnh0.rnh_nodes[1].rn_u.rn_node.rn_R == &rnh0.rnh_nodes[2]);	// .rn_r

	// [2] ttt leaf
	// 	*ttt = *tt;
	// 	ttt->rn_key = rn_ones;
	// より [0] との差分は .rn_key だけ
	(void)(rnh0.rnh_nodes[2].rn_p == &rnh0.rnh_nodes[1]);
	(void)(rnh0.rnh_nodes[2].rn_b == (-1 | -2 | -3));			// -(off + 1)
	(void)(rnh0.rnh_nodes[2].rn_bmask == 0);				// 0 for leaf
	(void)(rnh0.rnh_nodes[2].rn_flags == (RNF_ROOT | RNF_ACTIVE));		// 6
	(void)(rnh0.rnh_nodes[2].rn_u.rn_leaf.rn_Key == rn_ones);		// .rn_key

	(void)(rnh0.rnh_treetop == &rnh0.rnh_nodes[1]);
	// clang-format on

	//        [1] (.rn_p: self (should be NULL?), .rn_b:0/+)
	// .rn_l /   \ .rn_r
	//     [0]   [2]
	//    zeros  ones
	// 3つとも RNF_ROOT

	printf("&rnh0.rnh_nodes[0] %p	&rnh0.rnh_nodes[1] %p	&rnh0.rnh_nodes[2] %p\n", &rnh0.rnh_nodes[0], &rnh0.rnh_nodes[1], &rnh0.rnh_nodes[2]);
	for (int off = 0; off < 129; off++) {
		break;
		// memset(&rnh0, 0, sizeof(rnh0)); // done in rn_inithead0()
		(void)rn_inithead0(&rnh0, off); // always returns 1
		// clang-format off
		printf("off %d	rnh0.rnh_nodes[0].rn_p %p	rn_b %hd	rn_bmask 0x%02x(char)	rn_flags 0x%02x	rn_u.rn_leaf.rn_Key %p	rn_u.rn_leaf.rn_Mask %p	rn_u.rn_leaf.rn_Dupedkey %p	rn_u.rn_node.rn_Off %d(0x%08x)	rn_u.rn_node.rn_L %p	rn_u.rn_node.rn_R %p\n", off, rnh0.rnh_nodes[0].rn_p, rnh0.rnh_nodes[0].rn_b, rnh0.rnh_nodes[0].rn_bmask, rnh0.rnh_nodes[0].rn_flags, rnh0.rnh_nodes[0].rn_u.rn_leaf.rn_Key, rnh0.rnh_nodes[0].rn_u.rn_leaf.rn_Mask, rnh0.rnh_nodes[0].rn_u.rn_leaf.rn_Dupedkey, rnh0.rnh_nodes[0].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[0].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[0].rn_u.rn_node.rn_L, rnh0.rnh_nodes[0].rn_u.rn_node.rn_R);
		printf("off %d	rnh0.rnh_nodes[1].rn_p %p	rn_b %hd	rn_bmask 0x%02x(char)	rn_flags 0x%02x	rn_u.rn_leaf.rn_Key %p	rn_u.rn_leaf.rn_Mask %p	rn_u.rn_leaf.rn_Dupedkey %p	rn_u.rn_node.rn_Off %d(0x%08x)	rn_u.rn_node.rn_L %p	rn_u.rn_node.rn_R %p\n", off, rnh0.rnh_nodes[1].rn_p, rnh0.rnh_nodes[1].rn_b, rnh0.rnh_nodes[1].rn_bmask, rnh0.rnh_nodes[1].rn_flags, rnh0.rnh_nodes[1].rn_u.rn_leaf.rn_Key, rnh0.rnh_nodes[1].rn_u.rn_leaf.rn_Mask, rnh0.rnh_nodes[1].rn_u.rn_leaf.rn_Dupedkey, rnh0.rnh_nodes[1].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[1].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[1].rn_u.rn_node.rn_L, rnh0.rnh_nodes[1].rn_u.rn_node.rn_R);
		printf("off %d	rnh0.rnh_nodes[2].rn_p %p	rn_b %hd	rn_bmask 0x%02x(char)	rn_flags 0x%02x	rn_u.rn_leaf.rn_Key %p	rn_u.rn_leaf.rn_Mask %p	rn_u.rn_leaf.rn_Dupedkey %p	rn_u.rn_node.rn_Off %d(0x%08x)	rn_u.rn_node.rn_L %p	rn_u.rn_node.rn_R %p\n", off, rnh0.rnh_nodes[2].rn_p, rnh0.rnh_nodes[2].rn_b, rnh0.rnh_nodes[2].rn_bmask, rnh0.rnh_nodes[2].rn_flags, rnh0.rnh_nodes[2].rn_u.rn_leaf.rn_Key, rnh0.rnh_nodes[2].rn_u.rn_leaf.rn_Mask, rnh0.rnh_nodes[2].rn_u.rn_leaf.rn_Dupedkey, rnh0.rnh_nodes[2].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[2].rn_u.rn_node.rn_Off, rnh0.rnh_nodes[2].rn_u.rn_node.rn_L, rnh0.rnh_nodes[2].rn_u.rn_node.rn_R);
		// clang-format on
		__asm__("nop");
	}

	// ref:
	// aw -l inetdomain.dom_rtoffset
	// #0  rt_inithead (tp=0xffffffff82177ad0 <rt_tables+16>, off=32) at /home/wsh/qc/netbsd/sys/net/rtbl.c:126
	// #1  0xffffffff816c6e99 in rtbl_init () at /home/wsh/qc/netbsd/sys/net/rtbl.c:236
	// 			// dom == &inetdomain
	// 			// dom->dom_rtoffset == 32 == offsetof(struct sockaddr_in, sin_addr) * 8
	// 			dom->dom_rtattach(&rt_tables[dom->dom_family],	// rt_tables[AF_INET] ==
	// 			    dom->dom_rtoffset);
	// #2  0xffffffff816c0768 in rt_init () at /home/wsh/qc/netbsd/sys/net/route.c:493
	// #3  0xffffffff816c8525 in route_init () at /home/wsh/qc/netbsd/sys/net/rtsock_shared.c:1729
	// #4  0xffffffff8159e2b3 in domain_attach (dp=0xffffffff82177bf0 <routedomain>) at /home/wsh/qc/netbsd/sys/kern/uipc_domain.c:148
	// 		(*dp->dom_init)();
	// #5  0xffffffff8159de5f in domaininit (attach=true) at /home/wsh/qc/netbsd/sys/kern/uipc_domain.c:121
	// #6  0xffffffff8148226a in main () at /home/wsh/qc/netbsd/sys/kern/init_main.c:620
	// #7  0xffffffff8020e450 in start () at /home/wsh/qc/netbsd/sys/arch/amd64/amd64/locore.S:1024
	// #8  0x0000000000000000 in ?? ()

	if (rn_inithead((void **)&rnh, offsetof(struct sockaddr_in, sin_addr) * 8) == 0) {
		log(LOG_ERR, "rn_inithead() failed\n");
		return;
	}

	//        [1] (.rn_p: self (should be NULL?), .rn_b:0/+)
	// .rn_l /   \ .rn_r
	//     [0]   [2]
	//    zeros  ones
	// 3つとも RNF_ROOT

	rn_printer_t printer = rn_dbg_print;
	void *arg = (void *)0x42U;
	struct radix_node *rn_parent = &rnh->rnh_nodes[1];
	struct radix_node *rn_left = &rnh->rnh_nodes[0];
	struct radix_node *rn_right = &rnh->rnh_nodes[2];

	// rn_walkfirst(): get leftmost leaf (rn_left)
	rn = rn_walkfirst(rn_parent, printer, arg /* unused */);	// [
	(void)(rn == rn_left);
	rn_dbg_print(arg /* unused */, "\n");

	// rn_walknext: next

	rn = rn_walknext(rn_left, printer, arg);	// (P: p<P> l<L> r<R>)
	(void)(rn == rn_right);
	rn_dbg_print(arg /* unused */, "\n");

	rn = rn_walknext(rn_right, printer, arg);
	(void)(rn == rn_right);
	rn_dbg_print(arg /* unused */, "\n");

	rn_walknext(rn_left, printer, rn_parent);	// arg: p -> (*P: p<P> l<L> r<R>)
	rn_dbg_print(arg /* unused */, "\n");

	rn_walknext(rn_left, printer, rn_left);	// arg: l -> (P: p<P> l<L> r<R>)
	rn_dbg_print(arg /* unused */, "\n");

	rn_walknext(rn_left, printer, rn_right);	// arg: r -> (P: p<P> l<L> r<R>)
	rn_dbg_print(arg /* unused */, "\n");

	rn_treeprint(rnh, rn_dbg_print, arg);

	// 全て RNF_ROOT なので何も起きない
	rn_walktree_(rnh, sandbox_walktree_visitor, arg);

	// TCP/IP Illustrated, Volume 2; Chapter 18

	struct sockaddr_in sin = {
	    .sin_len = sizeof(struct sockaddr_in),
	    .sin_family = AF_INET,
	    // .sin_port = htons(12345),
	    .sin_addr.s_addr = htonl(0x01020304U), // 1.2.3.4
	};
	struct sockaddr_in sin_mask = {
	    .sin_len = sizeof(struct sockaddr_in),
	    .sin_family = AF_INET,
	    // .sin_port = htons(12345),
	    .sin_addr.s_addr = htonl(0x01020304U), // 1.2.3.4
	};

	(void)(rnh->rnh_addaddr == rn_addroute);

	// route add default		140.242.13.33  # 0.0.0.0/0
	sin.sin_addr.s_addr = htonl(0x00000000U);      // 0.0.0.0
	sin_mask.sin_addr.s_addr = htonl(0x00000000U); // 0.0.0.0
	struct radix_node rn_0_m0;
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_0_m0);

	//  route add 127.0.0.0/8	127.0.0.1
	sin.sin_addr.s_addr = htonl(0x7f000000U);	// 127.0.0.0
	sin_mask.sin_addr.s_addr = htonl(0xff000000U);	// 255.0.0.0
	struct radix_node rn_127_m8 = {}; // TODO: 0埋めが正しいのか？ rt の実装見る
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_127_m8);

	// route add 127.0.0.1		127.0.0.1
	sin.sin_addr.s_addr = htonl(0x7f000001U);	// 127.0.0.1
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_127_0_0_1 = {};
	return;
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_127_0_0_1); // 2023-09-26 gcc panic

	// route add 128.32.33.5	140.252.13.33
	sin.sin_addr.s_addr = htonl(0x80202105);	// 128.32.33.5
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_128_32_33_5 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_128_32_33_5);

	// route add 140.252.13.32	link#1
	sin.sin_addr.s_addr = htonl(0x8cfc0d20U);	// 140.252.13.32
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_140_252_13_32 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_140_252_13_32);

	// route add 140.252.13.33	(arp)
	sin.sin_addr.s_addr = htonl(0x8cfc0d21U);	// 140.252.13.33
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_140_252_13_33 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_140_252_13_33);

	// route add 140.252.13.34	(arp)
	sin.sin_addr.s_addr = htonl(0x8cfc0d22U);	// 140.252.13.34
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_140_252_13_34 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_140_252_13_34);

	// route add 140.252.13.35	(arp)
	sin.sin_addr.s_addr = htonl(0x8cfc0d23U);	// 140.252.13.35
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_140_252_13_35 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_140_252_13_35);

	// route add 140.252.13.65	140.252.13.66
	sin.sin_addr.s_addr = htonl(0x8cfc0d41U);	// 140.252.13.65
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_140_252_13_65 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_140_252_13_65);

	// route add 224.0.0.0/8	link#1
	sin.sin_addr.s_addr = htonl(0xe0000000U);	// 224.0.0.0/8
	sin_mask.sin_addr.s_addr = htonl(0xff000000U);	// 255.0.0.0
	struct radix_node rn_224_m8 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_224_m8);

	// route add 224.0.0.1		link#1
	sin.sin_addr.s_addr = htonl(0xe0000001U);	// 224.0.0.1
	sin_mask.sin_addr.s_addr = htonl(0xffffffffU);	// 255.255.255.255
	struct radix_node rn_224_0_0_1 = {};
	rnh->rnh_addaddr(&sin, &sin_mask, rnh, &rn_224_0_0_1);

	printf("\n");
	dump_rn_nodes(rnh->rnh_nodes);
	// なんか微妙に本と違う… 何故だ？
	// nodes: 0xffffa08043fcc8d0 0xffffa08043fcc918 0xffffa08043fcc960
	// dumping nodes[1]...
	//       |b:-33 bmask:0x00 flags:0x06 key:255.255.255.255 (0xffffa08044087604 0x0) dupedkey:0x0 self:0xffffa08043fcc960
	//     |b:35 bmask:0x10 flags:0x04 off:4 mklist:0x0 self:0xffffffff826659a0
	//       |b:-65 bmask:0x00 flags:0x05 key:16.2.0.0 mask:8.255.255.255 (0xffffffff82665e28 0xffffa080440a62d8) dupedkey:0xffffffff82665958 self:0xffffffff82665910
	//   |b:33 bmask:0x40 flags:0x04 off:4 mklist:0x0 self:0xffffffff82665b50
	//     |b:-65 bmask:0x00 flags:0x05 key:16.2.0.0 mask:8.255.255.255 (0xffffffff82665e28 0xffffa080440a62d8) dupedkey:0x0 self:0xffffffff82665b08
	// |b:32 bmask:0xffffff80 flags:0x06 off:4 mklist:0xffffa080441449c8 self:0xffffa08043fcc918
	//         |b:-33 bmask:0x00 flags:0x06 key:255.255.255.255 (0xffffa08044087604 0x0) dupedkey:0x0 self:0xffffa08043fcc960
	//       |b:35 bmask:0x10 flags:0x04 off:4 mklist:0x0 self:0xffffffff826659a0
	//         |b:-65 bmask:0x00 flags:0x05 key:16.2.0.0 mask:8.255.255.255 (0xffffffff82665e28 0xffffa080440a62d8) dupedkey:0xffffffff82665958 self:0xffffffff82665910
	//     |b:33 bmask:0x40 flags:0x04 off:4 mklist:0x0 self:0xffffffff82665b50
	//       |b:-65 bmask:0x00 flags:0x05 key:16.2.0.0 mask:8.255.255.255 (0xffffffff82665e28 0xffffa080440a62d8) dupedkey:0x0 self:0xffffffff82665b08
	//   |b:33 bmask:0x40 flags:0x04 off:4 mklist:0xffffa080441449c8 self:0xffffffff82665be0
	//     |b:-33 bmask:0x00 flags:0x06 key:0.0.0.0 (0xffffa080440875c8 0x0) dupedkey:0xffffffff82665be0 self:0xffffa08043fcc8d0

	__asm__("nop");

	// TODO: rn_search()

	// https://www.netbsd.org/docs/internals/en/netbsd-internals.html
	// > Returns the leaf node found at the end of the bit comparisons. This is either a match or the leaf in the the tree that should be backtracked to find a match.
	// 	for (x = head; x->rn_b >= 0;) {	// while x is node (non-leaf)
	// 		// x->rn_bmask: 0x80
	// 		// &v[4] == &((struct sockaddr_in *)v)->sin_addr
	// 		//   -> 0x01  -> (be) 1.2.3.4
	// 		// 0x80 & 0x01 == 0
	// 		if (x->rn_bmask & v[x->rn_off])
	// 			x = x->rn_r;
	// 		else
	// 			x = x->rn_l; // this
	rn_search(&sin, rnh->rnh_nodes);
	(void)rn_lookup;

	Free(rnh);
}

// -----------------------------------------------------------------------------
// wataash_sandbox - softint

static struct wataash_softc {
	void *sc_si1;
	void *sc_si2;
} sc;

static void
softint_cb(void *arg)
{
	// softint ではLWP (thread context) を持つ
	struct lwp *l = curlwp;
	// l->l_wmesg: NULL
	// @ref:qc-netbsd-lwp-r12
	// (gdb) info symbol ((struct switchframe *)((struct trapframe *)l->l_md.md_regs) - 1)->sf_r12
	// softint_thread in section .text

	// なので yield(9) が使える 多分
	// preempt();
	// yield();
	// -> go back to softint()

	// comintr() で preempt() / yield() を呼ぶと panic
	// curlwp->l_proc->p_comm は "sh" で、多分 curlwp はゴミ
	// l->l_wmesg: "pipe_rd"

	// p: predicate https://stackoverflow.com/questions/70545047/what-does-suffix-p-mean-in-common-lisp-functions
	cpu_intr_p();		// false
	cpu_softintr_p();	// true
	kpreempt_disabled();	// true
	if (0) // TODO
		ASSERT_SLEEPABLE(); // panic; sleep禁止

	__asm__("nop");
}

static void
softint_cb_mpsafe(void *arg)
{
	struct lwp *l = curlwp; // == ↑ l
	cpu_intr_p();		// false
	cpu_softintr_p();	// true
	kpreempt_disabled();	// true
	if (0)
		ASSERT_SLEEPABLE(); // panic; sleep禁止
	__asm__("nop");
}

static void
softint(void)
{
	// TODO: SOFTINT_RCPU; Remote CPU; 他のcpuで実行されるらしい
	sc.sc_si1 = softint_establish(SOFTINT_SERIAL, softint_cb, &sc);
	sc.sc_si2 = softint_establish(SOFTINT_SERIAL | SOFTINT_MPSAFE, softint_cb_mpsafe, &sc);

	// without kpreempt_disable(): panic
	// 普通はhardware interrupt handlerで soft_schedule() を呼ぶので、既に kpreempt_disabled() なんだと思う
	kpreempt_disable();
	softint_schedule(sc.sc_si1); // -> softint_trigger orq	%rdi,CPUVAR(IPENDING); spllower doreti (device handler finishes) 等のタイミングでチェックされて IDTVEC(softintr) の実行が予約されるっぽい
	softint_schedule(sc.sc_si2);
	kpreempt_enable(); // spl

	// softint_disestablish(opaque1);
	// softint_disestablish(opaque2);

	cpu_intr_p();       // false
	cpu_softintr_p();   // false
	ASSERT_SLEEPABLE(); // ok

	__asm__("nop");
}

// -----------------------------------------------------------------------------
// wataash_sandbox - sysctl_

static void
sysctl_(void)
{
	__asm__("nop");
}
// -----------------------------------------------------------------------------
// wataash_sandbox - x_net

static void
x_net(void)
{
	__asm__("nop");
}
