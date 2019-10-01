//InfoCardExample.cpp by MFCoin developers
#include "InfoCardExample.h"
#include <QString>

//_____________________________________________
const QString InfoCardExample::corporate =
R"DEMO(# This is example of corp InfoCard for MFCoin Info system
# This card will be retrieved by reference to NVS from MFCSSL subsystem,
# or imported by another card.
#
# All fields are OPTIONAL
#
# Syntax:
# Keyword (whitespaces) value
# If keyword is omitted, value will be added as additional element
# into values list, reference by this keyword (see HomeAddress following).
#
# Card can contain reference to "parent" card(s) with syntax like:
# Import info:569dcc6b7aee11375b76:62615c3f6f62096b68bbc980c153917d505f8d24 

BusinessAddress
	Kucuk Ayasofya Cad. Kapi Agasi	# Free form address
	Sok. No:55			# Free form address 
	Sultanahmet, Fatih		# Free form address
	Istanbul				# City
	34400				# ZIP code
	Turkey				# Country
WorkPhone	+90-555-123-4568
Email		info@bubbleinflators.com
WEB		http://www.bubbleinflators.com
Facebook	BubbleInflators
Twitter		BubbleInflators
)DEMO";
//_____________________________________________
const QString InfoCardExample::user =
R"DEMO(# This is example of personal card for emerCoin Info system
# This card will be retrieved by reference to NVS from MFCSSL subsystem.
#
# All fields are OPTIONAL
#
# Syntax:
# Keyword (whitespaces) value
# If keyword is omitted, value will be added as additional element
# into values list, reference by this keyword (see HomeAddress following).
#
# Card can contain reference to "parent" card(s) with syntax like:
# Import info:569dcc6b7aee11375b76:62615c3f6f62096b68bbc980c153917d505f8d24	

Alias		superabdul		# Short name (username, login)
FirstName	Abdul			# First (short) name
LastName	Kurbashi Bey		# Remain part of full name
HomeAddress
	Sinan Pasa Mah. Hayrettin Iskelesi# Free form address
	Sok. No \#1			# Free form address 
	Besiktas, Besiktas		# Free form address
	Istanbul				# City
	34353				# ZIP code
	Turkey				# Country
HomePhone	+90-555-123-4567
CellPhone	+90-555-123-4569
Gender		M
Birthdate	1991-05-27		# May, 27, 1991
Email+		abdul@bubbleinflators.com
+WEB		http://www.bubbleinflators.com/superabdul
Facebook	Abdul.KurbashiBey
Twitter		AbdulKurbashiBey
MFC	EdvJ7b7zPL6gj5f8VNfX6zmVcftb35sKX2	# MFCoin payment address
BTC	1MkKuU78bikC2ACLspofQZnNb6Vz9AP1Np	# BitCoin payment address
)DEMO";
const QString InfoCardExample::emptyDoc = R"DEMO(#This is comment, ignored by InfoCard processor. It is marked with different color.
#To insert # sign itself, place \ sign before it.
#See examples in the right tabs on how to fill your InfoCard

Key1 Value
Key2 Value # Another comment
)DEMO";
