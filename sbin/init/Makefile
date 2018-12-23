#	$NetBSD: Makefile,v 1.39 2013/07/15 00:18:03 khorben Exp $
#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CPPFLAGS+=	-DMFS_DEV_IF_NO_CONSOLE -DSUPPORT_UTMP -DSUPPORT_UTMPX

.ifdef	 INIT_CHROOT
CPPFLAGS+=	-DCHROOT
.elifdef SMALLPROG
CPPFLAGS+=	-DLETS_GET_SMALL
.else
CPPFLAGS+=	-DALTSHELL -DSECURE -DCHROOT
DPADD+=		${LIBCRYPT}
.if ${:!uname -s!} == "Darwin"
LDADD+=		-lcrypto
MKDYNAMICROOT=	yes
CFLAGS+=	-O0 -ggdb3
# TODO: test ${HOME} or raplace with ${NETBSDSRC}
CPPFLAGS+=	-I${HOME}/src/netbsd/src/z_compat_apple
.else
LDADD+=		-lcrypt
.endif
.endif



.include <bsd.prog.mk>
