/*
 * Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>
#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_SHA1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>

#define PROG pkcs12_main

const EVP_CIPHER *enc;


#define NOKEYS		0x1
#define NOCERTS 	0x2
#define INFO		0x4
#define CLCERTS		0x8
#define CACERTS		0x10

int get_cert_chain (X509 *cert, X509_STORE *store, STACK_OF(X509) **chain);
int dump_certs_keys_p12(BIO *out, PKCS12 *p12, char *pass, int passlen, int options, char *pempass);
int dump_certs_pkeys_bags(BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags, char *pass,
			  int passlen, int options, char *pempass);
int dump_certs_pkeys_bag(BIO *out, PKCS12_SAFEBAG *bags, char *pass, int passlen, int options, char *pempass);
int print_attribs(BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst,const char *name);
void hex_prin(BIO *out, unsigned char *buf, int len);
int alg_print(BIO *x, X509_ALGOR *alg);
int cert_load(BIO *in, STACK_OF(X509) *sk);
static int set_pbe(int *ppbe, const char *str);

int pkcs12_export(
		const char* infile,// -in
		const char* keyname,//-inKey
		const char* certfile,//-certfile
		const char* outfile,//-out
		const char* passout)
{
    ENGINE *e = NULL;
    BIO *in=NULL, *out = NULL;
    char **args;
    char *name = NULL;
    char *csp_name = NULL;
    int add_lmk = 0;
    PKCS12 *p12 = NULL;
    char pass[50], macpass[50];
    int export_cert = 0;
    int options = 0;
    int chain = 0;
    int badarg = 0;
    int iter = PKCS12_DEFAULT_ITER;
    int maciter = PKCS12_DEFAULT_ITER;
    int twopass = 0;
    int keytype = 0;
    int cert_pbe;
    int key_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
    int ret = 1;
    int macver = 1;
    int noprompt = 0;
    STACK_OF(OPENSSL_STRING) *canames = NULL;
    char *cpass = NULL, *mpass = NULL;
    char *passargin = NULL, *passargout = NULL, *passarg = NULL;
	char *passin = NULL;
    char *inrand = NULL;
    char *macalg = NULL;
    char *CApath = NULL, *CAfile = NULL;
#ifndef OPENSSL_NO_ENGINE
    char *engine=NULL;
#endif

	opensslInit():

#ifdef OPENSSL_FIPS
    if (FIPS_mode())
	cert_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
    else
#endif
    cert_pbe = NID_pbe_WithSHA1And40BitRC2_CBC;

    enc = EVP_des_ede3_cbc();
    if (bio_err == NULL ) bio_err = BIO_new_fp (stderr, BIO_NOCLOSE);

	if (!load_config(bio_err, NULL))
		goto end;

#ifndef OPENSSL_NO_ENGINE
    e = setup_engine(bio_err, engine, 0);
#endif

    if(passarg) {
	if(export_cert) passargout = passarg;
	else passargin = passarg;
    }

    if(!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
	BIO_printf(bio_err, "Error getting passwords\n");
	goto end;
    }

    if(!cpass) {
    	if(export_cert) cpass = passout;
    	else cpass = passin;
    }

    if(cpass) {
	mpass = cpass;
	noprompt = 1;
    } else {
	cpass = pass;
	mpass = macpass;
    }

    if(export_cert || inrand) {
    	app_RAND_load_file(NULL, bio_err, (inrand != NULL));
        if (inrand != NULL)
		BIO_printf(bio_err,"%ld semi-random bytes loaded\n",
			app_RAND_load_files(inrand));
    }
    ERR_load_crypto_strings();

#ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("read files");
#endif

    if (!infile) in = BIO_new_fp(stdin, BIO_NOCLOSE);
    else in = BIO_new_file(infile, "rb");
    if (!in) {
	    BIO_printf(bio_err, "Error opening input file %s\n",
						infile ? infile : "<stdin>");
	    perror (infile);
	    goto end;
   }

#ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
    CRYPTO_push_info("write files");
#endif

    if (!outfile) {
	out = BIO_new_fp(stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
	{
	    BIO *tmpbio = BIO_new(BIO_f_linebuffer());
	    out = BIO_push(tmpbio, out);
	}
#endif
    } else out = BIO_new_file(outfile, "wb");
    if (!out) {
	BIO_printf(bio_err, "Error opening output file %s\n",
						outfile ? outfile : "<stdout>");
	perror (outfile);
	goto end;
    }
    if (twopass) {
#ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("read MAC password");
#endif
	if(EVP_read_pw_string (macpass, sizeof macpass, "Enter MAC Password:", export_cert))
	{
    	    BIO_printf (bio_err, "Can't read Password\n");
    	    goto end;
       	}
#ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
#endif
    }

    if (export_cert) {
	EVP_PKEY *key = NULL;
	X509 *ucert = NULL, *x = NULL;
	STACK_OF(X509) *certs=NULL;
	const EVP_MD *macmd = NULL;
	unsigned char *catmp = NULL;
	int i;

	if ((options & (NOCERTS|NOKEYS)) == (NOCERTS|NOKEYS))
		{	
		BIO_printf(bio_err, "Nothing to do!\n");
		goto export_end;
		}

	if (options & NOCERTS)
		chain = 0;

#ifdef CRYPTO_MDEBUG
	CRYPTO_push_info("process -export_cert");
	CRYPTO_push_info("reading private key");
#endif
	if (!(options & NOKEYS))
		{
		key = load_key(bio_err, keyname ? keyname : infile,
				FORMAT_PEM, 1, passin, e, "private key");
		if (!key)
			goto export_end;
		}

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("reading certs from input");
#endif

	/* Load in all certs in input file */
	if(!(options & NOCERTS))
		{
		certs = load_certs(bio_err, infile, FORMAT_PEM, NULL, e,
							"certificates");
		if (!certs)
			goto export_end;

		if (key)
			{
			/* Look for matching private key */
			for(i = 0; i < sk_X509_num(certs); i++)
				{
				x = sk_X509_value(certs, i);
				if(X509_check_private_key(x, key))
					{
					ucert = x;
					/* Zero keyid and alias */
					X509_keyid_set1(ucert, NULL, 0);
					X509_alias_set1(ucert, NULL, 0);
					/* Remove from list */
					(void)sk_X509_delete(certs, i);
					break;
					}
				}
			if (!ucert)
				{
				BIO_printf(bio_err, "No certificate matches private key\n");
				goto export_end;
				}
			}

		}

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("reading certs from input 2");
#endif

	/* Add any more certificates asked for */
	if(certfile)
		{
		STACK_OF(X509) *morecerts=NULL;
		if(!(morecerts = load_certs(bio_err, certfile, FORMAT_PEM,
					    NULL, e,
					    "certificates from certfile")))
			goto export_end;
		while(sk_X509_num(morecerts) > 0)
			sk_X509_push(certs, sk_X509_shift(morecerts));
		sk_X509_free(morecerts);
 		}

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("reading certs from certfile");
#endif

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("building chain");
#endif

	/* If chaining get chain from user cert */
	if (chain) {
        	int vret;
		STACK_OF(X509) *chain2;
		X509_STORE *store = X509_STORE_new();
		if (!store)
			{
			BIO_printf (bio_err, "Memory allocation error\n");
			goto export_end;
			}
		if (!X509_STORE_load_locations(store, CAfile, CApath))
			X509_STORE_set_default_paths (store);

		vret = get_cert_chain (ucert, store, &chain2);
		X509_STORE_free(store);

		if (!vret) {
		    /* Exclude verified certificate */
		    for (i = 1; i < sk_X509_num (chain2) ; i++) 
			sk_X509_push(certs, sk_X509_value (chain2, i));
		    /* Free first certificate */
		    X509_free(sk_X509_value(chain2, 0));
		    sk_X509_free(chain2);
		} else {
			if (vret >= 0)
				BIO_printf (bio_err, "Error %s getting chain.\n",
					X509_verify_cert_error_string(vret));
			else
				ERR_print_errors(bio_err);
			goto export_end;
		}			
    	}

	/* Add any CA names */

	for (i = 0; i < sk_OPENSSL_STRING_num(canames); i++)
		{
		catmp = (unsigned char *)sk_OPENSSL_STRING_value(canames, i);
		X509_alias_set1(sk_X509_value(certs, i), catmp, -1);
		}

	if (csp_name && key)
		EVP_PKEY_add1_attr_by_NID(key, NID_ms_csp_name,
				MBSTRING_ASC, (unsigned char *)csp_name, -1);

	if (add_lmk && key)
		EVP_PKEY_add1_attr_by_NID(key, NID_LocalKeySet, 0, NULL, -1);

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("reading password");
#endif

	if(!noprompt &&
		EVP_read_pw_string(pass, sizeof pass, "Enter Export Password:", 1))
		{
	    	BIO_printf (bio_err, "Can't read Password\n");
	    	goto export_end;
        	}
	if (!twopass) BUF_strlcpy(macpass, pass, sizeof macpass);

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("creating PKCS#12 structure");
#endif

	p12 = PKCS12_create(cpass, name, key, ucert, certs,
				key_pbe, cert_pbe, iter, -1, keytype);

	if (!p12)
		{
	    	ERR_print_errors (bio_err);
		goto export_end;
		}

	if (macalg)
		{
		macmd = EVP_get_digestbyname(macalg);
		if (!macmd)
			{
			BIO_printf(bio_err, "Unknown digest algorithm %s\n", 
						macalg);
			}
		}

	if (maciter != -1)
		PKCS12_set_mac(p12, mpass, -1, NULL, 0, maciter, macmd);

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_push_info("writing pkcs12");
#endif

	i2d_PKCS12_bio(out, p12);

	ret = 0;

    export_end:
#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
	CRYPTO_pop_info();
	CRYPTO_push_info("process -export_cert: freeing");
#endif

	if (key) EVP_PKEY_free(key);
	if (certs) sk_X509_pop_free(certs, X509_free);
	if (ucert) X509_free(ucert);

#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
#endif
	goto end;
	
    }

    if (!(p12 = d2i_PKCS12_bio (in, NULL))) {
	ERR_print_errors(bio_err);
	goto end;
    }

#ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("read import password");
#endif
    if(!noprompt && EVP_read_pw_string(pass, sizeof pass, "Enter Import Password:", 0)) {
	BIO_printf (bio_err, "Can't read Password\n");
	goto end;
    }
#ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
#endif

    if (!twopass) BUF_strlcpy(macpass, pass, sizeof macpass);

    if ((options & INFO) && p12->mac) BIO_printf (bio_err, "MAC Iteration %ld\n", p12->mac->iter ? ASN1_INTEGER_get (p12->mac->iter) : 1);
    if(macver) {
#ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("verify MAC");
#endif
	/* If we enter empty password try no password first */
	if(!mpass[0] && PKCS12_verify_mac(p12, NULL, 0)) {
		/* If mac and crypto pass the same set it to NULL too */
		if(!twopass) cpass = NULL;
	} else if (!PKCS12_verify_mac(p12, mpass, -1)) {
	    BIO_printf (bio_err, "Mac verify error: invalid password?\n");
	    ERR_print_errors (bio_err);
	    goto end;
	}
	BIO_printf (bio_err, "MAC verified OK\n");
#ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
#endif
    }

#ifdef CRYPTO_MDEBUG
    CRYPTO_push_info("output keys and certificates");
#endif
    if (!dump_certs_keys_p12 (out, p12, cpass, -1, options, passout)) {
	BIO_printf(bio_err, "Error outputting keys and certificates\n");
	ERR_print_errors (bio_err);
	goto end;
    }
#ifdef CRYPTO_MDEBUG
    CRYPTO_pop_info();
#endif
    ret = 0;
 end:
    if (p12) PKCS12_free(p12);
    if(export_cert || inrand) app_RAND_write_file(NULL, bio_err);
#ifdef CRYPTO_MDEBUG
    CRYPTO_remove_all_info();
#endif
    BIO_free(in);
    BIO_free_all(out);
    if (canames) sk_OPENSSL_STRING_free(canames);
    if(passin) OPENSSL_free(passin);
    if(passout) OPENSSL_free(passout);
    apps_shutdown();
    OPENSSL_EXIT(ret);
}

int dump_certs_keys_p12 (BIO *out, PKCS12 *p12, char *pass,
	     int passlen, int options, char *pempass)
{
	STACK_OF(PKCS7) *asafes = NULL;
	STACK_OF(PKCS12_SAFEBAG) *bags;
	int i, bagnid;
	int ret = 0;
	PKCS7 *p7;

	if (!( asafes = PKCS12_unpack_authsafes(p12))) return 0;
	for (i = 0; i < sk_PKCS7_num (asafes); i++) {
		p7 = sk_PKCS7_value (asafes, i);
		bagnid = OBJ_obj2nid (p7->type);
		if (bagnid == NID_pkcs7_data) {
			bags = PKCS12_unpack_p7data(p7);
			if (options & INFO) BIO_printf (bio_err, "PKCS7 Data\n");
		} else if (bagnid == NID_pkcs7_encrypted) {
			if (options & INFO) {
				BIO_printf(bio_err, "PKCS7 Encrypted data: ");
				alg_print(bio_err, 
					p7->d.encrypted->enc_data->algorithm);
			}
			bags = PKCS12_unpack_p7encdata(p7, pass, passlen);
		} else continue;
		if (!bags) goto err;
	    	if (!dump_certs_pkeys_bags (out, bags, pass, passlen, 
						 options, pempass)) {
			sk_PKCS12_SAFEBAG_pop_free (bags, PKCS12_SAFEBAG_free);
			goto err;
		}
		sk_PKCS12_SAFEBAG_pop_free (bags, PKCS12_SAFEBAG_free);
		bags = NULL;
	}
	ret = 1;

	err:

	if (asafes)
		sk_PKCS7_pop_free (asafes, PKCS7_free);
	return ret;
}

int dump_certs_pkeys_bags (BIO *out, STACK_OF(PKCS12_SAFEBAG) *bags,
			   char *pass, int passlen, int options, char *pempass)
{
	int i;
	for (i = 0; i < sk_PKCS12_SAFEBAG_num (bags); i++) {
		if (!dump_certs_pkeys_bag (out,
					   sk_PKCS12_SAFEBAG_value (bags, i),
					   pass, passlen,
					   options, pempass))
		    return 0;
	}
	return 1;
}

int dump_certs_pkeys_bag (BIO *out, PKCS12_SAFEBAG *bag, char *pass,
	     int passlen, int options, char *pempass)
{
	EVP_PKEY *pkey;
	PKCS8_PRIV_KEY_INFO *p8;
	X509 *x509;
	
	switch (M_PKCS12_bag_type(bag))
	{
	case NID_keyBag:
		if (options & INFO) BIO_printf (bio_err, "Key bag\n");
		if (options & NOKEYS) return 1;
		print_attribs (out, bag->attrib, "Bag Attributes");
		p8 = bag->value.keybag;
		if (!(pkey = EVP_PKCS82PKEY (p8))) return 0;
		print_attribs (out, p8->attributes, "Key Attributes");
		PEM_write_bio_PrivateKey (out, pkey, enc, NULL, 0, NULL, pempass);
		EVP_PKEY_free(pkey);
	break;

	case NID_pkcs8ShroudedKeyBag:
		if (options & INFO) {
			BIO_printf (bio_err, "Shrouded Keybag: ");
			alg_print (bio_err, bag->value.shkeybag->algor);
		}
		if (options & NOKEYS) return 1;
		print_attribs (out, bag->attrib, "Bag Attributes");
		if (!(p8 = PKCS12_decrypt_skey(bag, pass, passlen)))
				return 0;
		if (!(pkey = EVP_PKCS82PKEY (p8))) {
			PKCS8_PRIV_KEY_INFO_free(p8);
			return 0;
		}
		print_attribs (out, p8->attributes, "Key Attributes");
		PKCS8_PRIV_KEY_INFO_free(p8);
		PEM_write_bio_PrivateKey (out, pkey, enc, NULL, 0, NULL, pempass);
		EVP_PKEY_free(pkey);
	break;

	case NID_certBag:
		if (options & INFO) BIO_printf (bio_err, "Certificate bag\n");
		if (options & NOCERTS) return 1;
                if (PKCS12_get_attr(bag, NID_localKeyID)) {
			if (options & CACERTS) return 1;
		} else if (options & CLCERTS) return 1;
		print_attribs (out, bag->attrib, "Bag Attributes");
		if (M_PKCS12_cert_bag_type(bag) != NID_x509Certificate )
								 return 1;
		if (!(x509 = PKCS12_certbag2x509(bag))) return 0;
		dump_cert_text (out, x509);
		PEM_write_bio_X509 (out, x509);
		X509_free(x509);
	break;

	case NID_safeContentsBag:
		if (options & INFO) BIO_printf (bio_err, "Safe Contents bag\n");
		print_attribs (out, bag->attrib, "Bag Attributes");
		return dump_certs_pkeys_bags (out, bag->value.safes, pass,
							    passlen, options, pempass);
					
	default:
		BIO_printf (bio_err, "Warning unsupported bag type: ");
		i2a_ASN1_OBJECT (bio_err, bag->type);
		BIO_printf (bio_err, "\n");
		return 1;
	break;
	}
	return 1;
}

/* Given a single certificate return a verified chain or NULL if error */

/* Hope this is OK .... */

int get_cert_chain (X509 *cert, X509_STORE *store, STACK_OF(X509) **chain)
{
	X509_STORE_CTX store_ctx;
	STACK_OF(X509) *chn;
	int i = 0;

	/* FIXME: Should really check the return status of X509_STORE_CTX_init
	 * for an error, but how that fits into the return value of this
	 * function is less obvious. */
	X509_STORE_CTX_init(&store_ctx, store, cert, NULL);
	if (X509_verify_cert(&store_ctx) <= 0) {
		i = X509_STORE_CTX_get_error (&store_ctx);
		if (i == 0)
			/* avoid returning 0 if X509_verify_cert() did not
			 * set an appropriate error value in the context */
			i = -1;
		chn = NULL;
		goto err;
	} else
		chn = X509_STORE_CTX_get1_chain(&store_ctx);
err:
	X509_STORE_CTX_cleanup(&store_ctx);
	*chain = chn;
	
	return i;
}	

int alg_print (BIO *x, X509_ALGOR *alg)
{
	PBEPARAM *pbe;
	const unsigned char *p;
	p = alg->parameter->value.sequence->data;
	pbe = d2i_PBEPARAM(NULL, &p, alg->parameter->value.sequence->length);
	if (!pbe)
		return 1;
	BIO_printf (bio_err, "%s, Iteration %ld\n", 
		OBJ_nid2ln(OBJ_obj2nid(alg->algorithm)),
		ASN1_INTEGER_get(pbe->iter));
	PBEPARAM_free (pbe);
	return 1;
}

/* Load all certificates from a given file */

int cert_load(BIO *in, STACK_OF(X509) *sk)
{
	int ret;
	X509 *cert;
	ret = 0;
#ifdef CRYPTO_MDEBUG
	CRYPTO_push_info("cert_load(): reading one cert");
#endif
	while((cert = PEM_read_bio_X509(in, NULL, NULL, NULL))) {
#ifdef CRYPTO_MDEBUG
		CRYPTO_pop_info();
#endif
		ret = 1;
		sk_X509_push(sk, cert);
#ifdef CRYPTO_MDEBUG
		CRYPTO_push_info("cert_load(): reading one cert");
#endif
	}
#ifdef CRYPTO_MDEBUG
	CRYPTO_pop_info();
#endif
	if(ret) ERR_clear_error();
	return ret;
}

/* Generalised attribute print: handle PKCS#8 and bag attributes */

int print_attribs (BIO *out, STACK_OF(X509_ATTRIBUTE) *attrlst,const char *name)
{
	X509_ATTRIBUTE *attr;
	ASN1_TYPE *av;
	char *value;
	int i, attr_nid;
	if(!attrlst) {
		BIO_printf(out, "%s: <No Attributes>\n", name);
		return 1;
	}
	if(!sk_X509_ATTRIBUTE_num(attrlst)) {
		BIO_printf(out, "%s: <Empty Attributes>\n", name);
		return 1;
	}
	BIO_printf(out, "%s\n", name);
	for(i = 0; i < sk_X509_ATTRIBUTE_num(attrlst); i++) {
		attr = sk_X509_ATTRIBUTE_value(attrlst, i);
		attr_nid = OBJ_obj2nid(attr->object);
		BIO_printf(out, "    ");
		if(attr_nid == NID_undef) {
			i2a_ASN1_OBJECT (out, attr->object);
			BIO_printf(out, ": ");
		} else BIO_printf(out, "%s: ", OBJ_nid2ln(attr_nid));

		if(sk_ASN1_TYPE_num(attr->value.set)) {
			av = sk_ASN1_TYPE_value(attr->value.set, 0);
			switch(av->type) {
				case V_ASN1_BMPSTRING:
        			value = OPENSSL_uni2asc(av->value.bmpstring->data,
                                	       av->value.bmpstring->length);
				BIO_printf(out, "%s\n", value);
				OPENSSL_free(value);
				break;

				case V_ASN1_OCTET_STRING:
				hex_prin(out, av->value.octet_string->data,
					av->value.octet_string->length);
				BIO_printf(out, "\n");	
				break;

				case V_ASN1_BIT_STRING:
				hex_prin(out, av->value.bit_string->data,
					av->value.bit_string->length);
				BIO_printf(out, "\n");	
				break;

				default:
					BIO_printf(out, "<Unsupported tag %d>\n", av->type);
				break;
			}
		} else BIO_printf(out, "<No Values>\n");
	}
	return 1;
}

void hex_prin(BIO *out, unsigned char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) BIO_printf (out, "%02X ", buf[i]);
}

static int set_pbe(BIO *err, int *ppbe, const char *str)
	{
	if (!str)
		return 0;
	if (!strcmp(str, "NONE"))
		{
		*ppbe = -1;
		return 1;
		}
	*ppbe=OBJ_txt2nid(str);
	if (*ppbe == NID_undef)
		{
		BIO_printf(bio_err, "Unknown PBE algorithm %s\n", str);
		return 0;
		}
	return 1;
	}
			
#endif
