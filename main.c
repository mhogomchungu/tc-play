/*
 * Copyright (c) 2011 Alex Hornung <alex@alexhornung.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <err.h>
#include <time.h>
#include <libutil.h>

#include "tc-play.h"

static
void
sig_handler(int sig)
{
	if ((sig == SIGUSR1 || sig == SIGINFO) && (summary_fn != NULL))
		summary_fn();
}

static
void
usage(void)
{
	fprintf(stderr,
	    "Usage: tc-play <command> [options]\n"
	    "Valid commands and its arguments are:\n"
	    " -c, --create\n"
	    "\t Creates a new TC volume on the device specified by -d or --device\n"
	    " -i, --info\n"
	    "\t Gives information about the TC volume specified by -d or --device\n"
	    " -m <mapping name>, --map=<mapping name>\n"
	    "\t Creates a dm-crypt mapping with the given name for the device\n"
	    "\t specified by -d or --device\n"
	    "\nValid options and its arguments for 'create' are:\n"
	    " -a <pbkdf prf algorithm>, --pbkdf-prf=<pbkdf prf algorithm>\n"
	    "\t specifies which hashing function to use for the PBKDF password\n"
	    "\t derivation when creating a new volume\n"
	    "\t To see valid options, specify -a help\n"
	    " -b <cipher>, --cipher=<cipher>\n"
	    "\t specifies which cipher to use when creating a new TC volume\n"
	    "\t To see valid options, specify -b help\n"
	    " -g, --hidden\n"
	    "\t specifies that the newly created volume will contain a hidden volume\n"
	    "\nValid options and its arguments for 'info' and 'map' are:\n"
	    " -e, --protect-hidden\n"
	    "\t protect a hidden volume when mounting the outer volume\n"
	    " -s <disk path>, --system-encryption=<disk path>\n"
	    "\t specifies that the disk (e.g. /dev/da0) is using system encryption\n"
	    "\nValid options and its arguments common to all commands are:\n"
	    " -d <device path>, --device=<device path>\n"
	    "\t specifies the path to the volume to operate on (e.g. /dev/da0s1)\n"
	    " -k <key file>, --keyfile=<key file>\n"
	    "\t specifies a key file to use for the password derivation, can appear\n"
	    "\t multiple times\n"
	    " -f <key file>, --keyfile-hidden=<key file>\n"
	    "\t specifies a key file to use for the hidden volume password derivation\n"
	    "\t This option is only valid in combination with -e, --protect-hidden\n"
	    "\t or -g, --hidden\n"
	    );

	exit(1);
}

static struct option longopts[] = {
	{ "create",		no_argument,		NULL, 'c' },
	{ "cipher",		required_argument,	NULL, 'b' },
	{ "hidden",		no_argument,		NULL, 'g' },
	{ "pbkdf-prf",		required_argument,	NULL, 'a' },
	{ "info",		no_argument,		NULL, 'i' },
	{ "map",		required_argument,	NULL, 'm' },
	{ "keyfile",		required_argument,	NULL, 'k' },
	{ "keyfile-hidden",	required_argument,	NULL, 'f' },
	{ "protect-hidden",	no_argument,		NULL, 'e' },
	{ "device",		required_argument,	NULL, 'd' },
	{ "system-encryption",	required_argument,	NULL, 's' },
	{ "version",		no_argument,		NULL, 'v' },
	{ "help",		no_argument,		NULL, 'h' },
	{ NULL,			0,			NULL, 0   },
};

int
main(int argc, char *argv[])
{
	const char *dev = NULL, *sys_dev = NULL, *map_name = NULL;
	const char *keyfiles[MAX_KEYFILES];
	const char *h_keyfiles[MAX_KEYFILES];
	int nkeyfiles;
	int n_hkeyfiles;
	int ch, error;
	int sflag = 0, info_vol = 0, map_vol = 0, protect_hidden = 0,
	    create_vol = 0, contain_hidden = 0;
	struct pbkdf_prf_algo *prf = NULL;
	struct tc_cipher_chain *cipher_chain = NULL;

	tc_play_init();
	signal(SIGUSR1, sig_handler);
	signal(SIGINFO, sig_handler);

	nkeyfiles = 0;
	n_hkeyfiles = 0;

	while ((ch = getopt_long(argc, argv, "a:b:cd:efgh:ik:m:s:v", longopts,
	    NULL)) != -1) {
		switch(ch) {
		case 'a':
			if (prf != NULL)
				usage();
			if ((prf = check_prf_algo(optarg, 0)) == NULL) {
				if (strcmp(optarg, "help") == 0)
					exit(0);
				else
					usage();
			}
			break;
		case 'b':
			if (cipher_chain != NULL)
				usage();
			if ((cipher_chain = check_cipher_chain(optarg, 0)) == NULL) {
				if (strcmp(optarg, "help") == 0)
					exit(0);
				else
					usage();
			}
			break;
		case 'c':
			create_vol = 1;
			break;
		case 'd':
			dev = optarg;
			break;
		case 'e':
			protect_hidden = 1;
			break;
		case 'f':
			h_keyfiles[n_hkeyfiles++] = optarg;
			break;
		case 'g':
			contain_hidden = 1;
			break;
		case 'i':
			info_vol = 1;
			break;
		case 'k':
			keyfiles[nkeyfiles++] = optarg;
			break;
		case 'm':
			map_vol = 1;
			map_name = optarg;
			break;
		case 's':
			sflag = 1;
			sys_dev = optarg;
			break;
		case 'v':
			printf("tc-play v%d.%d\n", MAJ_VER, MIN_VER);
			exit(0);
			/* NOT REACHED */
		case 'h':
		case '?':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	argc -= optind;
	argv += optind;

	/* Check arguments */
	if (!((map_vol || info_vol || create_vol) && dev != NULL) ||
	    (map_vol && info_vol) ||
	    (map_vol && create_vol) ||
	    (create_vol && info_vol) ||
	    (contain_hidden && !create_vol) ||
	    (sflag && (sys_dev == NULL)) ||
	    (map_vol && (map_name == NULL)) ||
	    (!(protect_hidden || create_vol) && n_hkeyfiles > 0)) {
		usage();
		/* NOT REACHED */
	}

	/* Create a new volume */
	if (create_vol) {
		error = create_volume(dev, contain_hidden, keyfiles, nkeyfiles,
		    h_keyfiles, n_hkeyfiles, prf, cipher_chain, NULL, NULL,
		    0, 1 /* interactive */);
		if (error) {
			tc_log(1, "could not create new volume on %s\n", dev);
		}
	} else if (info_vol) {
		error = info_volume(dev, sflag, sys_dev, protect_hidden,
		    keyfiles, nkeyfiles, h_keyfiles, n_hkeyfiles, NULL, NULL,
		    1 /* interactive */, DEFAULT_RETRIES);
	} else if (map_vol) {
		error = map_volume(map_name,
		    dev, sflag, sys_dev, protect_hidden,
		    keyfiles, nkeyfiles, h_keyfiles, n_hkeyfiles, NULL, NULL,
		    1 /* interactive */, DEFAULT_RETRIES);
	}

	return error;
}