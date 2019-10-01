#!/bin/sh
# Install script for MFCoin Daemin and 2 cli - mainnet/testnet
# Before install, you needed to create usernames/group:

MFCGROUP=mfcoin
MFCUSER=mfc
TMFCUSER=tmfc

DST=${1:-'/usr/local/bin'}

function install_cli() {
  cp mfcoin-cli $DST/$1
  chown $1:$MFCGROUP $DST/$1
  chmod 4750 $DST/$1
}

echo "Install mfcoin to: $DST"
cp mfcoind $DST/mfcoind
chown root:$MFCGROUP $DST/mfcoind
chmod 750  $DST/mfcoind

install_cli $MFCUSER
install_cli $TMFCUSER

