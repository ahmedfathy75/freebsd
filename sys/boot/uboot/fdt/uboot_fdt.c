/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <stand.h>
#include <fdt_platform.h>

#include "glue.h"

#define STR(number) #number
#define STRINGIFY(number) STR(number)

int
fdt_platform_load_dtb(void)
{
	struct fdt_header *hdr;
	const char *s;
	char *p;

	/*
	 * If the U-boot environment contains a variable giving the address of a
	 * valid blob in memory, use it.  The U-boot README says the right
	 * variable for fdt data loaded into ram is fdt_addr_r, so try that
	 * first.  Board vendors also use both fdtaddr and fdt_addr names.
	 */
	s = ub_env_get("fdt_addr_r");
	if (s == NULL)
		s = ub_env_get("fdtaddr");
	if (s == NULL)
		s = ub_env_get("fdt_addr");
	if (s != NULL && *s != '\0') {
		hdr = (struct fdt_header *)strtoul(s, &p, 16);
		if (*p == '\0') {
			if (fdt_load_dtb_addr(hdr) == 0) {
				printf("Using DTB provided by U-Boot at "
				    "address %p.\n", hdr);
				return (0);
			}
		}
	}

	/*
	 * If the U-boot environment contains a variable giving the name of a
	 * file, use it if we can load and validate it.
	 */
	s = ub_env_get("fdtfile");
	if (s == NULL)
		s = ub_env_get("fdt_file");
	if (s != NULL && *s != '\0') {
		if (fdt_load_dtb_file(s) == 0) {
			printf("Loaded DTB from file '%s'.\n", s);
			return (0);
		}
	}

	return (1);
}

void
fdt_platform_fixups(void)
{
	struct fdt_mem_region regions[3];
	const char *env, *str;
	char *end, *ethstr;
	int eth_no, i, len, n;
	struct sys_info *si;

	env = NULL;
	eth_no = 0;
	ethstr = NULL;

	/* Acquire sys_info */
	si = ub_get_sys_info();

	while ((env = ub_env_enum(env)) != NULL) {
		if (strncmp(env, "eth", 3) == 0 &&
		    strncmp(env + (strlen(env) - 4), "addr", 4) == 0) {
			/*
			 * Handle Ethernet addrs: parse uboot env eth%daddr
			 */

			if (!eth_no) {
				/*
				 * Check how many chars we will need to store
				 * maximal eth iface number.
				 */
				len = strlen(STRINGIFY(TMP_MAX_ETH)) +
				    strlen("ethernet") + 1;

				/*
				 * Reserve mem for string "ethernet" and len
				 * chars for iface no.
				 */
				ethstr = (char *)malloc(len * sizeof(char));
				bzero(ethstr, len * sizeof(char));
				strcpy(ethstr, "ethernet0");
			}

			/* Extract interface number */
			i = strtol(env + 3, &end, 10);
			if (end == (env + 3))
				/* 'ethaddr' means interface 0 address */
				n = 0;
			else
				n = i;

			if (n > TMP_MAX_ETH)
				continue;

			str = ub_env_get(env);

			if (n != 0) {
				/*
				 * Find the lenght of the interface id by
				 * taking in to account the first 3 and
				 * last 4 characters.
				 */
				i = strlen(env) - 7;
				strncpy(ethstr + 8, env + 3, i);
			}

			/* Modify blob */
			fdt_fixup_ethernet(str, ethstr, len);

			/* Clear ethernet..XXXX.. string */
			bzero(ethstr + 8, len - 8);

			if (n + 1 > eth_no)
				eth_no = n + 1;
		} else if (strcmp(env, "consoledev") == 0) {
			str = ub_env_get(env);
			fdt_fixup_stdout(str);
		}
	}

	/* Modify cpu(s) and bus clock frequenties in /cpus node [Hz] */
	fdt_fixup_cpubusfreqs(si->clk_cpu, si->clk_bus);

	/* Copy the data into a useful form */
	for (i = 0; i < si->mr_no; i++) {
		if (i > nitems(regions)) {
			i = nitems(regions);
			break;
		}

		regions[i].start = si->mr[i].start;
		regions[i].size = si->mr[i].size;
	}

	/* Fixup memory regions */
	fdt_fixup_memory(regions, i);
}
