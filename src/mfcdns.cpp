/*
 * Simple DNS server for MFCoin project
 *
 * Lookup for names like "dns:some-name" in the local nameindex database.
 * Database is updated from blockchain, and keeps NMC-transactions.
 *
 * Supports standard RFC1034 UDP DNS protocol only
 *
 * Supported fields: A, AAAA, NS, PTR, MX, TXT, CNAME
 * Does not support: SOA, WKS, SRV
 * Does not support recursive query, authority zone and namezone transfers.
 * 
 *
 * Author: maxihatop
 *
 * This code can be used according BSD license:
 * http://en.wikipedia.org/wiki/BSD_licenses
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>

#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <ctype.h>

#include "namecoin.h"
#include "util.h"
#include "mfcdns.h"
#include "random.h"
#include "validation.h"
#include "base58.h"
#include "netbase.h"

/*---------------------------------------------------*/
/*
 * m_verbose legend:
 * 0 = disabled
 * 1 = error, DAP blocking
 * 2 = start/stop, set domains, etc
 * 3 = single statistic message for packet received
 * 4 = handle packets
 * 5 = details for handle packets
 * 6 = more details, debug info
 */
/*---------------------------------------------------*/

#define MAX_OUT  512	// Old DNS restricts UDP to 512 bytes; keep compatible
#define BUF_SIZE (2 * MAX_OUT)
#define MAX_TOK  64	// Maximal TokenQty in the vsl_list, like A=IP1,..,IPn
#define MAX_DOM  20	// Maximal domain level; min 10 is needed for NAPTR E164

#define VAL_SIZE (MAX_VALUE_LENGTH + 16)
#define DNS_PREFIX "dns"
#define REDEF_SYM  '~'

// HT offset contains it for ENUM SPFUN
#define ENUM_FLAG	(1 << 14)

/*---------------------------------------------------*/

#ifdef WIN32
int inet_pton(int af, const char *src, void *dst)
{
  struct sockaddr_storage ss;
  int size = sizeof(ss);
  char src_copy[INET6_ADDRSTRLEN+1];

  ZeroMemory(&ss, sizeof(ss));
  /* stupid non-const API */
  strncpy (src_copy, src, INET6_ADDRSTRLEN+1);
  src_copy[INET6_ADDRSTRLEN] = 0;

  if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
    switch(af) {
      case AF_INET:
    *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
    return 1;
      case AF_INET6:
    *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
    return 1;
    }
  }
  return 0;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
  if(size > 16) size = 16;
  uint8_t *p = (uint8_t *)src;
  while(size--)
    dst += sprintf(dst, "%02x:", *p++);
  dst[-1] = 0;
}

char *strsep(char **s, const char *ct)
{
    char *sstart = *s;
    char *end;

    if (sstart == NULL)
        return NULL;

    end = strpbrk(sstart, ct);
    if (end)
        *end++ = '\0';
    *s = end;
    return sstart;
}
#endif

/*---------------------------------------------------*/

MfcDns::MfcDns(const char *bind_ip, uint16_t port_no,
	  const char *gw_suffix, const char *allowed_suff, const char *local_fname, 
	  uint32_t dapsize, uint32_t daptreshold,
	  const char *enums, const char *tollfree, uint8_t verbose) 
    : m_status(-1), m_thread(StatRun, this) {

    // Clear vars [m_hdr..m_verbose)
    memset(&m_hdr, 0, &m_verbose - (uint8_t *)&m_hdr); // Clear previous state
    m_verbose = verbose;

    // Create and socket
    int ret = socket(PF_INET6, SOCK_DGRAM, 0);
    if(ret < 0) 
      throw runtime_error("MfcDns::MfcDns: Cannot create socket");
    m_sockfd = ret;

    struct sockaddr_in6 sin6;
    const int sin6len = sizeof(struct sockaddr_in6);
    memset(&sin6, 0, sin6len);
    sin6.sin6_port = htons(port_no);
    sin6.sin6_family = AF_INET6;

    if(*bind_ip == 0 || inet_pton(AF_INET6, bind_ip, &sin6.sin6_addr) != 1) {
      sin6.sin6_addr = in6addr_any;
      bind_ip = NULL;
    }

#if 0    
    char bind6ip[80];

    do { // ret here >= 0, since sock fd
      if(*bind_ip == 0) break;
      if(strchr(bind_ip, ':'))
        ret = ~inet_pton(AF_INET6, bind_ip, &sin6.sin6_addr);
//      else
//        ret = ~inet_pton(AF_INET, bind_ip, &sin6.sin_addr);

      // Convert bind_IP to IPV6 format
//      if(*bind_ip && strchr(bind_ip, ':') == NULL) {
//        sprintf(bind6ip, "::ffff:%s", bind_ip);
//        bind_ip = bind6ip;
//      }
//      ret = ~inet_pton(AF_INET6, bind_ip, &sin6.sin6_addr);
    } while(false);
    if(ret != ~1) {
      sin6.sin6_addr = in6addr_any;
      bind_ip = NULL;
    }
#endif

    int no = 0;
#ifdef WIN32
    if(setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&no, sizeof(no)) < 0)
#else
    if(setsockopt(m_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)) < 0)
#endif
      throw runtime_error("MfcDns::MfcDns: Cannot switch socket to IPV4 compatibility mode");

    if(::bind(m_sockfd, (struct sockaddr *)&sin6, sin6len) < 0) {
      char buf[80];
      sprintf(buf, "MfcDns::MfcDns: Cannot bind to port %u", port_no);
      throw runtime_error(buf);
    }

    // Upload Local DNS entries
    // Create temporary local buf on stack
    int local_len = 0;
    char local_tmp[1 << 15]; // max 32Kb
    FILE *flocal;
    uint8_t local_qty = 0;
    if(local_fname != NULL && (flocal = fopen(local_fname, "r")) != NULL) {
      char *rd = local_tmp;
      while(rd < local_tmp + (1 << 15) - 200 && fgets(rd, 200, flocal)) {
	if(*rd < '0' || *rd == ';')
	  continue;
	char *p = strchr(rd, '=');
	if(p == NULL)
	  continue;
	rd = strchr(p, 0);
        while(*--rd < 040) 
	  *rd = 0;
	rd += 2;
	local_qty++;
      } // while rd
      local_len = rd - local_tmp;
      fclose(flocal);
    }

    // Allocate memory
    int allowed_len = allowed_suff == NULL? 0 : strlen(allowed_suff);
    m_gw_suf_len    = gw_suffix    == NULL? 0 : strlen(gw_suffix);
    // Compute dots in the gw-suffix
    m_gw_suf_dots = 0;
    if(m_gw_suf_len)
      for(const char *p = gw_suffix; *p; p++)
        if(*p == '.') 
	  m_gw_suf_dots++;

    // Activate DAP only if specidied dapsize
    // If no memory, DAP is inactive - this is not critical problem
    m_dap_ht = NULL;
    if(dapsize) {
      dapsize += dapsize - 1;
      do m_dapmask = dapsize; while(dapsize &= dapsize - 1); // compute mask as 2^N
      m_dap_ht = (DNSAP*)calloc(m_dapmask, sizeof(DNSAP));
      m_dapmask--;
      m_daprand = GetRand(0xffffffff) | 1;
      m_dap_treshold = daptreshold;
    }

    m_value  = (char *)malloc(VAL_SIZE + BUF_SIZE + 2 + 
	    m_gw_suf_len + allowed_len + local_len + 4);
 
    if(m_value == NULL) 
      throw runtime_error("MfcDns::MfcDns: Cannot allocate buffer");

    // Temporary use m_value for parse enum-verifiers and toll-free lists, if exist

    if(enums && *enums) {
      char *str = strcpy(m_value, enums);
      Verifier empty_ver;
      while(char *p_tok = strsep(&str, "|,"))
        if(*p_tok) {
	  if(m_verbose > 1)
	  LogPrintf("\tMfcDns::MfcDns: enumtrust=%s\n", p_tok);
          m_verifiers[string(p_tok)] = empty_ver;
	}
    } // ENUMs completed 

    // Assign data buffers inside m_value hyper-array
    m_buf    = (uint8_t *)(m_value + VAL_SIZE);
    m_bufend = m_buf + MAX_OUT;
    char *varbufs = m_value + VAL_SIZE + BUF_SIZE + 2;

    m_gw_suffix = m_gw_suf_len?
      strcpy(varbufs, gw_suffix) : NULL;
    
    // Create array of allowed TLD-suffixes
    if(allowed_len) {
      m_allowed_base = strcpy(varbufs + m_gw_suf_len + 1, allowed_suff);
      uint8_t pos = 0, step = 0; // pos, step for double hashing
      for(char *p = m_allowed_base + allowed_len; p > m_allowed_base; ) {
	char c = *--p;
	if(c ==  '|' || c <= 040) {
	  *p = pos = step = 0;
	  continue;
	}
	if(c == '.' || c == '$') {
	  *p = 64;
	  if(p[1] > 040) { // if allowed domain is not empty - save it into ht
	    step |= 1;
	    do 
	      pos += step;
            while(m_ht_offset[pos] != 0);
	    m_ht_offset[pos] = p + 1 - m_allowed_base;
	    const char *dnstype = "DNS";
	    if(c == '$') {
	      m_ht_offset[pos] |= ENUM_FLAG;
	      char *pp = p; // ref to $
	      while(--pp >= m_allowed_base && *pp >= '0' && *pp <= '9');
	      if(++pp < p)
	        *p = atoi(pp);
	      dnstype = "ENUM";
	    }
	    m_allowed_qty++;
	    if(m_verbose > 1)
	      LogPrintf("\tMfcDns::MfcDns: Insert %s TLD=%s:%u\n", dnstype, p + 1, *p);
	  }
	  pos = step = 0;
	  continue;
	}
        pos  = ((pos >> 7) | (pos << 1)) + c;
	step = ((step << 5) - step) ^ c; // (step * 31) ^ c
      } // for
    } // if(allowed_len)

    if(local_len) {
      char *p = m_local_base = (char*)memcpy(varbufs + m_gw_suf_len + 1 + allowed_len + 1, local_tmp, local_len) - 1;
      // and populate hashtable with offsets
      while(++p < m_local_base + local_len) {
	char *p_eq = strchr(p, '=');
	if(p_eq == NULL)
	  break;
        char *p_h = p_eq;
        *p_eq++ = 0; // CLR = and go to data
        uint8_t pos = 0, step = 0; // pos, step for double hashing
	while(--p_h >= p) {
          pos  = ((pos >> 7) | (pos << 1)) + *p_h;
	  step = ((step << 5) - step) ^ *p_h; // (step * 31) ^ c
        } // while
	step |= 1;
	if(m_verbose > 1)
	  LogPrintf("\tMfcDns::MfcDns: Insert Local:[%s]->[%s] pos=%u step=%u\n", p, p_eq, pos, step);
	do 
	  pos += step;
        while(m_ht_offset[pos] != 0);
	m_ht_offset[pos] = m_local_base - p; // negative value - flag LOCAL
	p = strchr(p_eq, 0); // go to the next local record
      } // while
    } //  if(local_len)

    if(m_verbose > 1)
	 LogPrintf("MfcDns::MfcDns: Created/Attached: [%s]:%u; TLD=%u Local=%u\n", 
		 (bind_ip == NULL)? "INADDR_ANY" : bind_ip,
		 port_no, m_allowed_qty, local_qty);

    // Hack - pass TF file list through m_value to HandlePacket()

    if(tollfree && *tollfree) {
      if(m_verbose > 1)
	LogPrintf("\tMfcDns::MfcDns: Setup deferred toll-free=%s\n", tollfree);
      strcpy(m_value, tollfree);
    } else
      m_value[0] = 0;

    m_status = 1; // Active, and maybe download
} // MfcDns::MfcDns
/*---------------------------------------------------*/
void MfcDns::AddTF(char *tf_tok) {
  // Skip comments and empty lines
  if(tf_tok[0] < '0')
    return;

  // Clear TABs and SPs at the end of the line
  char *end = strchr(tf_tok, 0);
  while(*--end <= 040) {
    *end = 0;
    if(end <= tf_tok)
      return;
  }
  
  if(tf_tok[0] == '=') {
    if(tf_tok[1])
      m_tollfree.push_back(TollFree(tf_tok + 1));
  } else 
      if(!m_tollfree.empty())
        m_tollfree.back().e2u.push_back(string(tf_tok));

  if(m_verbose > 1)
    LogPrintf("\tMfcDns::AddTF: Added token [%s] tf/e2u=%u:%u\n", tf_tok, m_tollfree.size(), m_tollfree.back().e2u.size()); 
} // MfcDns::AddTF

/*---------------------------------------------------*/

MfcDns::~MfcDns() {
    // reset current object to initial state
#ifndef WIN32
    shutdown(m_sockfd, SHUT_RDWR);
#endif
    CloseSocket(m_sockfd);
    MilliSleep(100); // Allow 0.1s my thread to exit
    // m_thread.join();
    free(m_value);
    free(m_dap_ht);
    if(m_verbose > 1)
	 LogPrintf("MfcDns::~MfcDns: Destroyed OK\n");
} // MfcDns::~MfcDns


/*---------------------------------------------------*/

void MfcDns::StatRun(void *p) {
  MfcDns *obj = (MfcDns*)p;
  obj->Run();
//mfcoin  ExitThread(0);
} // MfcDns::StatRun

/*---------------------------------------------------*/
void MfcDns::Run() {
  if(m_verbose > 1) LogPrintf("MfcDns::Run: started\n");

  while(m_status < 0) // not initied yet
    MilliSleep(133);

  for( ; ; ) {
    struct sockaddr_in6 sin6;
    socklen_t sin6len = sizeof(struct sockaddr_in6);

    m_rcvlen = recvfrom(m_sockfd, (char *)m_buf, BUF_SIZE, 0,
	            (struct sockaddr *)&sin6, &sin6len);
    if(m_rcvlen <= 0)
	break;

    if(m_dap_ht) {
      uint32_t now = time(NULL);
      if(((now ^ m_daprand) & 0xfffff) == 0) // ~weekly update daprand
        m_daprand = GetRand(0xffffffff) | 1;
      m_timestamp = now >> MFCDNS_DAPSHIFTDECAY; // time in 256s (~4 min)
    }
    if(CheckDAP(&sin6.sin6_addr, sin6len, m_rcvlen)) {
      m_buf[BUF_SIZE] = 0; // Set terminal for infinity QNAME
      if(HandlePacket() == 0) {
        sendto(m_sockfd, (const char *)m_buf, m_snd - m_buf, MSG_NOSIGNAL,
	             (struct sockaddr *)&sin6, sin6len);

        CheckDAP(&sin6.sin6_addr, sin6len, m_snd - m_buf); // update for long answer
      }
    } // dap check
  } // for

  if(m_verbose > 1) LogPrintf("MfcDns::Run: Received Exit packet_len=%d\n", m_rcvlen);

} //  MfcDns::Run

/*---------------------------------------------------*/

int MfcDns::HandlePacket() {
  if(m_verbose > 3) LogPrintf("*\tMfcDns::HandlePacket: Handle packet_len=%d\n", m_rcvlen);

  m_hdr = (DNSHeader *)m_buf;
  // Decode input header from network format
  m_hdr->Transcode();

  m_rcv = m_buf + sizeof(DNSHeader);
  m_rcvend = m_snd = m_buf + m_rcvlen;

  if(m_verbose > 4) {
    LogPrintf("\tMfcDns::HandlePacket: msgID  : %d\n", m_hdr->msgID);
    LogPrintf("\tMfcDns::HandlePacket: Bits   : %04x\n", m_hdr->Bits);
    LogPrintf("\tMfcDns::HandlePacket: QDCount: %d\n", m_hdr->QDCount);
    LogPrintf("\tMfcDns::HandlePacket: ANCount: %d\n", m_hdr->ANCount);
    LogPrintf("\tMfcDns::HandlePacket: NSCount: %d\n", m_hdr->NSCount);
    LogPrintf("\tMfcDns::HandlePacket: ARCount: %d\n", m_hdr->ARCount);
  }
  // Assert following 3 counters and bits are zero
//*  uint16_t zCount = m_hdr->ANCount | m_hdr->NSCount | m_hdr->ARCount | (m_hdr->Bits & (m_hdr->QR_MASK | m_hdr->TC_MASK));
  uint16_t zCount = m_hdr->ANCount | m_hdr->NSCount | (m_hdr->Bits & (m_hdr->QR_MASK | m_hdr->TC_MASK));

  // Clear answer counters - maybe contains junk from client
  m_hdr->ANCount = m_hdr->NSCount = m_hdr->ARCount = 0;
  m_hdr->Bits &= m_hdr->RD_MASK;
  m_hdr->Bits |= m_hdr->QR_MASK | m_hdr->AA_MASK;

  do {
    // check flags QR=0 and TC=0
    if(m_hdr->QDCount == 0 || zCount != 0) {
      m_hdr->Bits |= 1; // Format error, expected request
      break;
    }

    uint16_t opcode = m_hdr->Bits & m_hdr->OPCODE_MASK;

    if(opcode != 0) {
      m_hdr->Bits |= 4; // Not implemented; handle standard query only
      break;
    }

    if(m_status) {
      if((m_status = IsInitialBlockDownload()) != 0) {
        m_hdr->Bits |= 2; // Server failure - not available valid nameindex DB yet
        break;
      } else {
	// Fill deferred toll-free default entries
        char *tf_str = m_value;
        // Iterate the list of Toll-Free fnames; can be fnames and NVS records
        while(char *tf_fname = strsep(&tf_str, "|")) {
          if(m_verbose > 1)
	    LogPrintf("\tMfcDns::HandlePacket: handle deferred toll-free=%s\n", tf_fname);
          if(tf_fname[0] == '@') { // this is NVS record
            string value;
            if(hooks->getNameValue(string(tf_fname + 1), value)) {
              char *tf_val = strcpy(m_value, value.c_str());
              while(char *tf_tok = strsep(&tf_val, "\r\n"))
	        AddTF(tf_tok);
	    }
          } else { // This is file
	    FILE *tf = fopen(tf_fname, "r");
 	    if(tf != NULL) {
	      while(fgets(m_value, VAL_SIZE, tf))
	        AddTF(m_value);
	      fclose(tf);
	    }
          } // if @
        } // while tf_name
      } // m_status #2
    } // m_status #1

    // Handle questions here
    for(uint16_t qno = 0; qno < m_hdr->QDCount && m_snd < m_bufend; qno++) {
      if(m_verbose > 5) 
        LogPrintf("\tMfcDns::HandlePacket: qno=%u m_hdr->QDCount=%u\n", qno, m_hdr->QDCount);
      uint16_t rc = HandleQuery();
      if(rc) {
	if(rc == 0xDead)
	  return rc; // DAP or another error - lookup interrupted, don't answer
	m_hdr->Bits |= rc;
	break;
      }
    }
  } while(false);

  // Remove AR-section from request, if exist
  int ar_len = m_rcvend - m_rcv;

  if(ar_len < 0) {
      m_hdr->Bits |= 1; // Format error, RCV pointer is over
  }

  if(ar_len > 0) {
    memmove(m_rcv, m_rcvend, m_snd - m_rcvend);
    m_snd -= ar_len;
  }

  // Add an empty EDNS RR record
  Answer_OPT();

  // Truncate answer, if needed
  if(m_snd >= m_bufend) {
    m_hdr->Bits |= m_hdr->TC_MASK;
    m_snd = m_buf + MAX_OUT;
  }
  // Encode output header into network format
  m_hdr->Transcode();
  return 0; // answer ready
} // MfcDns::HandlePacket

/*---------------------------------------------------*/
uint16_t MfcDns::HandleQuery() {
  // Decode qname
  uint8_t key[BUF_SIZE];				// Key, transformed to dot-separated LC
  uint8_t *key_end = key;
  uint8_t *domain_ndx[MAX_DOM];				// indexes to domains
  uint8_t **domain_ndx_p = domain_ndx;			// Ptr to the end

  // m_rcv is pointer to QNAME
  // Set reference to domain label
  m_label_ref = (m_rcv - m_buf) | 0xc000;

  // Convert DNS request (QNAME) to dot-separated printed domaon name in LC
  // Fill domain_ndx - indexes for domain entries
  uint8_t dom_len;
  while((dom_len = *m_rcv++) != 0) {
    // wrong domain length | key too long, over BUF_SIZE | too mant domains, max is MAX_DOM
    if((dom_len & 0xc0) || key_end >= key + BUF_SIZE || domain_ndx_p >= domain_ndx + MAX_DOM)
      return 1; // Invalid request
    *domain_ndx_p++ = key_end;
    do {
      char c = 040 | *m_rcv++; // tolower char
      *key_end++ = c;
    } while(--dom_len);
    *key_end++ = '.'; // Set DOT at domain end
  } //  while(dom_len)

  *--key_end = 0; // Remove last dot, set EOLN

  if(!CheckDAP(key, key - key_end, 0)) {
    if(m_verbose > 0)
      LogPrintf("\tMfcDns::HandleQuery: Aborted domain %s by DAP\n", key);
    return 0xDead; // Botnet detected, abort query processing
  }

  if(m_verbose > 4) 
    LogPrintf("MfcDns::HandleQuery: Translated domain name: [%s]; DomainsQty=%d\n", key, (int)(domain_ndx_p - domain_ndx));

  uint16_t qtype  = *m_rcv++; qtype  = (qtype  << 8) + *m_rcv++; 
  uint16_t qclass = *m_rcv++; qclass = (qclass << 8) + *m_rcv++;

  if(m_verbose > 2) 
    LogPrintf("MfcDns::HandleQuery: Key=%s QType=0x%x QClass=0x%x\n", key, qtype, qclass);

  if(qclass != 1)
    return 4; // Not implemented - support INET only

  // If thid is public gateway, gw-suffix can be specified, like 
  // mfcdnssuffix=.xyz.com
  // Followind block cuts this suffix, if exists.
  // If received domain name "xyz.com" only, keyp is empty string

  if(m_gw_suf_len) { // suffix defined [public DNS], need to cut
    uint8_t *p_suffix = key_end - m_gw_suf_len;
    if(p_suffix >= key && strcmp((const char *)p_suffix, m_gw_suffix) == 0) {
      *p_suffix = 0; // Cut suffix m_gw_sufix
      key_end = p_suffix;
      domain_ndx_p -= m_gw_suf_dots; 
    } else 
    // check special - if suffix == GW-site, e.g., request: emergate.net
    if(p_suffix == key - 1 && strcmp((const char *)p_suffix + 1, m_gw_suffix + 1) == 0) {
      *++p_suffix = 0; // Set empty search key
      key_end = p_suffix;
      domain_ndx_p = domain_ndx;
    } 
  } // if(m_gw_suf_len)

  // Search for TLD-suffix, like ".coin"
  // If name without dot, like "www", this is candidate for local search
  // Compute 2-hash params for TLD-suffix or local name

  uint8_t pos = 0, step = 0; // pos, step for double hashing

  uint8_t *p = key_end;

  if(m_verbose > 4) 
    LogPrintf("MfcDns::HandleQuery: After TLD-suffix cut: [%s]\n", key);

  while(p > key) {
    uint8_t c = *--p;
    if(c == '.')
      break; // this is TLD-suffix
    pos  = ((pos >> 7) | (pos << 1)) + *p;
    step = ((step << 5) - step) ^ *p; // (step * 31) ^ c
  }

  step |= 1; // Set even step for 2-hashing

  if(p == key && m_local_base != NULL) {
    // no TLD suffix, try to search local 1st
    if(LocalSearch(p, pos, step) > 0)
      p = NULL; // local search is OK, do not perform nameindex search
  }

  // If local search is unsuccessful, try to search in the nameindex DB.
  if(p) {
    // Check domain by tld filters, if activated. Otherwise, pass to nameindex as is.
    if(m_allowed_qty) { // Activated TLD-filter
      if(*p != '.') {
        if(m_verbose > 0) 
          LogPrintf("MfcDns::HandleQuery: TLD-suffix=[.%s] is not specified in given key=%s; return NXDOMAIN\n", p, key);
	return 3; // TLD-suffix is not specified, so NXDOMAIN
      } 
      p++; // Set PTR after dot, to the suffix
      do {
        pos += step;
        if(m_ht_offset[pos] == 0) {
          if(m_verbose > 0) 
  	    LogPrintf("MfcDns::HandleQuery: TLD-suffix=[.%s] in given key=%s is not allowed; return NXDOMAIN\n", p, key);
	  return 3; // Reached EndOfList, so NXDOMAIN
        } 
      } while(m_ht_offset[pos] < 0 || strcmp((const char *)p, m_allowed_base + (m_ht_offset[pos] & ~ENUM_FLAG)) != 0);

      // ENUM SPFUN works only if TLD-filter is active
      if(m_ht_offset[pos] & ENUM_FLAG)
        return SpfunENUM(m_allowed_base[(m_ht_offset[pos] & ~ENUM_FLAG) - 1], domain_ndx, domain_ndx_p);
      

    } // if(m_allowed_qty)

    uint8_t **cur_ndx_p, **prev_ndx_p = domain_ndx_p - 2;
    if(prev_ndx_p < domain_ndx) 
      prev_ndx_p = domain_ndx;
#if 0
    else {
      // 2+ domain level. 
      // Try to adjust TLD suffix for peering GW-site, like opennic.coin
      if(strncmp((const char *)(*prev_ndx_p), "opennic.", 8) == 0)
        strcpy((char*)domain_ndx_p[-1], "*"); // substitute TLD to '*'; don't modify domain_ndx_p[0], for keep TLD size for REF
    }
#endif
 
    // Search in the nameindex db. Possible to search filtered indexes, or even pure names, like "dns:www"

    bool step_next;
    do {
      cur_ndx_p = prev_ndx_p;
      if(Search(*cur_ndx_p) <= 0) // Result saved into m_value
	return 3; // empty answer, not found, return NXDOMAIN
      if(cur_ndx_p == domain_ndx)
	break; // This is 1st domain (last in the chain), go to answer
      // Try to search allowance in SD=list for step down
      prev_ndx_p = cur_ndx_p - 1;
      int domain_len = *cur_ndx_p - *prev_ndx_p - 1;
      char val2[VAL_SIZE];
      char *tokens[MAX_TOK];
      step_next = false;
      int sdqty = Tokenize("SD", ",", tokens, strcpy(val2, m_value));
      while(--sdqty >= 0 && !step_next)
        step_next = strncmp((const char *)*prev_ndx_p, tokens[sdqty], domain_len) == 0;

      // if no way down - maybe, we can create REF-answer from NS-records
      if(step_next == false && TryMakeref(m_label_ref + (*cur_ndx_p - key)))
	return 0;
      // if cannot create REF - just ANSWER for parent domain (ignore prefix)
    } while(step_next);
    
  } // if(p) - ends of DB search 

  char val2[VAL_SIZE];
  // There is generate ANSWER section
  { // Extract TTL
    char *tokens[MAX_TOK];
    int ttlqty = Tokenize("TTL", NULL, tokens, strcpy(val2, m_value));
    m_ttl = ttlqty? atoi(tokens[0]) : 3600; // 1 hour default TTL
  }
  
  // List values for ANY:    A NS CNA PTR MX AAAA
  const uint16_t q_all[] = { 1, 2, 5, 12, 15, 28, 0 };

  switch(qtype) {
    case 0xff:	// ALL
      for(const uint16_t *q = q_all; *q; q++)
        Answer_ALL(*q, strcpy(val2, m_value));
      break;
    case 1:	// A
    case 28:	// AAAA
      Answer_ALL(qtype, strcpy(val2, m_value));
      // Not found A/AAAA - try lookup for CNAME in the default section
      // Quoth RFC 1034, Section 3.6.2:
      // If a CNAME RR is present at a node, no other data should be present;
      // Not found A/AAAA/CNAME - try lookup for CNAME in the default section
      if(m_hdr->ANCount == 0)
        Answer_ALL(5, strcpy(val2, m_value));
      if(m_hdr->ANCount != 0)
	break;
      // Add Authority/Additional section here, if still no answer
      // Fill AUTH+ADDL section according https://www.ietf.org/rfc/rfc1034.txt, sec 6.2.6
//      m_hdr->Bits &= ~m_hdr->AA_MASK;
      qtype = 2 | 0x80;
      // go to default below
    default:
      Answer_ALL(qtype, m_value);
      break;
  } // switch
  return 0;
} // MfcDns::HandleQuery

/*---------------------------------------------------*/
int MfcDns::TryMakeref(uint16_t label_ref) {
  char val2[VAL_SIZE];
  char *tokens[MAX_TOK];
  int ttlqty = Tokenize("TTL", NULL, tokens, strcpy(val2, m_value));
  m_ttl = ttlqty? atoi(tokens[0]) : 24 * 3600;
  uint16_t orig_label_ref = m_label_ref;
  m_label_ref = label_ref;
  Answer_ALL(2, strcpy(val2, m_value));
  m_label_ref = orig_label_ref;
  m_hdr->NSCount = m_hdr->ANCount;
  m_hdr->ANCount = 0;
  LogPrintf("MfcDns::TryMakeref: Generated REF NS=%u\n", m_hdr->NSCount);
  return m_hdr->NSCount;
} //  MfcDns::TryMakeref
/*---------------------------------------------------*/

int MfcDns::Tokenize(const char *key, const char *sep2, char **tokens, char *buf) {
  int tokensN = 0;

  // Figure out main separator. If not defined, use |
  char mainsep[2];
  if(*buf == '~') {
    buf++;
    mainsep[0] = *buf++;
  } else
     mainsep[0] = '|';
  mainsep[1] = 0;

  for(char *token = strtok(buf, mainsep);
    token != NULL; 
      token = strtok(NULL, mainsep)) {
      // LogPrintf("Token:%s\n", token);
      char *val = strchr(token, '=');
      if(val == NULL)
	  continue;
      *val = 0;
      if(strcmp(key, token)) {
	  *val = '=';
	  continue;
      }
      val++;
      // Uplevel token found, tokenize value if needed
      // LogPrintf("Found: key=%s; val=%s\n", key, val);
      if(sep2 == NULL || *sep2 == 0) {
	tokens[tokensN++] = val;
	break;
      }
     
      // if needed. redefine sep2
      char sepulka[2];
      if(*val == '~') {
	  val++;
	  sepulka[0] = *val++;
	  sepulka[1] = 0;
	  sep2 = sepulka;
      }
      // Tokenize value
      for(token = strtok(val, sep2); 
	 token != NULL && tokensN < MAX_TOK; 
	   token = strtok(NULL, sep2)) {
	  // LogPrintf("Subtoken=%s\n", token);
	  tokens[tokensN++] = token;
      }
      break;
  } // for - big tokens (MX, A, AAAA, etc)
  return tokensN;
} // MfcDns::Tokenize

/*---------------------------------------------------*/

void MfcDns::Answer_ALL(uint16_t qtype, char *buf) {
  const char *key;
  uint16_t needed_addl = qtype & 0x80;
  qtype ^= needed_addl;

  switch(qtype) {
      case  1 : key = "A";      break;
      case  2 : key = "NS";     break;
      case  5 : key = "CNAME";  break;
      case 12 : key = "PTR";    break;
      case 15 : key = "MX";     break;
      case 16 : key = "TXT";    break;
      case 28 : key = "AAAA";   break;
      default: return;
  } // switch

  //uint16_t addl_refs[MAX_TOK];
  char *tokens[MAX_TOK];
  int tokQty = Tokenize(key, ",", tokens, buf);

  if(m_verbose > 4) LogPrintf("MfcDns::Answer_ALL(QT=%d, key=%s); TokenQty=%d\n", qtype, key, tokQty);

  // Shuffle tokens for randomization output order
  for(int i = tokQty; i > 1; ) {
    int randndx = GetRand(i);
    char *tmp = tokens[randndx];
    --i;
    tokens[randndx] = tokens[i];
    tokens[i] = tmp;
  }

  for(int tok_no = 0; tok_no < tokQty; tok_no++) {
      if(m_verbose > 4) 
	LogPrintf("\tMfcDns::Answer_ALL: Token:%u=[%s]\n", tok_no, tokens[tok_no]);
      Out2(m_label_ref);
      Out2(qtype); // A record, or maybe something else
      Out2(1); //  INET
      Out4(m_ttl);
      switch(qtype) {
	case 1 : Fill_RD_IP(tokens[tok_no], AF_INET);  break;
	case 28: Fill_RD_IP(tokens[tok_no], AF_INET6); break;
	case 2 :
	case 5 :
    //case 12: addl_refs[tok_no] = Fill_RD_DName(tokens[tok_no], 0, 0); break; // NS,CNAME,PTR
    case 12: Fill_RD_DName(tokens[tok_no], 0, 0); break; // NS,CNAME,PTR
	case 15: Fill_RD_DName(tokens[tok_no], 2, 0); break; // MX
	case 16: Fill_RD_DName(tokens[tok_no], 0, 1); break; // TXT
	default: break;
      } // swithc
  } // for

  if(needed_addl) { // Foll ADDL section (NS in NSCount)
    m_hdr->NSCount += tokQty;
#if 0
    for(int tok_no = 0; tok_no < tokQty; tok_no++) {
      Out2(addl_refs[tok_no]);
      Out2(1); // PTR record, or maybe something else
      Out2(1); //  INET
      Out4(m_ttl);
      //Out2(2); // out.sz
      //Out2(addl_refs[tok_no]);
      Out2(4); // out.sz
      inet_pton(AF_INET, "91.217.137.1", m_snd);
      m_snd += 4;
      m_hdr->ARCount++;
    } // for
#endif
  } else
    m_hdr->ANCount += tokQty;
} // MfcDns::Answer_ALL 

/*---------------------------------------------------*/
/*
NAME 	domain name 	MUST be 0 (root domain)
TYPE 	u_int16_t 	OPT (41)
CLASS 	u_int16_t 	requestor's UDP payload size
TTL 	u_int32_t 	extended RCODE and flags
RDLEN 	u_int16_t 	length of all RDATA
RDATA 	octet stream 	{attribute,value} pairs
*/
void MfcDns::Answer_OPT() {
  *m_snd++ = 0; // Name: =0
  Out2(41);     // Type: OPT record 0x29
  Out2(MAX_OUT);// Class: Out size
  Out4(0);      // TTL - all zeroes
  Out2(0);      // RDLEN
  m_hdr->ARCount++;
} // MfcDns::Answer_OPT
/*---------------------------------------------------*/

void MfcDns::Fill_RD_IP(char *ipddrtxt, int af) {
  uint16_t out_sz;
  switch(af) {
      case AF_INET : out_sz = 4;  break;
      case AF_INET6: out_sz = 16; break;
      default: return;
  }
  Out2(out_sz);
  if(inet_pton(af, ipddrtxt, m_snd)) 
    m_snd += out_sz;
  else
    m_snd -= 12, m_hdr->ANCount--; // 12 = clear this 2 and 10 bytes at caller
} // MfcDns::Fill_RD_IP

/*---------------------------------------------------*/

int MfcDns::Fill_RD_DName(char *txt, uint8_t mxsz, int8_t txtcor) {
  uint8_t *snd0 = m_snd;
  m_snd += 3 + mxsz; // skip SZ and sz0
  uint8_t *tok_sz = m_snd - 1;
  uint16_t mx_pri = 1; // Default MX priority
  char c;

  uint8_t *bufend = m_snd + 255;

  if(m_bufend < bufend)
    bufend = m_bufend;

  int label_ref = (tok_sz - m_buf - (m_rcvend - m_rcv)) | 0xc000;

  do {
    c = *m_snd++ = *txt++;
    if(c == '.') {
      *tok_sz = m_snd - tok_sz - 2;
      tok_sz  = m_snd - 1;
    }
    if(c == ':' && mxsz) { // split for MX only
      c = m_snd[-1] = 0;
      mx_pri = atoi(txt);
    }
  } while(c && m_snd < bufend);

  *tok_sz = m_snd - tok_sz - 2;

  // Remove trailing \0 at end of text
  m_snd -= txtcor;

  uint16_t len = m_snd - snd0 - 2;
  *snd0++ = len >> 8;
  *snd0++ = len;
  if(mxsz) {
    *snd0++ = mx_pri >> 8;
    *snd0++ = mx_pri;
  }
  return label_ref;
} // MfcDns::Fill_RD_DName

/*---------------------------------------------------*/
/*---------------------------------------------------*/

int MfcDns::Search(uint8_t *key) {
  if(m_verbose > 4) 
    LogPrintf("MfcDns::Search(%s)\n", key);

  string value;
  if (!hooks->getNameValue(string("dns:") + (const char *)key, value))
    return 0;

  strcpy(m_value, value.c_str());
  return 1;
} //  MfcDns::Search

/*---------------------------------------------------*/

int MfcDns::LocalSearch(const uint8_t *key, uint8_t pos, uint8_t step) {
  if(m_verbose > 4) 
    LogPrintf("MfcDns::LocalSearch(%s, %u, %u) called\n", key, pos, step);
    do {
      pos += step;
      if(m_ht_offset[pos] == 0) {
        if(m_verbose > 4) 
  	  LogPrintf("MfcDns::LocalSearch: Local key=[%s] not found; go to nameindex search\n", key);
         return 0; // Reached EndOfList 
      } 
    } while(m_ht_offset[pos] > 0 || strcmp((const char *)key, m_local_base - m_ht_offset[pos]) != 0);

  strcpy(m_value, strchr(m_local_base - m_ht_offset[pos], 0) + 1);

  return 1;
} // MfcDns::LocalSearch


/*---------------------------------------------------*/
#define ROLADD(h,s,x)   h = ((h << s) | (h >> (32 - s))) + (x)
// Returns true - can handle packet; false = ignore
bool MfcDns::CheckDAP(void *key, int len, uint32_t packet_size) { 
  if(m_dap_ht == NULL)
    return true; // Filter is inactive

  uint32_t ip_addr = 0;
  if(len < 0) { // domain string
    for(int i = 0; i < -len; i++)
      ROLADD(ip_addr, 6, ((const char *)key)[i]);
  } else { // IP addr
    for(int i = 0; i < (len / 4); i++)
      ROLADD(ip_addr, 1, ((const uint32_t *)key)[i]);
  }
  
  uint16_t inctemp = (packet_size >> 5) + 1; // 1 degr = 32 bytes unit
  uint32_t hash = m_daprand, mintemp = ~0;

  int used_ndx[MFCDNS_DAPBLOOMSTEP];
  for(int bloomstep = 0; bloomstep < MFCDNS_DAPBLOOMSTEP; bloomstep++) {
    int ndx, att = 0;
    do {
      ++att;
      hash *= ip_addr;
      hash ^= hash >> 16;
      hash += hash >> 7;
      ndx = (hash ^ att) & m_dapmask; // always positive
      for(int i = 0; i < bloomstep; i++)
	if(ndx == used_ndx[i])
	  ndx = -1;
    } while(ndx < 0);

    DNSAP *dap = &m_dap_ht[used_ndx[bloomstep] = ndx];
    uint16_t dt = m_timestamp - dap->timestamp;
    uint32_t new_temp = (dt > 15? 0 : dap->temp >> dt) + inctemp;
    dap->temp = (new_temp > 0xffff)? 0xffff : new_temp;
    dap->timestamp = m_timestamp;
    if(new_temp < mintemp) 
      mintemp = new_temp;
  } // for

  bool rc = mintemp < m_dap_treshold;
  if(m_verbose > 5 || (!rc && m_verbose > 0)) {
    char buf[80];
    LogPrintf("MfcDns::CheckDAP: IP=[%s] packet_size=%u, mintemp=%u dap_treshold=%u rc=%d\n", 
		    len < 0? (const char *)key : inet_ntop(len == 4? AF_INET : AF_INET6, key, buf, len),
                    packet_size, mintemp, m_dap_treshold, rc);
  }
  return rc;
} // MfcDns::CheckDAP 

/*---------------------------------------------------*/
// Handle Special function - phone number in the E.164 format
// to support ENUM service
int MfcDns::SpfunENUM(uint8_t len, uint8_t **domain_start, uint8_t **domain_end) {
  int dom_length = domain_end - domain_start;
  const char *tld = (const char*)domain_end[-1];

  if(m_verbose > 3)
    LogPrintf("\tMfcDns::SpfunENUM: Domain=[%s] N=%u TLD=[%s] Len=%u\n", 
	    (const char*)*domain_start, dom_length, tld, len);

  do {
    if(dom_length < 2)
      break; // no domains for phone number - NXDOMAIN

    if(m_verifiers.empty() && m_tollfree.empty())	
      break; // no verifier - all ENUMs untrusted

    // convert reversed domain record to ITU-T number
    char itut_num[68], *pitut = itut_num, *pitutend = itut_num + len;
    for(const uint8_t *p = domain_end[-1]; --p >= *domain_start; )
      if(*p >= '0' && *p <= '9') {
	*pitut++ = *p;
        if(pitut >= pitutend)
	  break;
      }
    *pitut = 0; // EOLN at phone number end

    if(pitut == itut_num)
      break; // Empty phone number - NXDOMAIN

    if(m_verbose > 4)
      LogPrintf("\tMfcDns::SpfunENUM: ITU-T num=[%s]\n", itut_num);

    // Itrrate all available ENUM-records, and build joined answer from them
    if(!m_verifiers.empty()) {
      for(int16_t qno = 0; qno >= 0; qno++) {
        char q_str[100];
        sprintf(q_str, "%s:%s:%u", tld, itut_num, qno); 
        if(m_verbose > 4) 
          LogPrintf("\tMfcDns::SpfunENUM Search(%s)\n", q_str);

        string value;
        if(!hooks->getNameValue(string(q_str), value))
          break;

        strcpy(m_value, value.c_str());
        Answer_ENUM(q_str);
      } // for 
    } // if

    // If notheing found in the ENUM - try to search in the Toll-Free
    m_ttl = 24 * 3600; // 24h by default
    boost::xpressive::smatch nameparts;
    for(vector<TollFree>::const_iterator tf = m_tollfree.begin(); 
	      m_hdr->ANCount == 0 && tf != m_tollfree.end(); 
	      tf++) {
      bool matched = regex_match(string(itut_num), nameparts, tf->regex);
      // bool matched = regex_search(string(itut_num), nameparts, tf->regex);
      if(m_verbose > 4) 
          LogPrintf("\tMfcDns::SpfunENUM TF-match N=[%s] RE=[%s] -> %u\n", itut_num, tf->regex_str.c_str(), matched);
      if(matched)
        for(vector<string>::const_iterator e2u = tf->e2u.begin(); e2u != tf->e2u.end(); e2u++)
          HandleE2U(strcpy(m_value, e2u->c_str()));
    } // tf processing

    if(m_hdr->ANCount) {
      return 0; // if collected some answers - OK
    }

  } while(false);

  return 3; // NXDOMAIN
} // MfcDns::SpfunENUM

/*---------------------------------------------------*/

#define ENC3(a, b, c) (a | (b << 8) | (c << 16))

/*---------------------------------------------------*/
// Generate answewr for found EMUM NVS record
void MfcDns::Answer_ENUM(const char *q_str) {
  char *str_val = m_value;
  const char *pttl;
  char *e2u[VAL_SIZE / 4]; // 20kb max input, and min 4 bytes per token
  uint16_t e2uN = 0;
  bool sigOK = false;

  m_ttl = 24 * 3600; // 24h by default

  // Tokenize lines in the NVS-value.
  // There can be prefixes SIG=, TTL=, E2U
  while(char *tok = strsep(&str_val, "\n\r"))
    switch((*(uint32_t*)tok & 0xffffff) | 0x202020) {
	case ENC3('e', '2', 'u'):
	  e2u[e2uN++] = tok;
	  continue;

	case ENC3('t', 't', 'l'):
	  pttl = strchr(tok + 3, '=');
	  if(pttl) 
	    m_ttl = atoi(pttl + 1);
	  continue;

	case ENC3('s', 'i', 'g'):
	  if(!sigOK)
	    sigOK = CheckEnumSig(q_str, strchr(tok + 3, '='));
	  continue;

	default:
	  continue;
    } // while + switch

  if(!sigOK)
    return; // This ENUM-record does not contain a valid signature

  // Generate ENUM-answers here
  for(uint16_t e2undx = 0; e2undx < e2uN; e2undx++)
    if(m_snd < m_bufend - 24)
      HandleE2U(e2u[e2undx]);

 } // MfcDns::Answer_ENUM

/*---------------------------------------------------*/
void MfcDns::OutS(const char *p) {
  int len = strlen(strcpy((char *)m_snd + 1, p));
  *m_snd = len;
  m_snd += len + 1; 
} // MfcDns::OutS

/*---------------------------------------------------*/
 // Generate ENUM-answers for a single E2U entry
 // E2U+sip=100|10|!^(.*)$!sip:17771234567@in.callcentric.com!
void MfcDns::HandleE2U(char *e2u) {
  char *data = strchr(e2u, '=');
  if(data == NULL) 
    return;

  // Cleanum sufix for service; Service started from E2U
  for(char *p = data; *--p <= 040; *p = 0) {}

  unsigned int ord, pref;
  char re[VAL_SIZE];

  *data++ = 0; // Remove '='

  if(sscanf(data, "%u | %u | %s", &ord, &pref, re) != 3)
    return;

  if(m_verbose > 5)
    LogPrintf("\tMfcDns::HandleE2U: Parsed: %u %u %s %s\n", ord, pref, e2u, re);

  if(m_snd + strlen(re) + strlen(e2u) + 24 >= m_bufend)
    return;

  Out2(m_label_ref);
  Out2(0x23); // NAPTR record
  Out2(1); //  INET
  Out4(m_ttl);
  uint8_t *snd0 = m_snd; m_snd += 2;
  Out2(ord);
  Out2(pref);
  OutS("u");
  OutS(e2u);
  OutS(re);
  *m_snd++ = 0;

  uint16_t len = m_snd - snd0 - 2;
  *snd0++ = len >> 8;
  *snd0++ = len;

  m_hdr->ANCount++;
} //  MfcDns::HandleE2U

/*---------------------------------------------------*/
bool MfcDns::CheckEnumSig(const char *q_str, char *sig_str) {
    if(sig_str == NULL)
      return false;

    // skip SP/TABs in signature
    while(*++sig_str <= ' ');

    char *signature = strchr(sig_str, '|');
    if(signature == NULL)
      return false;
    
    for(char *p = signature; *--p <= 040; *p = 0) {}
    *signature++ = 0;

    map<string, Verifier>::iterator it = m_verifiers.find(sig_str);
    if(it == m_verifiers.end())
      return false; // Unknown verifier - do not trust it

    Verifier &ver = it->second;

    if(ver.mask < 0) {
      if(ver.mask == VERMASK_BLOCKED)
	return false; // Already unable to fetch

      do {
        NameTxInfo nti;
        CNameRecord nameRec;
        CTransactionRef tx;
        LOCK(cs_main);
        CNameDB dbName("r");
        if(!dbName.ReadName(CNameVal(it->first.c_str(), it->first.c_str() + it->first.size()), nameRec))
	  break; // failed to read from name DB
        if(nameRec.vtxPos.size() < 1)
	  break; // no result returned
        if(!GetTransaction(nameRec.vtxPos.back().txPos, tx))
          break; // failed to read from from disk
        if(!DecodeNameTx(tx, nti, true))
          break; // failed to decode name
	CBitcoinAddress addr(nti.strAddress);
        if(!addr.IsValid())
          break; // Invalid address
        if(!addr.GetKeyID(ver.keyID))
          break; // Address does not refer to key

	// Verifier has been read successfully, configure SRL if exist
	char valbuf[VAL_SIZE], *str_val = valbuf;
        memcpy(valbuf, &nti.value[0], nti.value.size());
        valbuf[nti.value.size()] = 0;

	// Proces SRL-line like
	// SRL=5|srl:hello-%02x
	ver.mask = VERMASK_NOSRL;
        while(char *tok = strsep(&str_val, "\n\r"))
	  if(((*(uint32_t*)tok & 0xffffff) | 0x202020) == ENC3('s', 'r', 'l') && (tok = strchr(tok + 3, '='))) {
	    unsigned nbits = atoi(++tok);
            if(nbits > 30) nbits = 30;
	    tok = strchr(tok, '|');
	    if(tok != NULL) {
		do {
		  if(*++tok == 0)
		    break; // empty SRL, thus keep VERMASK_NOSRL
		  char *pp = strchr(tok, '%');
		  if(pp != NULL) {
		    if(*++pp == '0')
		      do ++pp; while(*pp >= '0' && *pp <= '9');
                    if(strchr("diouXx", *pp) == NULL)
			break; // Invalid char in the template
		    if(strchr(pp, '%'))
			break; // Not allowed 2nd % symbol
		  } else
		    nbits = 0; // Don't needed nbits/mask for no-bucket srl_tpl

                  ver.srl_tpl.assign(tok);
		  ver.mask = (1 << nbits) - 1;
		} while(false);
	    } // if(tok != NULL)
	    if(ver.mask != VERMASK_NOSRL)
	      break; // Mask found
	  } // while + if
      } while(false);

      if(ver.mask < 0) {
	ver.mask = VERMASK_BLOCKED; // Unable to read - block next read
	return false;
      } // if(ver.mask < 0) - after try-fill verifiyer

    } // if(ver.mask < 0) - main
 
    while(*signature <= 040 && *signature) 
      signature++;

    bool fInvalid = false;
    vector<unsigned char> vchSig(DecodeBase64(signature, &fInvalid));

    if(fInvalid)
      return false;

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << string(q_str);

    CPubKey pubkey;
    if(!pubkey.RecoverCompact(ss.GetHash(), vchSig))
      return false;

    if(pubkey.GetID() != ver.keyID)
	return false; // Signature check did not passed

    if(ver.mask == VERMASK_NOSRL)
	return true; // This verifiyer does not have active SRL

    char valbuf[VAL_SIZE];

    // Compute a simple hash from q_str like enum:17771234567:0
    // This hasu must be used by verifiyers for build buckets
    unsigned h = 0x5555;
    for(const char *p = q_str; *p; p++)
	h += (h << 5) + *p;
    sprintf(valbuf, ver.srl_tpl.c_str(), h & ver.mask);

    string value;
    if(!hooks->getNameValue(string(valbuf), value))
      return true; // Unable fetch SRL - as same as SRL does not exist

    // Is q_str missing in the SRL
    return value.find(q_str) == string::npos;

#if 0
    char *valstr = strcpy(valbuf, value.c_str());
    while(char *tok = strsep(&valstr, "|, \r\n\t"))
      if(strcmp(tok, q_str) == 0)
	reurn false;

    return true;
#endif
} // MfcDns::CheckEnumSig

