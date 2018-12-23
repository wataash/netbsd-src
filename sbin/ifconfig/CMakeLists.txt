# CMAKE_MINIMUM_REQUIRED(VERSION 3.8)
project(ifconfig)

add_executable(ifconfig
        af_atalk.c
        af_link.c
        carp.c
        pfsync.c
        # Makefile.common
        af_inetany.h env.h extern.h media.h parse.h util.h
        af_inet.c af_inetany.c env.c ether.c ieee80211.c
        ifconfig.c media.c parse.c tunnel.c util.c vlan.c
        agr.c l2tp.c
        )

include_directories(${PROJECT_SOURCE_DIR}/../../sys/dist/pf/)
add_definitions()

#[[
#	$NetBSD: Makefile,v 1.61 2017/10/10 19:30:06 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

# when making a change to this file, please check if the change is
# also needed for src/distrib/utils/x_ifconfig.
# such stuff should be into Makefile.inc.

.include <bsd.own.mk>

RUMPPRG=	ifconfig
MAN=		ifconfig.8

SRCS=		af_atalk.c af_link.c carp.c

CPPFLAGS+=	-I${NETBSDSRCDIR}/sys/dist/pf/
SRCS+=		pfsync.c

.if (${USE_INET6} != "no")
CPPFLAGS+=	-DINET6
SRCS+=		af_inet6.c
.endif

.include "Makefile.common"

.if (${MKRUMP} != "no")
CPPFLAGS+=	-DRUMP_ACTION
LDADD.rump +=	-lrumpres
DPADD.rump +=	${LIBRUMPRES}
.endif

.include <bsd.prog.mk>
]]


#[[
#	$NetBSD: Makefile.common,v 1.5 2017/05/02 20:12:27 christos Exp $

# shared stuff with src/distrib/utils/x_ifconfig for install media.
# stuff not required by install media should be into Makefile.

.ifdef SMALLPROG
CPPFLAGS+=	-DSMALL
.endif

DPADD+=		${LIBUTIL} ${LIBPROP}
LDADD+=		-lutil -lprop

INCS+=		af_inetany.h env.h extern.h media.h parse.h util.h
SRCS+=		af_inet.c af_inetany.c env.c ether.c ieee80211.c \
		ifconfig.c media.c parse.c tunnel.c util.c vlan.c
.ifndef SMALLPROG
SRCS+=		agr.c l2tp.c
.endif

]]
