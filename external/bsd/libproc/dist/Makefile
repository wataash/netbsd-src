# $FreeBSD: head/lib/libproc/Makefile 284345 2015-06-13 19:20:56Z sjg $

.include <src.opts.mk>

LIB=	proc

SRCS=	proc_bkpt.c		\
	proc_create.c		\
	proc_regs.c		\
	proc_sym.c		\
	proc_rtld.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I${.CURDIR}
# avoid cyclic dependency
CFLAGS+=	-I${.CURDIR:H}/librtld_db
GENDIRDEPS_FILTER+= Nlib/librtld_db

.if ${MK_CXX} == "no"
CFLAGS+=	-DNO_CXA_DEMANGLE
.elif ${MK_LIBCPLUSPLUS} != "no"
LIBADD+=		cxxrt
.else
LIBADD+=	supcplusplus
.endif

LIBADD+=	elf rtld_db util

.if ${MK_CDDL} != "no"
LIBADD+=	ctf
IGNORE_PRAGMA=	YES
CFLAGS+=	-I${.CURDIR}/../../cddl/contrib/opensolaris/lib/libctf/common \
		-I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../sys/cddl/compat/opensolaris
.else
CFLAGS+=	-DNO_CTF
.endif

SHLIB_MAJOR=	3

MAN=

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
