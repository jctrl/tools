
/***************************************************************************
 * TargetGroup.cc -- The "TargetGroup" class holds a group of IP           *
 * addresses, such as those from a '/16' or '10.*.*.*' specification.  It  *
 * also has a trivial HostGroupState class which handles a bunch of        *
 * expressions that go into TargetGroup classes.                           *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *                                                                         *
 * The Nmap Security Scanner is (C) 1996-2012 Insecure.Com LLC. Nmap is    *
 * also a registered trademark of Insecure.Com LLC.  This program is free  *
 * software; you may redistribute and/or modify it under the terms of the  *
 * GNU General Public License as published by the Free Software            *
 * Foundation; Version 2 with the clarifications and exceptions described  *
 * below.  This guarantees your right to use, modify, and redistribute     *
 * this software under certain conditions.  If you wish to embed Nmap      *
 * technology into proprietary software, we sell alternative licenses      *
 * (contact sales@insecure.com).  Dozens of software vendors already       *
 * license Nmap technology such as host discovery, port scanning, OS       *
 * detection, version detection, and the Nmap Scripting Engine.            *
 *                                                                         *
 * Note that the GPL places important restrictions on "derived works", yet *
 * it does not provide a detailed definition of that term.  To avoid       *
 * misunderstandings, we interpret that term as broadly as copyright law   *
 * allows.  For example, we consider an application to constitute a        *
 * "derivative work" for the purpose of this license if it does any of the *
 * following:                                                              *
 * o Integrates source code from Nmap                                      *
 * o Reads or includes Nmap copyrighted data files, such as                *
 *   nmap-os-db or nmap-service-probes.                                    *
 * o Executes Nmap and parses the results (as opposed to typical shell or  *
 *   execution-menu apps, which simply display raw Nmap output and so are  *
 *   not derivative works.)                                                *
 * o Integrates/includes/aggregates Nmap into a proprietary executable     *
 *   installer, such as those produced by InstallShield.                   *
 * o Links to a library or executes a program that does any of the above   *
 *                                                                         *
 * The term "Nmap" should be taken to also include any portions or derived *
 * works of Nmap, as well as other software we distribute under this       *
 * license such as Zenmap, Ncat, and Nping.  This list is not exclusive,   *
 * but is meant to clarify our interpretation of derived works with some   *
 * common examples.  Our interpretation applies only to Nmap--we don't     *
 * speak for other people's GPL works.                                     *
 *                                                                         *
 * If you have any questions about the GPL licensing restrictions on using *
 * Nmap in non-GPL works, we would be happy to help.  As mentioned above,  *
 * we also offer alternative license to integrate Nmap into proprietary    *
 * applications and appliances.  These contracts have been sold to dozens  *
 * of software vendors, and generally include a perpetual license as well  *
 * as providing for priority support and updates.  They also fund the      *
 * continued development of Nmap.  Please email sales@insecure.com for     *
 * further information.                                                    *
 *                                                                         *
 * As a special exception to the GPL terms, Insecure.Com LLC grants        *
 * permission to link the code of this program with any version of the     *
 * OpenSSL library which is distributed under a license identical to that  *
 * listed in the included docs/licenses/OpenSSL.txt file, and distribute   *
 * linked combinations including the two. You must obey the GNU GPL in all *
 * respects for all of the code used other than OpenSSL.  If you modify    *
 * this file, you may extend this exception to your version of the file,   *
 * but you are not obligated to do so.                                     *
 *                                                                         *
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the terms above, then that            *
 * alternative license agreement takes precedence over these comments.     *
 *                                                                         *
 * Source is provided to this software because we believe users have a     *
 * right to know exactly what a program is going to do before they run it. *
 * This also allows you to audit the software for security holes (none     *
 * have been found so far).                                                *
 *                                                                         *
 * Source code also allows you to port Nmap to new platforms, fix bugs,    *
 * and add new features.  You are highly encouraged to send your changes   *
 * to the dev@nmap.org mailing list for possible incorporation into the    *
 * main distribution.  By sending these changes to Fyodor or one of the    *
 * Insecure.Org development mailing lists, or checking them into the Nmap  *
 * source code repository, it is understood (unless you specify otherwise) *
 * that you are offering the Nmap Project (Insecure.Com LLC) the           *
 * unlimited, non-exclusive right to reuse, modify, and relicense the      *
 * code.  Nmap will always be available Open Source, but this is important *
 * because the inability to relicense code has caused devastating problems *
 * for other Free Software projects (such as KDE and NASM).  We also       *
 * occasionally relicense the code to third parties as discussed above.    *
 * If you wish to specify special license conditions of your               *
 * contributions, just say so when you send them.                          *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the Nmap      *
 * license file for more details (it's in a COPYING file included with     *
 * Nmap, and also available from https://svn.nmap.org/nmap/COPYING         *
 *                                                                         *
 ***************************************************************************/

/* $Id: TargetGroup.cc 30437 2012-12-19 01:10:39Z david $ */

#include "tcpip.h"
#include "TargetGroup.h"
#include "NmapOps.h"
#include "nmap_error.h"
#include "global_structures.h"
#include "libnetutil/netutil.h"

extern NmapOps o;

NewTargets *NewTargets::new_targets;

TargetGroup::TargetGroup() {
  Initialize();
}

// Bring back (or start with) original state
void TargetGroup::Initialize() {
  targets_type = TYPE_NONE;
  memset(addresses, 0, sizeof(addresses));
  memset(current, 0, sizeof(current));
  memset(last, 0, sizeof(last));
  exhausted = true;
}

/* Return a newly allocated string containing the part of expr up to the last
   '/' (or a copy of the whole string if there is no slash). *bits will contain
   the number after the slash, or -1 if there was no slash. In case of error
   return NULL; *bits is then undefined. */
static char *split_netmask(const char *expr, int *bits) {
  const char *slash;

  slash = strrchr(expr, '/');
  if (slash != NULL) {
    long l;
    char *tail;

    l = parse_long(slash + 1, &tail);
    if (tail == slash + 1 || *tail != '\0' || l < 0 || l > INT_MAX)
      return NULL;
    *bits = (int) l;
  } else {
    slash = expr + strlen(expr);
    *bits = -1;
  }

  return mkstr(expr, slash);
}

/* Fill in an in6_addr with a CIDR-style netmask with the given number of bits. */
static void make_ipv6_netmask(struct in6_addr *mask, int bits) {
  unsigned int i;

  memset(mask, 0, sizeof(*mask));

  if (bits < 0)
    bits = 0;
  else if (bits > 128)
    bits = 128;

  if (bits == 0)
    return;

  i = 0;
  /* 0 < bits <= 128, so this loop goes at most 15 times. */
  for ( ; bits > 8; bits -= 8)
    mask->s6_addr[i++] = 0xFF;
  mask->s6_addr[i] = 0xFF << (8 - bits);
}

/* a = (a & mask) | (b & ~mask) */
static void ipv6_or_mask(struct in6_addr *a, const struct in6_addr *mask, const struct in6_addr *b) {
  unsigned int i;

  for (i = 0; i < sizeof(a->s6_addr) / sizeof(*a->s6_addr); i++)
    a->s6_addr[i] = (a->s6_addr[i] & mask->s6_addr[i]) | (b->s6_addr[i] & ~mask->s6_addr[i]);
}

/* Initializes (or reinitializes) the object with a new expression, such
   as 192.168.0.0/16 , 10.1.0-5.1-254 , or fe80::202:e3ff:fe14:1102 .
   Returns 0 for success */
int TargetGroup::parse_expr(const char *target_expr, int af) {

  int i = 0, j = 0, k = 0;
  int start, end;
  char *r, *s;
  char *addy[5];
  char *hostexp;
  int bits;
  namedhost = 0;

  if (targets_type != TYPE_NONE)
    Initialize();

  resolvedaddrs.clear();

  hostexp = split_netmask(target_expr, &bits);
  if (hostexp == NULL) {
    error("Unable to split netmask from target expression: \"%s\"", target_expr);
    goto bail;
  }

  if (af == AF_INET) {

    if (strchr(hostexp, ':')) {
      error("Invalid host expression: %s -- colons only allowed in IPv6 addresses, and then you need the -6 switch", hostexp);
      goto bail;
    }

    /*struct in_addr current_in;*/
    addy[0] = addy[1] = addy[2] = addy[3] = addy[4] = NULL;
    addy[0] = r = hostexp;

    /* Check the netmask. */
    if (bits == -1) {
      netmask = 32;
    } else if (0 <= bits && bits <= 32) {
      netmask = bits;
    } else {
      error("Illegal netmask value, must be /0 - /32 .  Assuming /32 (one host)");
      netmask = 32;
    }

    resolvedname = hostexp;
    for (i = 0; hostexp[i]; i++)
      if (isupper((int) (unsigned char) hostexp[i]) ||
          islower((int) (unsigned char) hostexp[i])) {
        namedhost = 1;
        break;
      }
    if (netmask != 32 || namedhost) {
      struct addrinfo *addrs, *addr;
      struct sockaddr_storage ss;
      size_t sslen;

      targets_type = IPV4_NETMASK;
      addrs = resolve_all(hostexp, AF_INET);
      for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        if (addr->ai_family != AF_INET)
          continue;
        if (addr->ai_addrlen < sizeof(ss)) {
          memcpy(&ss, addr->ai_addr, addr->ai_addrlen);
          resolvedaddrs.push_back(ss);
        }
      }
      if (addrs != NULL)
        freeaddrinfo(addrs);

      if (resolvedaddrs.empty()) {
        error("Failed to resolve given hostname/IP: %s.  Note that you can't use '/mask' AND '1-4,7,100-' style IP ranges. If the machine only has an IPv6 address, add the Nmap -6 flag to scan that.", hostexp);
        goto bail;
      } else {
        ss = *resolvedaddrs.begin();
        sslen = sizeof(ss);
      }

      if (resolvedaddrs.size() > 1 && o.verbose > 1)
        error("Warning: Hostname %s resolves to %lu IPs. Using %s.", hostexp, (unsigned long)resolvedaddrs.size(), inet_ntop_ez(&ss, sslen));

      if (netmask) {
        struct sockaddr_in *sin = (struct sockaddr_in *) &ss;
        unsigned long longtmp = ntohl(sin->sin_addr.s_addr);
        startaddr.s_addr = longtmp & (unsigned long) (0 - (1 << (32 - netmask)));
        endaddr.s_addr = longtmp | (unsigned long)  ((1 << (32 - netmask)) - 1);
      } else {
        /* The above calculations don't work for a /0 netmask, though at first
         * glance it appears that they would
         */
        startaddr.s_addr = 0;
        endaddr.s_addr = 0xffffffff;
      }
      currentaddr = startaddr;
      if (startaddr.s_addr <= endaddr.s_addr)
        goto done;
      fprintf(stderr, "Host specification invalid");
      goto bail;
    } else {
      targets_type = IPV4_RANGES;
      i = 0;

      while (*r) {
        if (*r == '.' && ++i < 4) {
          *r = '\0';
          addy[i] = r + 1;
        } else if (*r != '*' && *r != ',' && *r != '-' && !isdigit((int) (unsigned char) *r)) {
          error("Invalid character in host specification: %s.  Note in particular that square brackets [] are no longer allowed.  They were redundant and can simply be removed.", hostexp);
          goto bail;
        }
        r++;
      }
      if (i != 3) {
        error("Invalid target host specification: %s", hostexp);
        goto bail;
      }

      for (i = 0; i < 4; i++) {
        j = 0;
        do {
          s = strchr(addy[i], ',');
          if (s) *s = '\0';
          if (*addy[i] == '*') {
            start = 0;
            end = 255;
          } else if (*addy[i] == '-') {
            start = 0;
            if (*(addy[i] + 1) == '\0') end = 255;
            else end = atoi(addy[i] + 1);
          } else {
            start = end = atoi(addy[i]);
            if ((r = strchr(addy[i], '-')) && *(r + 1) ) end = atoi(r + 1);
            else if (r && !*(r + 1)) end = 255;
          }
          /* if (o.debugging > 2)
           *   log_write(LOG_STDOUT, "The first host is %d, and the last one is %d\n", start, end); */
          if (start < 0 || start > end || start > 255 || end > 255) {
            error("Your host specifications are illegal!");
            goto bail;
          }
          if (j + (end - start) > 255) {
            error("Your host specifications are illegal!");
            goto bail;
          }
          for (k = start; k <= end; k++)
            addresses[i][j++] = k;
          last[i] = j - 1;
          if (s) addy[i] = s + 1;
        } while (s);
      }
    }
    memset((char *)current, 0, sizeof(current));
  } else {
#if HAVE_IPV6
    struct addrinfo *addrs, *addr;
    struct sockaddr_storage ss;
    size_t sslen;

    assert(af == AF_INET6);

    /* Check the netmask. */
    if (bits == -1) {
      netmask = 128;
    } else if (0 <= bits && bits <= 128) {
      netmask = bits;
    } else {
      error("Illegal netmask value, must be /0 - /128 .  Assuming /128 (one host)");
      netmask = 128;
    }
    resolvedname = hostexp;
    if (strchr(hostexp, ':') == NULL)
      namedhost = 1;

    targets_type = IPV6_NETMASK;
    addrs = resolve_all(hostexp, AF_INET6);
    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
      if (addr->ai_family != AF_INET6)
        continue;
      if (addr->ai_addrlen < sizeof(ss)) {
        memcpy(&ss, addr->ai_addr, addr->ai_addrlen);
        resolvedaddrs.push_back(ss);
      }
    }
    if (addrs != NULL)
      freeaddrinfo(addrs);

    if (resolvedaddrs.empty()) {
      error("Failed to resolve given IPv6 hostname/IP: %s.  Note that you can't use '/mask' or '[1-4,7,100-]' style ranges for IPv6.", hostexp);
      goto bail;
    } else {
      ss = *resolvedaddrs.begin();
      sslen = sizeof(ss);
    }

    if (resolvedaddrs.size() > 1 && o.verbose > 1)
      error("Warning: Hostname %s resolves to %lu IPs. Using %s.", hostexp, (unsigned long)resolvedaddrs.size(), inet_ntop_ez(&ss, sslen));

    assert(sizeof(ip6) <= sslen);
    memcpy(&ip6, &ss, sizeof(ip6));

    const struct in6_addr zeros = { { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} } };
    const struct in6_addr ones = { { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} } };

    struct in6_addr mask;
    make_ipv6_netmask(&mask, netmask);

    startaddr6 = ((struct sockaddr_in6 *) &ss)->sin6_addr;
    ipv6_or_mask(&startaddr6, &mask, &zeros);
    endaddr6 = ((struct sockaddr_in6 *) &ss)->sin6_addr;
    ipv6_or_mask(&endaddr6, &mask, &ones);

    currentaddr6 = startaddr6;
#else // HAVE_IPV6
    fatal("IPv6 not supported on your platform");
#endif // HAVE_IPV6
  }

done:
  free(hostexp);
  exhausted = false;
  return 0;

bail:
  free(hostexp);
  exhausted = true;
  return 1;
}

/* Get the sin6_scope_id member of a sockaddr_in6, based on a device name. This
   is used to assign scope to all addresses that otherwise lack a scope id when
   the -e option is used. */
static int get_scope_id(const char *devname) {
  struct interface_info *ii;

  if (devname == NULL || devname[0] == '\0')
    return 0;
  ii = getInterfaceByName(devname, AF_INET6);
  if (ii != NULL)
    return ii->ifindex;
  else
    return 0;
}

/* Grab the next host from this expression (if any) and updates its internal
   state to reflect that the IP was given out.  Returns 0 and
   fills in ss if successful.  ss must point to a pre-allocated
   sockaddr_storage structure */
int TargetGroup::get_next_host(struct sockaddr_storage *ss, size_t *sslen) {

  int octet;
  struct sockaddr_in *sin = (struct sockaddr_in *) ss;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;
startover: /* to handle nmap --resume where I have already
              * scanned many of the IPs */
  assert(ss);
  assert(sslen);


  if (exhausted)
    return -1;

  if (targets_type == IPV4_NETMASK) {
    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    *sslen = sizeof(struct sockaddr_in);
#if HAVE_SOCKADDR_SA_LEN
    sin->sin_len = *sslen;
#endif

    if (currentaddr.s_addr == endaddr.s_addr)
      exhausted = true;
    if (currentaddr.s_addr <= endaddr.s_addr) {
      sin->sin_addr.s_addr = htonl(currentaddr.s_addr++);
    } else {
      error("Bogus target structure passed to %s", __func__);
      exhausted = true;
      return -1;
    }
  } else if (targets_type == IPV4_RANGES) {
    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    *sslen = sizeof(struct sockaddr_in);
#if HAVE_SOCKADDR_SA_LEN
    sin->sin_len = *sslen;
#endif
    if (o.debugging > 2) {
      log_write(LOG_STDOUT, "doing %d.%d.%d.%d = %d.%d.%d.%d\n", current[0], current[1], current[2], current[3], addresses[0][current[0]], addresses[1][current[1]], addresses[2][current[2]], addresses[3][current[3]]);
    }
    /* Set the IP to the current value of everything */
    sin->sin_addr.s_addr = htonl(addresses[0][current[0]] << 24 |
                                 addresses[1][current[1]] << 16 |
                                 addresses[2][current[2]] <<  8 |
                                 addresses[3][current[3]]);

    /* Now we nudge up to the next IP */
    for (octet = 3; octet >= 0; octet--) {
      if (current[octet] < last[octet]) {
        /* OK, this is the column I have room to nudge upwards */
        current[octet]++;
        break;
      } else {
        /* This octet is finished so I reset it to the beginning */
        current[octet] = 0;
      }
    }
    if (octet == -1) {
      /* It didn't find anything to bump up, I must have taken the last IP */
      exhausted = true;
      /* So I set current to last with the very final octet up one ... */
      /* Note that this may make current[3] == 256 */
      current[0] = last[0];
      current[1] = last[1];
      current[2] = last[2];
      current[3] = last[3] + 1;
    } else {
      assert(!exhausted); /* There must be at least one more IP left */
    }
  } else {
    assert(targets_type == IPV6_NETMASK);
#if HAVE_IPV6
    *sslen = sizeof(struct sockaddr_in6);
    memset(sin6, 0, *sslen);
    sin6->sin6_family = AF_INET6;
#ifdef SIN_LEN
    sin6->sin6_len = *sslen;
#endif /* SIN_LEN */

    if (memcmp(currentaddr6.s6_addr, endaddr6.s6_addr, 16) == 0)
      exhausted = true;

    sin6->sin6_addr = currentaddr6;
    if (ip6.sin6_scope_id == 0)
      sin6->sin6_scope_id = get_scope_id(o.device);
    else
      sin6->sin6_scope_id = ip6.sin6_scope_id;

    /* Increment current address. */
    for (int i = 15; i >= 0; i--) {
      currentaddr6.s6_addr[i]++;
      if (currentaddr6.s6_addr[i] > 0)
	break;
    }
#else
    fatal("IPV6 not supported on this platform");
#endif // HAVE_IPV6
  }

  /* If we are resuming from a previous scan, we have already finished
     scans up to o.resume_ip.  */
  if (sin->sin_family == AF_INET && o.resume_ip.s_addr) {
    if (o.resume_ip.s_addr == sin->sin_addr.s_addr)
      o.resume_ip.s_addr = 0; /* So that we will KEEP the next one */
    goto startover; /* Try again */
  }

  return 0;
}

/* Returns the last given host, so that it will be given again next
     time get_next_host is called.  Obviously, you should only call
     this if you have fetched at least 1 host since parse_expr() was
     called */
int TargetGroup::return_last_host() {
  int octet;

  exhausted = false;
  if (targets_type == IPV4_NETMASK) {
    assert(currentaddr.s_addr > startaddr.s_addr);
    currentaddr.s_addr--;
  } else if (targets_type == IPV4_RANGES) {
    for (octet = 3; octet >= 0; octet--) {
      if (current[octet] > 0) {
        /* OK, this is the column I have room to nudge downwards */
        current[octet]--;
        break;
      } else {
        /* This octet is already at the beginning, so I set it to the end */
        current[octet] = last[octet];
      }
    }
    assert(octet != -1);
  } else {
    assert(targets_type == IPV6_NETMASK);
  }
  return 0;
}

/* Returns true iff the given address is the one that was resolved to create
   this target group; i.e., not one of the addresses derived from it with a
   netmask. */
bool TargetGroup::is_resolved_address(const struct sockaddr_storage *ss) {
  struct sockaddr_storage resolvedaddr;

  if (resolvedaddrs.empty())
    return false;
  /* We only have a single distinguished address for these target types.
     IPV4_RANGES doesn't, for example. */
  if (!(targets_type == IPV4_NETMASK || targets_type == IPV6_NETMASK))
    return false;
  resolvedaddr = *resolvedaddrs.begin();

  return sockaddr_storage_equal(&resolvedaddr, ss);
}

/* Return a string of the name or address that was resolved for this group. */
const char *TargetGroup::get_resolved_name(void) {
  return resolvedname.c_str();
}

/* Return the list of addresses that the name for this group resolved to, if
   it came from a name resolution. */
const std::list<struct sockaddr_storage> &TargetGroup::get_resolved_addrs(void) {
  return resolvedaddrs;
}

/* debug level for the adding target is: 3 */
NewTargets *NewTargets::get (void) {
  if (new_targets)
    return new_targets;
  new_targets = new NewTargets();
  return new_targets;
}

NewTargets::NewTargets (void) {
  Initialize();
}

void NewTargets::Initialize (void) {
  history.clear();
  while (!queue.empty())
    queue.pop();
}

/* This private method is used to push new targets to the
 * queue. It returns the number of targets in the queue. */
unsigned long NewTargets::push (const char *target) {
  std::pair<std::set<std::string>::iterator, bool> pair_iter;
  std::string tg(target);

  if (tg.length() > 0) {
    /* save targets in the scanned history here (NSE side). */
    pair_iter = history.insert(tg);

    /* A new target */
    if (pair_iter.second == true) {
      /* push target onto the queue for future scans */
      queue.push(tg);

      if (o.debugging > 2)
        log_write(LOG_PLAIN, "New Targets: target %s pushed onto the queue.\n", tg.c_str());
    } else {
      if (o.debugging > 2)
        log_write(LOG_PLAIN, "New Targets: target %s is already in the queue.\n", tg.c_str());
      /* Return 1 when the target is already in the history cache,
       * this will prevent returning 0 when the target queue is
       * empty since no target was added. */
      return 1;
    }
  }

  return queue.size();
}

/* Reads a target from the queue and return it to be pushed
 * onto Nmap scan queue */
std::string NewTargets::read (void) {
  std::string str;

  /* check to see it there are targets in the queue */
  if (!new_targets->queue.empty()) {
    str = new_targets->queue.front();
    new_targets->queue.pop();
  }

  return str;
}

void NewTargets::clear (void) {
  new_targets->history.clear();
}

unsigned long NewTargets::get_number (void) {
  return new_targets->history.size();
}

unsigned long NewTargets::get_scanned (void) {
  return new_targets->history.size() - new_targets->queue.size();
}

unsigned long NewTargets::get_queued (void) {
  return new_targets->queue.size();
}

/* This is the function that is used by nse_nmaplib.cc to add
 * new targets.
 * Returns the number of targets in the queue on success, or 0 on
 * failures or when the queue is empty. */
unsigned long NewTargets::insert (const char *target) {
  if (*target) {
    if (new_targets == NULL) {
      error("ERROR: to add targets run with -sC or --script options.");
      return 0;
    }
    if (o.current_scantype == SCRIPT_POST_SCAN) {
      error("ERROR: adding targets is disabled in the Post-scanning phase.");
      return 0;
    }
    if (strlen(target) >= 1024) {
      error("ERROR: new target is too long (>= 1024), failed to add it.");
      return 0;
    }
  }

  return new_targets->push(target);
}

/* Lookahead is the number of hosts that can be
   checked (such as ping scanned) in advance.  Randomize causes each
   group of up to lookahead hosts to be internally shuffled around.
   The target_expressions array MUST REMAIN VALID IN MEMORY as long as
   this class instance is used -- the array is NOT copied.
 */
HostGroupState::HostGroupState(int lookahead, int rnd,
                               char *expr[], int numexpr) {
  assert(lookahead > 0);
  hostbatch = (Target **) safe_zalloc(sizeof(Target *) * lookahead);
  max_batch_sz = lookahead;
  current_batch_sz = 0;
  next_batch_no = 0;
  randomize = rnd;
  target_expressions = expr;
  num_expressions = numexpr;
  next_expression = 0;
}

HostGroupState::~HostGroupState() {
  free(hostbatch);
}
