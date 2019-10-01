Name:           mfcoin
Version:        0.6.3
Release:        1%{?dist}
Summary:        MFCoin Wallet
Group:          Applications/Internet
Vendor:         MFCoin
License:        GPLv3
URL:            https://www.mfcoin.net
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:  autoconf automake libtool gcc-c++ openssl-devel >= 1:1.0.2d libdb4-devel libdb4-cxx-devel miniupnpc-devel boost-devel boost-static
Requires:       openssl >= 1:1.0.2d libdb4 libdb4-cxx miniupnpc logrotate

%description
MFCoin Wallet

%prep
%setup -q

%build
./autogen.sh
./configure
make

%install
%{__rm} -rf $RPM_BUILD_ROOT
%{__mkdir} -p $RPM_BUILD_ROOT%{_bindir} $RPM_BUILD_ROOT/etc/mfcoin $RPM_BUILD_ROOT/etc/ssl/mfc $RPM_BUILD_ROOT/var/lib/mfc/.mfcoin $RPM_BUILD_ROOT/usr/lib/systemd/system $RPM_BUILD_ROOT/etc/logrotate.d
%{__install} -m 755 src/mfcoind $RPM_BUILD_ROOT%{_bindir}
%{__install} -m 755 src/mfcoin-cli $RPM_BUILD_ROOT%{_bindir}
%{__install} -m 600 contrib/redhat/mfcoin.conf $RPM_BUILD_ROOT/var/lib/mfc/.mfcoin
%{__install} -m 644 contrib/redhat/mfcoind.service $RPM_BUILD_ROOT/usr/lib/systemd/system
%{__install} -m 644 contrib/redhat/mfcoind.logrotate $RPM_BUILD_ROOT/etc/logrotate.d/mfcoind
%{__mv} -f contrib/redhat/mfc $RPM_BUILD_ROOT%{_bindir}

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pretrans
getent passwd mfc >/dev/null && { [ -f /usr/bin/mfcoind ] || { echo "Looks like user 'mfc' already exists and have to be deleted before continue."; exit 1; }; } || useradd -r -M -d /var/lib/mfc -s /bin/false mfc

%post
[ $1 == 1 ] && {
  sed -i -e "s/\(^rpcpassword=MySuperPassword\)\(.*\)/rpcpassword=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)/" /var/lib/mfc/.mfcoin/mfcoin.conf
  openssl req -nodes -x509 -newkey rsa:4096 -keyout /etc/ssl/mfc/mfcoin.key -out /etc/ssl/mfc/mfcoin.crt -days 3560 -subj /C=US/ST=Oregon/L=Portland/O=IT/CN=mfcoin.mfc
  ln -sf /var/lib/mfc/.mfcoin/mfcoin.conf /etc/mfcoin/mfcoin.conf
  ln -sf /etc/ssl/mfc /etc/mfcoin/certs
  chown mfc.mfc /etc/ssl/mfc/mfcoin.key /etc/ssl/mfc/mfcoin.crt
  chmod 600 /etc/ssl/mfc/mfcoin.key
} || exit 0

%posttrans
[ -f /var/lib/mfc/.mfcoin/addr.dat ] && { cd /var/lib/mfc/.mfcoin && rm -rf database addr.dat nameindex* blk* *.log .lock; }
sed -i -e 's|rpcallowip=\*|rpcallowip=0.0.0.0/0|' /var/lib/mfc/.mfcoin/mfcoin.conf
systmfctl daemon-reload
systmfctl status mfcoind >/dev/null && systmfctl restart mfcoind || exit 0

%preun
[ $1 == 0 ] && {
  systmfctl is-enabled mfcoind >/dev/null && systmfctl disable mfcoind >/dev/null || true
  systmfctl status mfcoind >/dev/null && systmfctl stop mfcoind >/dev/null || true
  pkill -9 -u mfc > /dev/null 2>&1
  getent passwd mfc >/dev/null && userdel mfc >/dev/null 2>&1 || true
  rm -f /etc/ssl/mfc/mfcoin.key /etc/ssl/mfc/mfcoin.crt /etc/mfcoin/mfcoin.conf /etc/mfcoin/certs
} || exit 0

%files
%doc COPYING
%attr(750,mfc,mfc) %dir /etc/mfcoin
%attr(750,mfc,mfc) %dir /etc/ssl/mfc
%attr(700,mfc,mfc) %dir /var/lib/mfc
%attr(700,mfc,mfc) %dir /var/lib/mfc/.mfcoin
%attr(600,mfc,mfc) %config(noreplace) /var/lib/mfc/.mfcoin/mfcoin.conf
%attr(4750,mfc,mfc) %{_bindir}/mfcoin-cli
%defattr(-,root,root)
%config(noreplace) /etc/logrotate.d/mfcoind
%{_bindir}/mfcoind
%{_bindir}/mfc
/usr/lib/systemd/system/mfcoind.service

%changelog
* Thu Aug 31 2017 Aspanta Limited <info@aspanta.com> 0.6.3
- There is no changelog available. Please refer to the CHANGELOG file or visit the website.
