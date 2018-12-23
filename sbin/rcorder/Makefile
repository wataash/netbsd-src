#       $NetBSD: Makefile,v 1.8 2008/08/03 07:50:54 lukem Exp $

PROG=   rcorder
SRCS=   hash.c rcorder.c
MAN=	rcorder.8

LDADD+=-lutil
DPADD+=${LIBUTIL}

.if ${:!uname -s!} == "Darwin"
CFLAGS+= -O0 -ggdb3

MKDYNAMICROOT=	yes
CFLAGS+=-	-O0 -ggdb3
# TODO: test ${HOME}
CPPFLAGS+=	-I${HOME}/src/netbsd/src/z_compat_apple
LDADD+=		${.CURDIR}../../lib/libutil/libutil.a"

.endif

.include <bsd.prog.mk>
