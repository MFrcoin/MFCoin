//OpenSslConfigWriter.cpp by MFCoin developers
#include "OpenSslConfigWriter.h"
#include "ShellImitation.h"
#include "CertLogger.h"
#include "Settings.h"

OpenSslConfigWriter::OpenSslConfigWriter(CertLogger*logger): _logger(logger) {
}
bool OpenSslConfigWriter::writeIfAbsent(const QString & subPath, const char* strContents, QString & error) {
	const QString path = Settings::certDir().absoluteFilePath(subPath);
	if(QFile::exists(path))
		return true;
	ShellImitation shell(_logger);
	return shell.write(path, QByteArray(strContents), error);
}
static const char ecConfig[] =
R"HOBOT([ ca ]
default_ca             = CA_CLIENT       # При подписи сертификатов
# использовать секцию CA_CLIENT

[ CA_CLIENT ]
dir                    = ./db            # Каталог для служебных файлов
certs                  = $dir/certs      # Каталог для сертификатов
new_certs_dir          = $dir/newcerts   # Каталог для новых сертификатов

database               = $dir/index.txt  # Файл с базой данных
# подписанных сертификатов
serial                 = $dir/serial     # Файл содержащий серийный номер
# сертификата
# (в шестнадцатиричном формате)
certificate            = CA-EC/mfcssl_ca.crt        # Файл сертификата CA
private_key            = CA-EC/mfcssl_ca.key        # Файл закрытого ключа CA

default_days           = 1825            # Срок действия подписываемого 
					 # сертификата - 5 лет
default_crl_days       = 7               # Срок действия CRL (см. $4)
default_md             = sha256          # Алгоритм подписи
policy                 = policy_anything # Название секции с описанием
# политики в отношении данных
# сертификата
x509_extensions=cert_v3		# For v3 certificates.

[ policy_anything ]
countryName            = optional        # Код страны - не обязателен
stateOrProvinceName    = optional        # ......
localityName           = optional        # ......
organizationName       = optional        # ......
organizationalUnitName = optional        # ......
commonName             = supplied        # ...... - обязателен
emailAddress           = optional        # ......
userId                 = optional	 # Reference to external info
#domainComponent	       = optional	 # MFC payment address
#surname		       = optional        # 
#givenName	       = optional        # 
#pseudonym	       = optional        # 
#

[ cert_v3 ]
# With the exception of 'CA:FALSE', there are PKIX recommendations for end-user
# # certificates that should not be able to sign other certificates.
# # 'CA:FALSE' is explicitely set because some software will malfunction without.

 subjectKeyIdentifier=	hash
 basicConstraints=	CA:FALSE
 keyUsage=		nonRepudiation, digitalSignature, keyEncipherment

 nsCertType=		client, email
 nsComment=		"MFCoin MFCSSL PKI"

 authorityKeyIdentifier=keyid:always,issuer:always
)HOBOT";
//____________________________________________________________________________
static const char rsaConfig[] =
R"HOBOT([ ca ]
default_ca             = CA_CLIENT       # При подписи сертификатов
# использовать секцию CA_CLIENT
        
[ CA_CLIENT ]
dir                    = ./db            # Каталог для служебных файлов
certs                  = $dir/certs      # Каталог для сертификатов
new_certs_dir          = $dir/newcerts   # Каталог для новых сертификатов

database               = $dir/index.txt  # Файл с базой данных
# подписанных сертификатов
serial                 = $dir/serial     # Файл содержащий серийный номер
# сертификата
# (в шестнадцатиричном формате)
certificate            = CA-RSA/mfcssl_ca.crt        # Файл сертификата CA
private_key            = CA-RSA/mfcssl_ca.key        # Файл закрытого ключа CA

default_days           = 1825            # Срок действия подписываемого 
					 # сертификата - 5 лет
default_crl_days       = 7               # Срок действия CRL (см. $4)
default_md             = md5             # Алгоритм подписи
policy                 = policy_anything # Название секции с описанием
# политики в отношении данных
# сертификата

[ policy_anything ]
countryName            = optional        # Код страны - не обязателен
stateOrProvinceName    = optional        # ......
localityName           = optional        # ......
organizationName       = optional        # ......
organizationalUnitName = optional        # ......
commonName             = supplied        # ...... - обязателен
emailAddress           = optional        # ......
userId                 = optional	 # Reference to external info
#domainComponent	       = optional	 # MFC payment address
#surname		       = optional        # 
#givenName	       = optional        # 
#pseudonym	       = optional        # 

)HOBOT";
//____________________________________________________________________________
static const char winConfig[] =
R"HOBOT(# $FreeBSD: release/9.1.0/crypto/openssl/apps/openssl.cnf 237998 2012-07-02 16:00:38Z jkim $
#
# OpenSSL example configuration file.
# This is mostly being used for generation of certificate requests.
#

# This definition stops the following lines choking if HOME isn't
# defined.
HOME			= .
RANDFILE		= $ENV::HOME/.rnd

# Extra OBJECT IDENTIFIER info:
#oid_file		= $ENV::HOME/.oid
oid_section		= new_oids

# To use this configuration file with the "-extfile" option of the
# "openssl x509" utility, name here the section containing the
# X.509v3 extensions to use:
# extensions		= 
# (Alternatively, use a configuration file that has only
# X.509v3 extensions in its main [= default] section.)

[ new_oids ]

# We can add new OIDs in here for use by 'ca' and 'req'.
# Add a simple OID like this:
# testoid1=1.2.3.4
# Or use config file substitution like this:
# testoid2=${testoid1}.5.6

####################################################################
[ ca ]
default_ca	= CA_default		# The default ca section

####################################################################
[ CA_default ]

dir		= ./sslCA		# Where everything is kept
certs		= $dir/certs		# Where the issued certs are kept
crl_dir		= $dir/crl		# Where the issued crl are kept
database	= $dir/index.txt	# database index file.
#unique_subject	= no			# Set to 'no' to allow creation of
					# several ctificates with same subject.
new_certs_dir	= $dir/newcerts		# default place for new certs.

certificate	= $dir/cacert.pem 	# The CA certificate
serial		= $dir/serial 		# The current serial number
crlnumber	= $dir/crlnumber	# the current crl number
					# must be commented out to leave a V1 CRL
crl		= $dir/crl.pem 		# The current CRL
private_key	= $dir/private/cakey.pem# The private key
RANDFILE	= $dir/private/.rand	# private random number file

x509_extensions	= usr_cert		# The extentions to add to the cert

# Comment out the following two lines for the "traditional"
# (and highly broken) format.
name_opt 	= ca_default		# Subject Name options
cert_opt 	= ca_default		# Certificate field options

# Extension copying option: use with caution.
# copy_extensions = copy

# Extensions to add to a CRL. Note: Netscape communicator chokes on V2 CRLs
# so this is commented out by default to leave a V1 CRL.
# crlnumber must also be commented out to leave a V1 CRL.
# crl_extensions	= crl_ext

default_days	= 3650			# how long to certify for
default_crl_days= 30			# how long before next CRL
default_md	= sha1			# which md to use.
preserve	= no			# keep passed DN ordering

# A few difference way of specifying how similar the request should look
# For type CA, the listed attributes must be the same, and the optional
# and supplied fields are just that :-)
policy		= policy_match

# For the CA policy
[ policy_match ]
countryName		= match
stateOrProvinceName	= match
organizationName	= match
organizationalUnitName	= optional
commonName		= supplied
emailAddress		= optional

# For the 'anything' policy
# At this point in time, you must list all acceptable 'object'
# types.
[ policy_anything ]
countryName		= optional
stateOrProvinceName	= optional
localityName		= optional
organizationName	= optional
organizationalUnitName	= optional
commonName		= supplied
emailAddress		= optional

####################################################################
[ req ]
default_bits		= 1024
default_keyfile 	= privkey.pem
distinguished_name	= req_distinguished_name
attributes		= req_attributes
x509_extensions	= v3_ca	# The extentions to add to the self signed cert

# Passwords for private keys if not present they will be prompted for
# input_password = secret
# output_password = secret

# This sets a mask for permitted string types. There are several options. 
# default: PrintableString, T61String, BMPString.
# pkix	 : PrintableString, BMPString.
# utf8only: only UTF8Strings.
# nombstr : PrintableString, T61String (no BMPStrings or UTF8Strings).
# MASK:XXXX a literal mask value.
# WARNING: current versions of Netscape crash on BMPStrings or UTF8Strings
# so use this option with caution!
string_mask = nombstr

# req_extensions = v3_req # The extensions to add to a certificate request

[ req_distinguished_name ]
countryName			= Country Name (2 letter code)
countryName_default		= US
countryName_min			= 2
countryName_max			= 2

stateOrProvinceName		= State or Province Name (full name)
stateOrProvinceName_default	= Maryland

localityName			= Locality name (city, town, etc)
localityName_default		= Potomac

0.organizationName		= Organization Name (eg, company)
0.organizationName_default	= MFCoin Foundation

# we can do this but it is not needed normally :-)
#1.organizationName		= Second Organization Name (eg, company)
#1.organizationName_default	= World Wide Web Pty Ltd

organizationalUnitName		= Organizational Unit Name (eg, section)
#organizationalUnitName_default	=

commonName			= Common Name (e.g. server FQDN or YOUR name)
commonName_max			= 64

emailAddress			= Email Address
emailAddress_max		= 64

# SET-ex3			= SET extension number 3

[ req_attributes ]
challengePassword		= A challenge password
challengePassword_min		= 4
challengePassword_max		= 20

unstructuredName		= An optional company name

[ usr_cert ]

# These extensions are added when 'ca' signs a request.

# This goes against PKIX guidelines but some CAs do it and some software
# requires this to avoid interpreting an end user certificate as a CA.

basicConstraints=CA:FALSE

# Here are some examples of the usage of nsCertType. If it is omitted
# the certificate can be used for anything *except* object signing.

# This is OK for an SSL server.
# nsCertType			= server

# For an object signing certificate this would be used.
# nsCertType = objsign

# For normal client use this is typical
# nsCertType = client, email

# and for everything including object signing:
# nsCertType = client, email, objsign

# This is typical in keyUsage for a client certificate.
# keyUsage = nonRepudiation, digitalSignature, keyEncipherment

# This will be displayed in Netscape's comment listbox.
nsComment			= "OpenSSL Generated Certificate"

# PKIX recommendations harmless if included in all certificates.
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer

# This stuff is for subjectAltName and issuerAltname.
# Import the email address.
# subjectAltName=email:copy
# An alternative to produce certificates that aren't
# deprecated according to PKIX.
# subjectAltName=email:move

# Copy subject details
# issuerAltName=issuer:copy

#nsCaRevocationUrl		= http://www.domain.dom/ca-crl.pem
#nsBaseUrl
#nsRevocationUrl
#nsRenewalUrl
#nsCaPolicyUrl
#nsSslServerName

[ v3_req ]

# Extensions to add to a certificate request

basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment

[ v3_ca ]


# Extensions for a typical CA


# PKIX recommendation.

subjectKeyIdentifier=hash

authorityKeyIdentifier=keyid:always,issuer:always

# This is what PKIX recommends but some broken software chokes on critical
# extensions.
#basicConstraints = critical,CA:true
# So we do this instead.
basicConstraints = CA:true

# Key usage: this is typical for a CA certificate. However since it will
# prevent it being used as an test self-signed certificate it is best
# left out by default.
# keyUsage = cRLSign, keyCertSign

# Some might want this also
# nsCertType = sslCA, emailCA

# Include email address in subject alt name: another PKIX recommendation
# subjectAltName=email:copy
# Copy issuer details
# issuerAltName=issuer:copy

# DER hex encoding of an extension: beware experts only!
# obj=DER:02:03
# Where 'obj' is a standard or added object
# You can even override a supported extension:
# basicConstraints= critical, DER:30:03:01:01:FF

[ crl_ext ]

# CRL extensions.
# Only issuerAltName and authorityKeyIdentifier make any sense in a CRL.

# issuerAltName=issuer:copy
authorityKeyIdentifier=keyid:always,issuer:always

[ proxy_cert_ext ]
# These extensions should be added when creating a proxy certificate

# This goes against PKIX guidelines but some CAs do it and some software
# requires this to avoid interpreting an end user certificate as a CA.

basicConstraints=CA:FALSE

# Here are some examples of the usage of nsCertType. If it is omitted
# the certificate can be used for anything *except* object signing.

# This is OK for an SSL server.
# nsCertType			= server

# For an object signing certificate this would be used.
# nsCertType = objsign

# For normal client use this is typical
# nsCertType = client, email

# and for everything including object signing:
# nsCertType = client, email, objsign

# This is typical in keyUsage for a client certificate.
# keyUsage = nonRepudiation, digitalSignature, keyEncipherment

# This will be displayed in Netscape's comment listbox.
nsComment			= "OpenSSL Generated Certificate"

# PKIX recommendations harmless if included in all certificates.
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer:always

# This stuff is for subjectAltName and issuerAltname.
# Import the email address.
# subjectAltName=email:copy
# An alternative to produce certificates that aren't
# deprecated according to PKIX.
# subjectAltName=email:move

# Copy subject details
# issuerAltName=issuer:copy

#nsCaRevocationUrl		= http://www.domain.dom/ca-crl.pem
#nsBaseUrl
#nsRevocationUrl
#nsRenewalUrl
#nsCaPolicyUrl
#nsSslServerName

# This really needs to be in place for it to be a proxy certificate.
proxyCertInfo=critical,language:id-ppl-anyLanguage,pathlen:3,policy:foo

)HOBOT";
//____________________________________________________________________________
static const char ecKey[] =
R"HOBOT(-----BEGIN EC PARAMETERS-----
BgUrgQQACg==
-----END EC PARAMETERS-----
-----BEGIN EC PRIVATE KEY-----
MHQCAQEEIMYxfnuh9SyX+xleY6Hpl5x0Q32W6TXgAYq0YoyCI5VqoAcGBSuBBAAK
oUQDQgAEJpsi7sbYqsSWtzzQ30aPCYDsJ8KGJDh8gL8KEDWK8f3kxQkartKzmrvc
45HuHj53XEL1YkeJ0T/wt5LV9+1MVA==
-----END EC PRIVATE KEY-----
)HOBOT";
static const char ecCrt[] =
R"HOBOT(-----BEGIN CERTIFICATE-----
MIICmDCCAj2gAwIBAgIJAODSa/QZoT5/MAoGCCqGSM49BAMCMGkxETAPBgNVBAoT
CEVtZXJDb2luMQwwCgYDVQQLEwNQS0kxDzANBgNVBAMTBkVNQ1NTTDEgMB4GCSqG
SIb3DQEJARYRdGVhbUBlbWVyY29pbi5jb20xEzARBgoJkiaJk/IsZAEBEwNFTUMw
IBcNMTYwMzA0MjM1NTU4WhgPMjExNjAyMDkyMzU1NThaMGkxETAPBgNVBAoTCEVt
ZXJDb2luMQwwCgYDVQQLEwNQS0kxDzANBgNVBAMTBkVNQ1NTTDEgMB4GCSqGSIb3
DQEJARYRdGVhbUBlbWVyY29pbi5jb20xEzARBgoJkiaJk/IsZAEBEwNFTUMwVjAQ
BgcqhkjOPQIBBgUrgQQACgNCAAQmmyLuxtiqxJa3PNDfRo8JgOwnwoYkOHyAvwoQ
NYrx/eTFCRqu0rOau9zjke4ePndcQvViR4nRP/C3ktX37UxUo4HOMIHLMB0GA1Ud
DgQWBBTVpM3ghYdPT4p0Ld1u+erh/cjnkTCBmwYDVR0jBIGTMIGQgBTVpM3ghYdP
T4p0Ld1u+erh/cjnkaFtpGswaTERMA8GA1UEChMIRW1lckNvaW4xDDAKBgNVBAsT
A1BLSTEPMA0GA1UEAxMGRU1DU1NMMSAwHgYJKoZIhvcNAQkBFhF0ZWFtQGVtZXJj
b2luLmNvbTETMBEGCgmSJomT8ixkAQETA0VNQ4IJAODSa/QZoT5/MAwGA1UdEwQF
MAMBAf8wCgYIKoZIzj0EAwIDSQAwRgIhAKwqadsTfTkYhAFT3vfAwNSNByMXpR3i
vgA7XnZFA5qAAiEA1ZGJ2R4odnXj6WpsrrCTUwfCMScS2jUfHu410yUGbbw=
-----END CERTIFICATE-----
)HOBOT";
//____________________________________________________________________________
static const char rsaKey[] =
R"HOBOT(-----BEGIN PRIVATE KEY-----
MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAuFXy29LbF9Lq7Syt
w4r8KU2kVqLmz/OZ0IscDcwFObznv6QCDj4Hpz8uHORmyUmfH0QKs5lGCT9lyTZJ
KYJeMwIDAQABAkAfNPXLf1P2IZACHRlBzIrKF0nmHOgEdpIouxRBxbNwxZ9Cjzdu
mrAXwwo/Hc166B8QmQGLwSRSbmOZ6jv0NEfBAiEA9S+iOq8ZQL20tW6696goUZ6L
RnDqf6rUOt8vwZhuQiECIQDAd0LehLpA+8n4KFWs24y7wRhVZeZnOUbrIdIn5OA9
0wIhAMq0dOUbej9CF7KgN0ck6SCBeRflppmh/BAoEO13PkDBAiANI5cjDbiRWx8M
m+RNaqeO4b3BhrVV8qkOwD5SjuNoFQIgc4U5ZuN6W93s2DbH+2iSw5c5Iql7lEyw
ajgr2A+m3K4=
-----END PRIVATE KEY-----
)HOBOT";
static const char rsaCrt[] =
R"HOBOT(-----BEGIN CERTIFICATE-----
MIICNzCCAeGgAwIBAgIJAM0KszlO0jEiMA0GCSqGSIb3DQEBBQUAMHYxETAPBgNV
BAoMCEVtZXJDb2luMQ8wDQYDVQQLDAZFTUNTU0wxGTAXBgNVBAMMEEVtZXJDb2lu
IFdXVyBQS0kxIDAeBgkqhkiG9w0BCQEWEXRlYW1AZW1lcmNvaW4uY29tMRMwEQYK
CZImiZPyLGQBAQwDRU1DMCAXDTE1MDQwODE4NTcyNVoYDzIxMTUwMzE1MTg1NzI1
WjB2MREwDwYDVQQKDAhFbWVyQ29pbjEPMA0GA1UECwwGRU1DU1NMMRkwFwYDVQQD
DBBFbWVyQ29pbiBXV1cgUEtJMSAwHgYJKoZIhvcNAQkBFhF0ZWFtQGVtZXJjb2lu
LmNvbTETMBEGCgmSJomT8ixkAQEMA0VNQzBcMA0GCSqGSIb3DQEBAQUAA0sAMEgC
QQC4VfLb0tsX0urtLK3DivwpTaRWoubP85nQixwNzAU5vOe/pAIOPgenPy4c5GbJ
SZ8fRAqzmUYJP2XJNkkpgl4zAgMBAAGjUDBOMB0GA1UdDgQWBBRxdx4/uGsAr6on
UZioj8Pih+DyljAfBgNVHSMEGDAWgBRxdx4/uGsAr6onUZioj8Pih+DyljAMBgNV
HRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA0EAUe6eKftSNRxS6jQ9s66ValAi5E3B
pNmW3lvRtW45dINrBBZN0mCtQxq0juW013RaK976jxhwu8h7ILAUVkbujg==
-----END CERTIFICATE-----
)HOBOT";
QString OpenSslConfigWriter::configPath() {
	return Settings::certDir().absoluteFilePath(winConfigPart());
}
QString OpenSslConfigWriter::winConfigPart() {
	return "CA-WINCONF/openssl.cnf";
}
QString OpenSslConfigWriter::checkAndWrite() {
	QString error;
	if(    !writeIfAbsent("CA-EC/ca.config", ecConfig, error)
		|| !writeIfAbsent("CA-EC/mfcssl_ca.key", ecKey, error)
		|| !writeIfAbsent("CA-EC/mfcssl_ca.crt", ecCrt, error)
		|| !writeIfAbsent("CA-RSA/ca.config", rsaConfig, error)
		|| !writeIfAbsent("CA-RSA/mfcssl_ca.key", rsaKey, error)
		|| !writeIfAbsent("CA-RSA/mfcssl_ca.crt", rsaCrt, error)
		|| !writeIfAbsent(winConfigPart(), winConfig, error))
		return error;
	return QString();
}
