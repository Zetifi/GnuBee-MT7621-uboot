/*
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/**************************************************************************
 *
 * Support for persistent environment data
 *
 * The "environment" is stored as a list of '\0' terminated
 * "name=value" strings. The end of the list is marked by a double
 * '\0'. New entries are always added at the end. Deleting an entry
 * shifts the remaining entries to the front. Replacing an entry is a
 * combination of deleting the old value and adding the new one.
 *
 * The environment is preceeded by a 32 bit CRC over the data part.
 *
 **************************************************************************
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <watchdog.h>
#include <serial.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#include <rt_mmap.h>

#if (CONFIG_COMMANDS & CFG_CMD_NET)
#include <net.h>
#endif

#if !defined(CFG_ENV_IS_IN_NVRAM)	&& \
    !defined(CFG_ENV_IS_IN_EEPROM)	&& \
    !defined(CFG_ENV_IS_IN_FLASH)	&& \
    !defined(CFG_ENV_IS_IN_DATAFLASH)	&& \
    !defined(CFG_ENV_IS_IN_NAND)	&& \
    !defined(CFG_ENV_IS_IN_SPI)	&& \
    !defined(CFG_ENV_IS_NOWHERE)
# error Define one of CFG_ENV_IS_IN_{NVRAM|EEPROM|FLASH|DATAFLASH|SPI|NOWHERE}
#endif

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/************************************************************************
************************************************************************/

/* Function that returns a character from the environment */
extern uchar (*env_get_char)(int);

/* Function that returns a pointer to a value from the environment */
/* (Only memory version supported / needed). */
extern uchar *env_get_addr(int);

/* Function that updates CRC of the enironment */
extern void env_crc_update (void);

/************************************************************************
************************************************************************/

static int envmatch (uchar *, int);

/*
 * Table with supported baudrates (defined in config_xyz.h)
 */
static const unsigned long baudrate_table[] = CFG_BAUDRATE_TABLE;
#define	N_BAUDRATES (sizeof(baudrate_table) / sizeof(baudrate_table[0]))

extern ulong sys_bootflag;

/************************************************************************
 * Command interface: print one or all environment variables
 */
#ifdef RALINK_CMDLINE
int do_printenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, j, k, nxt;
	int rcode = 0;

	if (argc == 1) {		/* Print all env variables	*/
		for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
			for (nxt=i; env_get_char(nxt) != '\0'; ++nxt)
				;
			for (k=i; k<nxt; ++k)
				putc(env_get_char(k));
			putc  ('\n');

			if (ctrlc()) {
				puts ("\n ** Abort\n");
				return 1;
			}
		}

		printf("\nEnvironment size: %d/%d bytes\n", i, ENV_SIZE);

		return 0;
	}

	for (i=1; i<argc; ++i) {	/* print single env variables	*/
		char *name = argv[i];

		k = -1;

		for (j=0; env_get_char(j) != '\0'; j=nxt+1) {

			for (nxt=j; env_get_char(nxt) != '\0'; ++nxt)
				;
			k = envmatch(name, j);
			if (k < 0) {
				continue;
			}
			puts (name);
			putc ('=');
			while (k < nxt)
				putc(env_get_char(k++));
			putc ('\n');
			break;
		}
		if (k < 0) {
			printf ("## Error: \"%s\" not defined\n", name);
			rcode ++;
		}
	}
	return rcode;
}
#endif // RALINK_CMDLINE //

/************************************************************************
 * Set a new environment variable,
 * or replace or delete an existing one.
 *
 * This function will ONLY work with a in-RAM copy of the environment
 */

int _do_setenv (int flag, int argc, char *argv[])
{
	DECLARE_GLOBAL_DATA_PTR;

	int   i, len, oldval;
	int   console = -1;
	uchar *env, *nxt = NULL;
	uchar *name;
	bd_t *bd = gd->bd;

	uchar *env_data = env_get_addr(0);

	if (!env_data)	/* need copy in RAM */
		return 1;

	name = argv[1];

	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	for (env=env_data; *env; env=nxt+1) {
		for (nxt=env; *nxt; ++nxt)
			;
		if ((oldval = envmatch(name, env-env_data)) >= 0)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval >= 0) {
#ifndef CONFIG_ENV_OVERWRITE

		/*
		 * Ethernet Address and serial# can be set only once,
		 * ver is readonly.
		 */
		if ( (strcmp (name, "serial#") == 0) ||
		    ((strcmp (name, "ethaddr") == 0)
#if defined(CONFIG_OVERWRITE_ETHADDR_ONCE) && defined(CONFIG_ETHADDR)
		     && (strcmp (env_get_addr(oldval),MK_STR(CONFIG_ETHADDR)) != 0)
#endif	/* CONFIG_OVERWRITE_ETHADDR_ONCE && CONFIG_ETHADDR */
		    ) ) {
			printf ("Can't overwrite \"%s\"\n", name);
			return 1;
		}
#endif

		if (strcmp(argv[1],"bootflag") == 0) {
			ulong flag = simple_strtoul(argv[2], NULL, 16);
			if (BOOT_FLAG_IMAGE_A != flag && BOOT_FLAG_IMAGE_B != flag) {
				printf("wrong flag, the bootflag can only be either 0x5A5A5A5A (for #img A) or 0xA5A5A5A5 (for #img B)\n");
				return 1;
			} else {
				// If the bootflag is invalid, then proceed to the image A
				sys_bootflag = flag;
			}
		}

		/* Check for console redirection */
		if (strcmp(name,"stdin") == 0) {
			console = stdin;
		} else if (strcmp(name,"stdout") == 0) {
			console = stdout;
		} else if (strcmp(name,"stderr") == 0) {
			console = stderr;
		}

		if (console != -1) {
			if (argc < 3) {		/* Cannot delete it! */
				printf("Can't delete \"%s\"\n", name);
				return 1;
			}

			/* Try assigning specified device */
			if (console_assign (console, argv[2]) < 0)
				return 1;

#ifdef CONFIG_SERIAL_MULTI
			if (serial_assign (argv[2]) < 0)
				return 1;
#endif
		}

		/*
		 * Switch to new baudrate if new baudrate is supported
		 */
		if (strcmp(argv[1],"baudrate") == 0) {
			int baudrate = simple_strtoul(argv[2], NULL, 10);
			int i;
			for (i=0; i<N_BAUDRATES; ++i) {
				if (baudrate == baudrate_table[i])
					break;
			}
			if (i == N_BAUDRATES) {
				printf ("## Baudrate %d bps not supported\n",
					baudrate);
				return 1;
			}
			printf ("## Switch baudrate to %d bps and press ENTER ...\n",
				baudrate);
			udelay(50000);
			gd->baudrate = baudrate;
#ifdef CONFIG_PPC
			gd->bd->bi_baudrate = baudrate;
#endif

#if defined(RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
			bbu_uart_init();
#else
			serial_setbrg ();
#endif

			udelay(50000);
			for (;;) {
				if (getc() == '\r')
				      break;
			}
		}

		if (*++nxt == '\0') {
			if (env > env_data) {
				env--;
			} else {
				*env = '\0';
			}
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

#ifdef CONFIG_NET_MULTI
	if (strncmp(name, "eth", 3) == 0) {
		char *end;
		int   num = simple_strtoul(name+3, &end, 10);

		if (strcmp(end, "addr") == 0) {
			eth_set_enetaddr(num, argv[2]);
		}
	}
#endif


	/* Delete only ? */
	if ((argc < 3) || argv[2] == NULL) {
		env_crc_update ();
		return 0;
	}

	/*
	 * Append new definition at the end
	 */
	for (env=env_data; *env || *(env+1); ++env)
		;
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i=2; i<argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	if (len > (&env_data[ENV_SIZE]-env)) {
		printf ("## Error: environment overflow, \"%s\" deleted\n", name);
		return 1;
	}
	while ((*env = *name++) != '\0')
		env++;
	for (i=2; i<argc; ++i) {
		char *val = argv[i];

		*env = (i==2) ? '=' : ' ';
		while ((*++env = *val++) != '\0')
			;
	}

	/* end is marked with double '\0' */
	*++env = '\0';

	/* Update CRC */
	env_crc_update ();

	/*
	 * Some variables should be updated when the corresponding
	 * entry in the enviornment is changed
	 */

	if (strcmp(argv[1],"ethaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		for (i=0; i<6; ++i) {
			bd->bi_enetaddr[i] = s ? simple_strtoul(s, &e, 16) : 0;
			if (s) s = (*e) ? e+1 : e;
		}
#ifdef CONFIG_NET_MULTI
		eth_set_enetaddr(0, argv[2]);
#endif
		return 0;
	}

	if (strcmp(argv[1],"ipaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		unsigned long addr;
		bd->bi_ip_addr = 0;
		for (addr=0, i=0; i<4; ++i) {
			ulong val = s ? simple_strtoul(s, &e, 10) : 0;
			addr <<= 8;
			addr  |= (val & 0xFF);
			if (s) s = (*e) ? e+1 : e;
		}
		bd->bi_ip_addr = htonl(addr);
		return 0;
	}
	if (strcmp(argv[1],"loadaddr") == 0) {
		load_addr = simple_strtoul(argv[2], NULL, 16);
		return 0;
	}
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if (strcmp(argv[1],"bootfile") == 0) {
		copy_filename (BootFile, argv[2], sizeof(BootFile));
		return 0;
	}
#endif	/* CFG_CMD_NET */

#ifdef CONFIG_AMIGAONEG3SE
	if (strcmp(argv[1], "vga_fg_color") == 0 ||
	    strcmp(argv[1], "vga_bg_color") == 0 ) {
		extern void video_set_color(unsigned char attr);
		extern unsigned char video_get_attr(void);

		video_set_color(video_get_attr());
		return 0;
	}
#endif	/* CONFIG_AMIGAONEG3SE */

#if 0
	if (strcmp(argv[1],"twe0") == 0) {
		printf("\n Reset  to Flash environment  \n");
	{

		unsigned int regvalue,kk0;


		kk0 = simple_strtoul(argv[2], NULL, 16);
		
		regvalue = *(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0308);

	printf("\n Default FLASH_CS1_CFG = %08X \n",regvalue);

    regvalue &= ~(0x3 << 26);
	regvalue |= (0x1 << 26);

	regvalue |= (0x1 << 24);

	regvalue &= ~(0x3 << 20);
	regvalue |= (0x1 << 20);

	regvalue &= ~(0x3 << 16);
	regvalue |= (0x1 << 16);

	regvalue &= ~(0xF << 12);
	regvalue |= (kk0 << 12);


	*(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0308) = regvalue;

	regvalue = *(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0308);

		}
	}
#endif

	return 0;
}

void setenv (char *varname, char *varvalue)
{
	char *argv[4] = { "setenv", varname, varvalue, NULL };
	_do_setenv (0, 3, argv);
}

int do_setenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	return _do_setenv (flag, argc, argv);
}

/************************************************************************
 * Prompt for environment variable
 */

#if (CONFIG_COMMANDS & CFG_CMD_ASKENV)
int do_askenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char console_buffer[CFG_CBSIZE];
	char message[CFG_CBSIZE];
	int size = CFG_CBSIZE - 1;
	int len;
	char *local_args[4];

	local_args[0] = argv[0];
	local_args[1] = argv[1];
	local_args[2] = NULL;
	local_args[3] = NULL;

	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	/* Check the syntax */
	switch (argc) {
	case 1:
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;

	case 2:		/* askenv envname */
		sprintf (message, "Please enter '%s':", argv[1]);
		break;

	case 3:		/* askenv envname size */
		sprintf (message, "Please enter '%s':", argv[1]);
		size = simple_strtoul (argv[2], NULL, 10);
		break;

	default:	/* askenv envname message1 ... messagen size */
	    {
		int i;
		int pos = 0;

		for (i = 2; i < argc - 1; i++) {
			if (pos) {
				message[pos++] = ' ';
			}
			strcpy (message+pos, argv[i]);
			pos += strlen(argv[i]);
		}
		message[pos] = '\0';
		size = simple_strtoul (argv[argc - 1], NULL, 10);
	    }
		break;
	}

	if (size >= CFG_CBSIZE)
		size = CFG_CBSIZE - 1;

	if (size <= 0)
		return 1;

	/* prompt for input */
	len = readline (message, 0);

	if (size < len)
		console_buffer[size] = '\0';

	len = 2;
	if (console_buffer[0] != '\0') {
		local_args[2] = console_buffer;
		len = 3;
	}

	/* Continue calling setenv code */
	return _do_setenv (flag, len, local_args);
}
#endif	/* CFG_CMD_ASKENV */

/************************************************************************
 * Look up variable from environment,
 * return address of storage for that variable,
 * or NULL if not found
 */

char *getenv (uchar *name)
{
	int i, nxt;

	WATCHDOG_RESET();

	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (NULL);
			}
		}
		if ((val=envmatch(name, i)) < 0)
			continue;
		return (env_get_addr(val));
	}

	return (NULL);
}

#if 0
int getenv_r (uchar *name, uchar *buf, unsigned len)
{
	int i, nxt;

	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val, n;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (-1);
			}
		}
		if ((val=envmatch(name, i)) < 0)
			continue;
		/* found; copy out */
		n = 0;
		while ((len > n++) && (*buf++ = env_get_char(val++)) != '\0')
			;
		if (len == n)
			*buf = '\0';
		return (n);
	}
	return (-1);
}
#endif

#ifdef RALINK_CMDLINE
#if defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) || \
    ((CONFIG_COMMANDS & (CFG_CMD_ENV|CFG_CMD_FLASH)) == \
      (CFG_CMD_ENV|CFG_CMD_FLASH))
int do_saveenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char * env_name_spec;

	printf ("Saving Environment to %s...\n", env_name_spec);

	return (saveenv() ? 1 : 0);
}
#endif
#endif // RALINK_CMDLINE //

/************************************************************************
 * Match a name / name=value pair
 *
 * s1 is either a simple 'name', or a 'name=value' pair.
 * i2 is the environment index for a 'name2=value2' pair.
 * If the names match, return the index for the value2, else NULL.
 */
static int
envmatch (uchar *s1, int i2)
{

	while (*s1 == env_get_char(i2++))
		if (*s1++ == '=')
			return(i2);
	if (*s1 == '\0' && env_get_char(i2-1) == '=')
		return(i2);
	return(-1);
}


#ifdef RALINK_CMDLINE
/**************************************************/

U_BOOT_CMD(
	printenv, CFG_MAXARGS, 1,	do_printenv,
	"printenv- print environment variables\n",
	"\n    - print values of all environment variables\n"
	"printenv name ...\n"
	"    - print value of environment variable 'name'\n"
);

U_BOOT_CMD(
	setenv, CFG_MAXARGS, 0,	do_setenv,
	"setenv  - set environment variables\n",
	"name value ...\n"
	"    - set environment variable 'name' to 'value ...'\n"
	"setenv name\n"
	"    - delete environment variable 'name'\n"
);

#if defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) || \
    ((CONFIG_COMMANDS & (CFG_CMD_ENV|CFG_CMD_FLASH)) == \
      (CFG_CMD_ENV|CFG_CMD_FLASH))
U_BOOT_CMD(
	saveenv, 1, 0,	do_saveenv,
	"saveenv - save environment variables to persistent storage\n",
	NULL
);

#endif
#endif	/* CFG_CMD_ENV */

#if (CONFIG_COMMANDS & CFG_CMD_ASKENV)

U_BOOT_CMD(
	askenv,	CFG_MAXARGS,	1,	do_askenv,
	"askenv  - get environment variables from stdin\n",
	"name [message] [size]\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name\n"
	"    - get environment variable 'name' from stdin\n"
	"askenv name size\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name [message] size\n"
	"    - display 'message' string and get environment variable 'name'"
	"from stdin (max 'size' chars)\n"
);
#endif	/* CFG_CMD_ASKENV */

#if (CONFIG_COMMANDS & CFG_CMD_RUN)
int do_run (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
U_BOOT_CMD(
	run,	CFG_MAXARGS,	1,	do_run,
	"run     - run commands in an environment variable\n",
	"var [...]\n"
	"    - run the commands in the environment variable(s) 'var'\n"
);
#endif  /* CFG_CMD_RUN */
