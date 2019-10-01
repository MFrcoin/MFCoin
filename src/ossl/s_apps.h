/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>

#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)
#include <conio.h>
#endif

#if defined(OPENSSL_SYS_MSDOS) && !defined(_WIN32)
#define _kbhit kbhit
#endif

#if defined(OPENSSL_SYS_VMS) && !defined(FD_SET)
/* VAX C does not defined fd_set and friends, but it's actually quite simple */
/* These definitions are borrowed from SOCKETSHR.	/Richard Levitte */
#define MAX_NOFILE	32
#define	NBBY		 8		/* number of bits in a byte	*/

#ifndef	FD_SETSIZE
#define	FD_SETSIZE	MAX_NOFILE
#endif	/* FD_SETSIZE */

/* How many things we'll allow select to use. 0 if unlimited */
#define MAXSELFD	MAX_NOFILE
typedef int	fd_mask;	/* int here! VMS prototypes int, not long */
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask (power of 2!)*/
#define NFDSHIFT 5				/* Shift based on above */

typedef fd_mask fd_set;
#define	FD_SET(n, p)	(*(p) |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	(*(p) &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	(*(p) & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((char *)(p), 0, sizeof(*(p)))
#endif

#define PORT            4433
#define PORT_STR        "4433"
#define PROTOCOL        "tcp"

int do_server(int port, int type, int *ret, int (*cb) (char *hostname, int s, unsigned char *context), unsigned char *context);
#ifdef HEADER_X509_H
int MS_CALLBACK verify_callback(int ok, X509_STORE_CTX *ctx);
#endif
#ifdef HEADER_SSL_H
int set_cert_stuff(SSL_CTX *ctx, char *cert_file, char *key_file);
int set_cert_key_stuff(SSL_CTX *ctx, X509 *cert, EVP_PKEY *key);
#endif
int init_client(int *sock, char *server, int port, int type);
int should_retry(int i);
int extract_port(char *str, short *port_ptr);
int extract_host_port(char *str,char **host_ptr,unsigned char *ip,short *p);

long MS_CALLBACK bio_dump_callback(BIO *bio, int cmd, const char *argp,
				   int argi, long argl, long ret);

#ifdef HEADER_SSL_H
void MS_CALLBACK apps_ssl_info_callback(const SSL *s, int where, int ret);
void MS_CALLBACK msg_cb(int write_p, int version, int content_type, const void *buf, size_t len, SSL *ssl, void *arg);
void MS_CALLBACK tlsext_cb(SSL *s, int client_server, int type,
					unsigned char *data, int len,
					void *arg);
#endif

int MS_CALLBACK generate_cookie_callback(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len);
int MS_CALLBACK verify_cookie_callback(SSL *ssl, unsigned char *cookie, unsigned int cookie_len);
