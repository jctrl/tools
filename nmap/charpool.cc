
/***************************************************************************
 * charpool.cc -- Handles Nmap's "character pool" memory allocation        *
 * system.                                                                 *
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

/* $Id: charpool.cc 30346 2012-12-06 01:21:42Z fyodor $ */

#include <stddef.h>

#include "nbase.h"

/* Character pool memory allocation */
#include "charpool.h"
#include "nmap_error.h"

static char *charpool[16];
static int currentcharpool;
static int currentcharpoolsz;
static char *nextchar;

/* Allocated blocks are allocated to multiples of ALIGN_ON. This is the
   definition used by the malloc in Glibc 2.7, which says that it "suffices for
   nearly all current machines and C compilers." */
#define ALIGN_ON (2 * sizeof(size_t))

static int cp_init(void) {
  static int charpool_initialized = 0;
  if (charpool_initialized) return 0;

  /* Create our char pool */
  currentcharpool = 0;
  currentcharpoolsz = 16384;
  nextchar = charpool[0] = (char *) safe_malloc(currentcharpoolsz);
  charpool_initialized = 1;
  return 0;
}

void cp_free(void) {
  int ccp;
  for(ccp=0; ccp <= currentcharpool; ccp++)
    if(charpool[ccp]){
      free(charpool[ccp]);
      charpool[ccp] = NULL;
  }
  currentcharpool = 0;
}

static inline void cp_grow(void) {
  /* Doh!  We've got to make room */
  if (++currentcharpool > 15) {
    fatal("Character Pool is out of buckets!");
  }
  currentcharpoolsz <<= 1;

  nextchar = charpool[currentcharpool] = (char *)
    safe_malloc(currentcharpoolsz);
}

void *cp_alloc(int sz) {
  char *p;
  int modulus;

  cp_init();

  if ((modulus = sz % ALIGN_ON))
    sz += ALIGN_ON - modulus;
  
  if ((nextchar - charpool[currentcharpool]) + sz <= currentcharpoolsz) {
    p = nextchar;
    nextchar += sz;
    return p;
  }
  /* Doh!  We've got to make room */
  cp_grow();

 return cp_alloc(sz);
 
}

char *cp_strdup(const char *src) {
const char *p;
char *q;
/* end points to the first illegal char */
char *end;
int modulus;

 cp_init();

 end = charpool[currentcharpool] + currentcharpoolsz;
 q = nextchar;
 p = src;
 while((nextchar < end) && *p) {
   *nextchar++ = *p++;
 }

 if (nextchar < end) {
   /* Goody, we have space */
   *nextchar++ = '\0';
   if ((modulus = (nextchar - q) % ALIGN_ON))
     nextchar += ALIGN_ON - modulus;
   return q;
 }

 /* Doh!  We ran out -- need to allocate more */
 cp_grow();

 return cp_strdup(src);
}