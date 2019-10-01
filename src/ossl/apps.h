


/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_APPS_H
#define HEADER_APPS_H

#include "e_os.h"

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/lhash.h>
#include <openssl/conf.h>
#include <openssl/txt_db.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#ifndef OPENSSL_NO_OCSP
#include <openssl/ocsp.h>
#endif
#include <openssl/ossl_typ.h>

int app_RAND_load_file(const char *file, BIO *bio_e, int dont_warn);
int app_RAND_write_file(const char *file, BIO *bio_e);
/* When `file' is NULL, use defaults.
 * `bio_e' is for error messages. */
void app_RAND_allow_write_file(void);
long app_RAND_load_files(char *file); /* `file' is a list of files to read,
                                       * separated by LIST_SEPARATOR_CHAR
                                       * (see e_os.h).  The string is
                                       * destroyed! */

#ifndef MONOLITH

#define MAIN(a,v)	main(a,v)

#ifndef NON_MAIN
CONF *config=NULL;
BIO *bio_err=NULL;
#else
extern CONF *config;
extern BIO *bio_err;
#endif

#else

#define MAIN(a,v)	PROG(a,v)
extern CONF *config;
extern char *default_config_file;
extern BIO *bio_err;

#endif

#ifndef OPENSSL_SYS_NETWARE
#include <signal.h>
#endif

#ifdef SIGPIPE
#define do_pipe_sig()	signal(SIGPIPE,SIG_IGN)
#else
#define do_pipe_sig()
#endif

#ifdef OPENSSL_NO_COMP
#define zlib_cleanup() 
#else
#define zlib_cleanup() COMP_zlib_cleanup()
#endif

#if defined(MONOLITH) && !defined(OPENSSL_C)
#  define apps_startup() \
		do_pipe_sig()
#  define apps_shutdown()
#else
#  ifndef OPENSSL_NO_ENGINE
#    define apps_startup() \
			do { do_pipe_sig(); CRYPTO_malloc_init(); \
			ERR_load_crypto_strings(); OpenSSL_add_all_algorithms(); \
			ENGINE_load_builtin_engines(); setup_ui_method(); } while(0)
#    define apps_shutdown() \
			do { CONF_modules_unload(1); destroy_ui_method(); \
			OBJ_cleanup(); EVP_cleanup(); ENGINE_cleanup(); \
			CRYPTO_cleanup_all_ex_data(); ERR_remove_thread_state(NULL); \
			RAND_cleanup(); \
			ERR_free_strings(); zlib_cleanup();} while(0)
#  else
#    define apps_startup() \
			do { do_pipe_sig(); CRYPTO_malloc_init(); \
			ERR_load_crypto_strings(); OpenSSL_add_all_algorithms(); \
			setup_ui_method(); } while(0)
#    define apps_shutdown() \
			do { CONF_modules_unload(1); destroy_ui_method(); \
			OBJ_cleanup(); EVP_cleanup(); \
			CRYPTO_cleanup_all_ex_data(); ERR_remove_thread_state(NULL); \
			RAND_cleanup(); \
			ERR_free_strings(); zlib_cleanup(); } while(0)
#  endif
#endif

#ifdef OPENSSL_SYSNAME_WIN32
#  define openssl_fdset(a,b) FD_SET((unsigned int)a, b)
#else
#  define openssl_fdset(a,b) FD_SET(a, b)
#endif


typedef struct args_st
	{
	char **data;
	int count;
	} ARGS;

#define PW_MIN_LENGTH 4
typedef struct pw_cb_data
	{
	const void *password;
	const char *prompt_info;
	} PW_CB_DATA;

int password_callback(char *buf, int bufsiz, int verify,
	PW_CB_DATA *cb_data);

int setup_ui_method(void);
void destroy_ui_method(void);

int should_retry(int i);
int args_from_file(char *file, int *argc, char **argv[]);
int str2fmt(char *s);
void program_name(char *in,char *out,int size);
int chopup_args(ARGS *arg,char *buf, int *argc, char **argv[]);
#ifdef HEADER_X509_H
int dump_cert_text(BIO *out, X509 *x);
void print_name(BIO *out, const char *title, X509_NAME *nm, unsigned long lflags);
#endif
int set_cert_ex(unsigned long *flags, const char *arg);
int set_name_ex(unsigned long *flags, const char *arg);
int set_ext_copy(int *copy_type, const char *arg);
int copy_extensions(X509 *x, X509_REQ *req, int copy_type);
int app_passwd(BIO *err, char *arg1, char *arg2, char **pass1, char **pass2);
int add_oid_section(BIO *err, CONF *conf);
X509 *load_cert(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *cert_descrip);
EVP_PKEY *load_key(BIO *err, const char *file, int format, int maybe_stdin,
	const char *pass, ENGINE *e, const char *key_descrip);
EVP_PKEY *load_pubkey(BIO *err, const char *file, int format, int maybe_stdin,
	const char *pass, ENGINE *e, const char *key_descrip);
STACK_OF(X509) *load_certs(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *cert_descrip);
STACK_OF(X509_CRL) *load_crls(BIO *err, const char *file, int format,
	const char *pass, ENGINE *e, const char *cert_descrip);
X509_STORE *setup_verify(BIO *bp, char *CAfile, char *CApath);
#ifndef OPENSSL_NO_ENGINE
ENGINE *setup_engine(BIO *err, const char *engine, int debug);
#endif

#ifndef OPENSSL_NO_OCSP
OCSP_RESPONSE *process_responder(BIO *err, OCSP_REQUEST *req,
			char *host, char *path, char *port, int use_ssl,
			STACK_OF(CONF_VALUE) *headers,
			int req_timeout);
#endif

int load_config(BIO *err, CONF *cnf);
char *make_config_name(void);

/* Functions defined in ca.c and also used in ocsp.c */
int unpack_revinfo(ASN1_TIME **prevtm, int *preason, ASN1_OBJECT **phold,
			ASN1_GENERALIZEDTIME **pinvtm, const char *str);

#define DB_type         0
#define DB_exp_date     1
#define DB_rev_date     2
#define DB_serial       3       /* index - unique */
#define DB_file         4       
#define DB_name         5       /* index - unique when active and not disabled */
#define DB_NUMBER       6

#define DB_TYPE_REV	'R'
#define DB_TYPE_EXP	'E'
#define DB_TYPE_VAL	'V'

typedef struct db_attr_st
	{
	int unique_subject;
	} DB_ATTR;
typedef struct ca_db_st
	{
	DB_ATTR attributes;
	TXT_DB *db;
	} CA_DB;

BIGNUM *load_serial(char *serialfile, int create, ASN1_INTEGER **retai);
int save_serial(char *serialfile, char *suffix, BIGNUM *serial, ASN1_INTEGER **retai);
int rotate_serial(char *serialfile, char *new_suffix, char *old_suffix);
int rand_serial(BIGNUM *b, ASN1_INTEGER *ai);
CA_DB *load_index(char *dbfile, DB_ATTR *dbattr);
int index_index(CA_DB *db);
int save_index(const char *dbfile, const char *suffix, CA_DB *db);
int rotate_index(const char *dbfile, const char *new_suffix, const char *old_suffix);
void free_index(CA_DB *db);
#define index_name_cmp_noconst(a, b) \
	index_name_cmp((const OPENSSL_CSTRING *)CHECKED_PTR_OF(OPENSSL_STRING, a), \
	(const OPENSSL_CSTRING *)CHECKED_PTR_OF(OPENSSL_STRING, b))
int index_name_cmp(const OPENSSL_CSTRING *a, const OPENSSL_CSTRING *b);
int parse_yesno(const char *str, int def);

X509_NAME *parse_name(char *str, long chtype, int multirdn);
int args_verify(char ***pargs, int *pargc,
			int *badarg, BIO *err, X509_VERIFY_PARAM **pm);
void policies_print(BIO *out, X509_STORE_CTX *ctx);
int bio_to_mem(unsigned char **out, int maxlen, BIO *in);
int pkey_ctrl_string(EVP_PKEY_CTX *ctx, char *value);
int init_gen_str(BIO *err, EVP_PKEY_CTX **pctx,
			const char *algname, ENGINE *e, int do_param);
int do_X509_sign(BIO *err, X509 *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts);
int do_X509_REQ_sign(BIO *err, X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts);
int do_X509_CRL_sign(BIO *err, X509_CRL *x, EVP_PKEY *pkey, const EVP_MD *md,
			STACK_OF(OPENSSL_STRING) *sigopts);
#ifndef OPENSSL_NO_PSK
extern char *psk_key;
#endif
#ifndef OPENSSL_NO_JPAKE
void jpake_client_auth(BIO *out, BIO *conn, const char *secret);
void jpake_server_auth(BIO *out, BIO *conn, const char *secret);
#endif

#if !defined(OPENSSL_NO_TLSEXT) && !defined(OPENSSL_NO_NEXTPROTONEG)
unsigned char *next_protos_parse(unsigned short *outlen, const char *in);
#endif  /* !OPENSSL_NO_TLSEXT && !OPENSSL_NO_NEXTPROTONEG */

#define FORMAT_UNDEF    0
#define FORMAT_ASN1     1
#define FORMAT_TEXT     2
#define FORMAT_PEM      3
#define FORMAT_NETSCAPE 4
#define FORMAT_PKCS12   5
#define FORMAT_SMIME    6
#define FORMAT_ENGINE   7
#define FORMAT_IISSGC	8	/* XXX this stupid macro helps us to avoid
				 * adding yet another param to load_*key() */
#define FORMAT_PEMRSA	9	/* PEM RSAPubicKey format */
#define FORMAT_ASN1RSA	10	/* DER RSAPubicKey format */
#define FORMAT_MSBLOB	11	/* MS Key blob format */
#define FORMAT_PVK	12	/* MS PVK file format */

#define EXT_COPY_NONE	0
#define EXT_COPY_ADD	1
#define EXT_COPY_ALL	2

#define NETSCAPE_CERT_HDR	"certificate"

#define APP_PASS_LEN	1024

#define SERIAL_RAND_BITS	64

int app_isdir(const char *);
int raw_read_stdin(void *,int);
int raw_write_stdout(const void *,int);

#define TM_START	0
#define TM_STOP		1
double app_tminterval (int stop,int usertime);

#define OPENSSL_NO_SSL_INTERN

#endif
