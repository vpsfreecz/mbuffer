/*
 *  Copyright (C) 2000-2021, Thomas Maier-Komor
 *
 *  This file is part of mbuffer's source code.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mbconf.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif


#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dest.h"
#include "globals.h"
#include "network.h"
#include "settings.h"
#include "log.h"

int32_t TCPBufSize = 0;
double TCPTimeout = 100;
#if defined(PF_INET6) && defined(PF_UNSPEC)
int AddrFam = PF_UNSPEC;
#else
int AddrFam = PF_INET;
#endif


static void setTCPBufferSize(int sock, int buffer)
{
	int err;
	int32_t osize, size;
	socklen_t bsize = (socklen_t)sizeof(osize);

	assert(buffer == SO_RCVBUF || buffer == SO_SNDBUF);
	err = getsockopt(sock,SOL_SOCKET,buffer,&osize,&bsize);
	assert((err == 0) && (bsize == sizeof(osize)));
	if (osize < TCPBufSize) {
		size = TCPBufSize;
		assert(size > 0);
		do {
			err = setsockopt(sock,SOL_SOCKET,buffer,(void *)&size,sizeof(size));
			size >>= 1;
		} while ((-1 == err) && (errno == ENOMEM) && (size > osize));
		if (err == -1) {
			warningmsg("unable to set socket buffer size: %s\n",strerror(errno));
			return;
		}
	}
	bsize = sizeof(size);
	err = getsockopt(sock,SOL_SOCKET,buffer,&size,&bsize);
	assert(err != -1);
	if (buffer == SO_RCVBUF) 
		infomsg("set TCP receive buffer size to %d\n",size);
	else
		infomsg("set TCP send buffer size to %d\n",size);
}


#ifdef HAVE_GETADDRINFO

static const char *addrinfo2str(const struct addrinfo *ai, char *buf)
{
	char *at = buf;
	struct protoent *pent = getprotobynumber(ai->ai_protocol);
	if (pent && pent->p_name) {
		size_t l = strlen(pent->p_name);
		memcpy(at,pent->p_name,l);
		at[l] = '/';
		at += l + 1;
	}
	if (ai->ai_family == AF_INET) {
		if (ai->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *a = (struct sockaddr_in *) ai->ai_addr;
			at += sprintf(at,"%u.%u.%u.%u:%hu"
					, (a->sin_addr.s_addr >> 24) & 0xff
					, (a->sin_addr.s_addr >> 16) & 0xff
					, (a->sin_addr.s_addr >> 8) & 0xff
					, (a->sin_addr.s_addr) & 0xff
					, ntohs(a->sin_port));
		} else {
			strcpy(at,"<ipv4 with unexpected addrlen>");
		}
	} else if (ai->ai_family == AF_INET6) {
		if (ai->ai_addrlen == sizeof(struct sockaddr_in6)) {
			struct sockaddr_in6 *a = (struct sockaddr_in6 *) ai->ai_addr;
			char scope[IF_NAMESIZE+1];
			if (0 == if_indextoname(a->sin6_scope_id,scope+1))
				scope[0] = 0;
			else
				scope[0] = '%';
			at += sprintf(at,"[%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx]%s:%hu"
					, ntohs(a->sin6_addr.s6_addr[0])
					, ntohs(a->sin6_addr.s6_addr[1])
					, ntohs(a->sin6_addr.s6_addr[2])
					, ntohs(a->sin6_addr.s6_addr[3])
					, ntohs(a->sin6_addr.s6_addr[4])
					, ntohs(a->sin6_addr.s6_addr[5])
					, ntohs(a->sin6_addr.s6_addr[6])
					, ntohs(a->sin6_addr.s6_addr[7])
					, scope
					, ntohs(a->sin6_port));
		} else {
			strcpy(at,"<ipv4 with unexpected addrlen>");
		}
	} else {
		strcpy(buf,"<unknown address family>");
	}
	return buf;
}


void initNetworkInput(const char *addr)
{
	char *host, *port;
	struct addrinfo hint, *pinfo = 0, *x, *cinfo = 0;
	int err, sock = -1, l;

	debugmsg("initNetworkInput(\"%s\")\n",addr);
	if (Infile != 0)
		fatal("cannot initialize input from network - input from file already set\n");
	if (In != -1)
		fatal("cannot initialize input from network - input already set\n");
	l = strlen(addr) + 1;
	host = alloca(l);
	memcpy(host,addr,l);
	port = strrchr(host,':');
	if (port == 0) {
		port = host;
		host = 0;
	} else if (port == host) {
		port = host + 1;
		host = 0;
	} else {
		if ((host[0] == '[') && (port[-1] == ']')) {
			++host;
			port[-1] = 0;
		}
		*port = 0;
		++port;
		bzero(&hint,sizeof(hint));
		hint.ai_family = AddrFam;
		hint.ai_protocol = IPPROTO_TCP;
		hint.ai_socktype = SOCK_STREAM;
#ifdef AI_V4MAPPED
		hint.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
#else
		hint.ai_flags = AI_ADDRCONFIG;
#endif
		err = getaddrinfo(host,0,&hint,&cinfo);
		if (err != 0) 
			fatal("unable to resolve address information for expected host '%s': %s\n",host,gai_strerror(err));
	}
	bzero(&hint,sizeof(hint));
	hint.ai_family = AddrFam;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	err = getaddrinfo(0,port,&hint,&pinfo);
	if (err != 0)
		fatal("unable to get address information for port/service '%s': %s\n",port,gai_strerror(err));
	assert(pinfo);
	for (x = pinfo; x; x = x->ai_next) {
		int reuse_addr = 1;
		debugmsg("creating socket for address familiy %d\n",x->ai_family);
		sock = socket(x->ai_family, SOCK_STREAM, 0);
		if (sock == -1) {
			warningmsg("unable to create socket for address family %u: %s\n",x->ai_family,strerror(errno));
			continue;
		}
		if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)))
			warningmsg("cannot set socket to reuse address: %s\n",strerror(errno));
		if (0 == bind(sock, x->ai_addr, x->ai_addrlen)) {
			debugmsg("successfully bound socket - address length %d\n",x->ai_addrlen);
			break;
		}
		char addrstr[64+IF_NAMESIZE];
		warningmsg("could not bind to socket for %s: %s\n",addrinfo2str(x,addrstr),strerror(errno));
		(void) close(sock);
	}
	if (x == 0)
		fatal("Unable to initialize network input.\n");
	infomsg("listening on socket...\n");
	if (0 > listen(sock,1))		/* accept only 1 incoming connection */
		fatal("could not listen on socket for network input: %s\n",strerror(errno));
	for (;;) {
		char chost[NI_MAXHOST], serv[NI_MAXSERV];
		struct sockaddr_in6 caddr;
		struct addrinfo *c;
		socklen_t len = sizeof(caddr);
		int err;

		debugmsg("waiting for incoming connection\n");
		In = accept(sock, (struct sockaddr *) &caddr, &len);
		if (0 > In)
			fatal("Unable to accept connection for network input: %s\n",strerror(errno));
		err = getnameinfo((struct sockaddr *) &caddr,len,chost,sizeof(chost),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV|NI_NOFQDN);
		if (0 != err) {
			fatal("unable to get name information for hostname of incoming connection: %s\n",gai_strerror(err));
		}
		infomsg("incoming connection from %s:%s\n",chost,serv);
		if (host == 0)
			break;
		for (c = cinfo; c; c = c->ai_next) {
			char xhost[NI_MAXHOST];
			if (0 == getnameinfo((struct sockaddr *)c->ai_addr,c->ai_addrlen,xhost,sizeof(xhost),0,0,NI_NUMERICHOST|NI_NOFQDN)) {
				debugmsg("checking against host '%s'\n",xhost);
				if (0 == strcmp(xhost,chost))
					break;
			}
		}
		if (c)
			break;
		warningmsg("rejected connection from %s\n",chost);
		if (-1 == close(In))
			warningmsg("error closing rejected input: %s\n",strerror(errno));
	}
	freeaddrinfo(pinfo);
	if (cinfo)
		freeaddrinfo(cinfo);
	debugmsg("input connection accepted\n");
	if (TCPBufSize)
		setTCPBufferSize(In,SO_RCVBUF);
	if (TCPTimeout > 0) {
		struct timeval timeo;
		timeo.tv_sec = floor(TCPTimeout);
		timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1000000;
		if (-1 == setsockopt(In, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)))
			warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
	} else {
		debugmsg("disabled TCP receive timeout\n");
	}
	(void) close(sock);
}


dest_t *createNetworkOutput(const char *addr)
{
	char *host, *port;
	struct addrinfo hint, *ret = 0, *x;
	int err, fd = -1;
	dest_t *d;

	assert(addr);
	host = strdup(addr);
	assert(host);
	port = strrchr(host,':');
	if (port == 0)
		fatal("syntax error - target must be given in the form <host>:<port>\n");
	if ((host[0] == '[') && (port > host) && (port[-1] == ']')) {
		++host;
		port[-1] = 0;
	}
	*port++ = 0;
	bzero(&hint,sizeof(hint));
	hint.ai_family = AddrFam;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_ADDRCONFIG;
	debugmsg("getting address info for %s\n",addr);
	err = getaddrinfo(host,port,&hint,&ret);
	if (err != 0)
		fatal("unable to resolve address information for '%s': %s\n",addr,gai_strerror(err));
	for (x = ret; x; x = x->ai_next) {
		fd = socket(x->ai_family, SOCK_STREAM, 0);
		if (fd == -1) {
			errormsg("unable to create socket: %s\n",strerror(errno));
			continue;
		}
		char addrstr[64+IF_NAMESIZE];
		if (0 == connect(fd, x->ai_addr, x->ai_addrlen)) {
			struct sockaddr_in local_address;
			socklen_t addr_size = sizeof(local_address);
			getsockname(fd, (struct sockaddr *) &local_address, &addr_size);
			infomsg("successfully connected to %s from :%d\n",addr,ntohs((&local_address)->sin_port));
			break;
		}
		warningmsg("error connecting to %s: %s\n",addrinfo2str(x,addrstr),strerror(errno));
		(void) close(fd);
		fd = -1;
	}
	if (fd == -1) {
		errormsg("unable to connect to %s\n",addr);
		host = 0;	// tag as start failed
	} else {
		if (TCPBufSize)
			setTCPBufferSize(fd,SO_SNDBUF);
		if (TCPTimeout) {
			struct timeval timeo;
			timeo.tv_sec = floor(TCPTimeout);
			timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1E6;
			if (-1 == setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)))
				warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
		} else {
			debugmsg("disabled TCP send timeout\n");
		}
	}
	d = (dest_t *) malloc(sizeof(dest_t));
	d->arg = addr;
	d->name = host;
	d->port = port;
	d->fd = fd;
	bzero(&d->thread,sizeof(d->thread));
	d->result = 0;
	d->next = 0;
	return d;
}


#else	/* HAVE_GETADDRINFO */


static void openNetworkInput(const char *host, unsigned short port)
{
	struct hostent *h = 0, *r = 0;
	const int reuse_addr = 1;
	int sock;

	debugmsg("openNetworkInput(\"%s\",%hu)\n",host,port);
	sock = socket(AddrFam == AF_INET6 ? AF_INET6 : AF_INET, SOCK_STREAM, 6);
	if (0 > sock)
		fatal("could not create socket for network input: %s\n",strerror(errno));
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)))
		warningmsg("cannot set socket to reuse address: %s\n",strerror(errno));
	setTCPBufferSize(sock,SO_RCVBUF);
	if (host[0]) {
		debugmsg("resolving hostname '%s' of input...\n",host);
		h = gethostbyname(host);
		if (0 == h)
#ifdef HAVE_HSTRERROR
			fatal("could not resolve server hostname: %s\n",hstrerror(h_errno));
#else
			fatal("could not resolve server hostname: error code %d\n",h_errno);
#endif
	}
	if (AddrFam == AF_INET6) {
		struct sockaddr_in6 saddr;
		bzero((void *) &saddr, sizeof(saddr));
		saddr.sin6_family = AF_INET6;
		saddr.sin6_port = htons(port);
		debugmsg("binding socket to port %d...\n",port);
		if (0 > bind(sock, (struct sockaddr *) &saddr, sizeof(saddr)))
			fatal("could not bind to ipv6 socket for network input: %s\n",strerror(errno));
	} else {
		struct sockaddr_in saddr;
		bzero((void *) &saddr, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(port);
		debugmsg("binding socket to port %d...\n",port);
		if (0 > bind(sock, (struct sockaddr *) &saddr, sizeof(saddr)))
			fatal("could not bind to socket for network input: %s\n",strerror(errno));
	}
	debugmsg("listening on socket...\n");
	if (0 > listen(sock,1))		/* accept only 1 incoming connection */
		fatal("could not listen on socket for network input: %s\n",strerror(errno));
	for (;;) {
		struct sockaddr_in caddr;
		socklen_t clen = sizeof(caddr);
		char **p;
		debugmsg("waiting to accept connection...\n");
		In = accept(sock, (struct sockaddr *)&caddr, &clen);
		if (0 > In)
			fatal("could not accept connection for network input: %s\n",strerror(errno));
		if (host[0] == 0) {
			infomsg("accepted connection from %s\n",inet_ntoa(caddr.sin_addr));
			(void) close(sock);
			if (TCPTimeout) {
				struct timeval timeo;
				timeo.tv_sec = floor(TCPTimeout);
				timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1000000;
				if (-1 == setsockopt(In, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)))
					warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
				else
					infomsg("set TCP receive timeout to %usec, %uusec\n",timeo.tv_sec,timeo.tv_usec);
			} else {
				debugmsg("disabled TCP receive timeout\n");
			}
			return;
		}
		for (p = h->h_addr_list; *p; ++p) {
			if (0 == memcmp(&caddr.sin_addr,*p,h->h_length)) {
				infomsg("accepted connection from %s\n",inet_ntoa(caddr.sin_addr));
				(void) close(sock);
				return;
			}
		}
		r = gethostbyaddr((char *)&caddr.sin_addr,sizeof(caddr.sin_addr.s_addr),AF_INET);
		if (r)
			warningmsg("rejected connection from %s (%s)\n",r->h_name,inet_ntoa(caddr.sin_addr));
		else
			warningmsg("rejected connection from %s\n",inet_ntoa(caddr.sin_addr));
		if (-1 == close(In))
			warningmsg("error closing rejected input: %s\n",strerror(errno));
	}
}


void initNetworkInput(const char *addr)
{
	char *host, *portstr;
	unsigned pnr;
	size_t l;

	debugmsg("initNetworkInput(\"%s\")\n",addr);
	l = strlen(addr) + 1;
	host = alloca(l);
	memcpy(host,addr,l);
	portstr = strrchr(host,':');
	if (portstr == 0) {
		portstr = host;
		host = "";
	} else if (portstr == host) {
		*portstr = 0;
		++portstr;
		host = "";
	} else {
		*portstr = 0;
		++portstr;
	}
	if (1 != sscanf(portstr,"%u",&pnr))
		fatal("invalid port string '%s' - port must be given by its number, not service name\n", portstr);
	openNetworkInput(host,pnr);
}


static void openNetworkOutput(dest_t *dest)
{
	unsigned short pnr;

	debugmsg("creating socket for output to %s:%s...\n",dest->name,dest->port);
	if (1 != sscanf(dest->port,"%hu",&pnr))
		fatal("port must be given by its number, not service name\n");
	out = socket(PF_INET, SOCK_STREAM, 0);
	if (0 > out) {
		errormsg("could not create socket for network output: %s\n",strerror(errno));
		return;
	}
	setTCPBufferSize(out,SO_SNDBUF);
	bzero((void *) &saddr, sizeof(saddr));
	saddr.sin6_port = htons(pnr);
	infomsg("resolving host %s...\n",dest->name);
	if (((dest->name[0] >= '0') && (dest->name[0] <= '9')) || (dest->name[0] == ':')) {
		if (AddrFam == AF_UNSPEC)
			saddr.sin6_family = strchr(dest->name,':') ? AF_INET6 : AF_INET;
		else
			saddr.sin6_family = AddrFam;
		a = inet_pton(saddr.sin6_family,dest->name,&saddr.sin6_addr);
		if (a != 1) {
			dest->result = "unable to translate address";
			errormsg("unable to translate address %s\n",dest->name);
			dest->fd = -1;
			dest->name = 0;		// tag as start failed
			(void) close(out);
			return;
		}
	} else {
		struct hostent *h = gethostbyname(dest->name);
		if (0 == h) {
#ifdef HAVE_HSTRERROR
			dest->result = hstrerror(h_errno);
			errormsg("could not resolve hostname %s: %s\n",dest->name,dest->result);
#else
			dest->result = "unable to resolve hostname";
			errormsg("could not resolve hostname %s: error code %d\n",dest->name,h_errno);
#endif
			dest->fd = -1;
			dest->name = 0;		// tag as start failed
			(void) close(out);
			return;
		}
		saddr.sin6_family = h->h_addrtype;
		assert(h->h_length <= sizeof(saddr.sin_addr));
		(void) memcpy(&saddr.sin_addr,h->h_addr_list[0],h->h_length);
	}
	infomsg("connecting to server at %s...\n",inet_ntoa(saddr.sin_addr));
	if (0 > connect(out, (struct sockaddr *) &saddr, sizeof(saddr))) {
		dest->result = strerror(errno);
		errormsg("could not connect to %s:%s: %s\n",dest->name,dest->port,dest->result);
		(void) close(out);
		out = -1;
	} else if (TCPTimeout) {
		struct timeval timeo;
		timeo.tv_sec = floor(TCPTimeout);
		timeo.tv_usec = (TCPTimeout-timeo.tv_sec)*1000000;
		if (-1 == setsockopt(out, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)))
			warningmsg("cannot set socket send timeout: %s\n",strerror(errno));
		else
			infomsg("set TCP transmit timeout to %usec, %uusec\n",timeo.tv_sec,timeo.tv_usec);
	} else {
		debugmsg("disabled TCP transmint timeout\n");

	}
	dest->fd = out;
}


dest_t *createNetworkOutput(const char *addr)
{
	char *host, *portstr;
	dest_t *d = (dest_t *) malloc(sizeof(dest_t));

	debugmsg("createNetworkOutput(\"%s\")\n",addr);
	host = strdup(addr);
	portstr = strrchr(host,':');
	if ((portstr == 0) || (portstr == host))
		fatal("argument '%s' doesn't match <host>:<port> format\n",addr);
	*portstr++ = 0;
	bzero(d, sizeof(dest_t));
	d->fd = -1;
	d->arg = addr;
	d->name = host;
	d->port = portstr;
	d->result = 0;
	openNetworkOutput(d);
	return d;
}


#endif /* HAVE_GETADDRINFO */


/* vim:tw=0
 */
