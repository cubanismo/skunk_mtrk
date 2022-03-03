#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nvm.h"

byte NVRAM[128*1024L];	/* 128K of NVRAM */
int debug_flag;
extern long NVM_Bios();
unsigned char scratch[16*1024];		/* scratch buffer for NV BIOS */

void
dohelp( void )
{
	printf("Valid commands:\n\n");
	printf("c name size -- create a file with the given name and size\n");
	printf("d app name  -- delete the file with the given name\n");
	printf("i app       -- initialize with application name\n");
	printf("l           -- list all files\n");
	printf("q           -- quit\n");
	printf("r hndl size -- read (and print) size bytes from the file with the given handle\n");
	printf("s hndl pos  -- seek to position in file with given handle\n");
	printf("w hndl      -- write data from stdin to the given file\n");
	printf("x hndl      -- close the file with the given handle\n");
	printf("F           -- dump the FAT table\n");
	printf("?           -- get this message\n");
	printf("!           -- toggle debug flag\n");
}

void
dumpfat( void )
{
	int i;
	int j;

	for (i = 0; i < 256; i+=16) {
		printf("%04x: ", i);
		for (j = 0; j < 16; j++) {
			printf(" %02x", NVRAM[i+j]);
		}
		printf("\n");
	}
}

void
dolist( void )
{
	SEARCHBUF sb;
	long ret;

	printf("%-20s %-10s %-5s\n\n", "Application", "File Name", "Size");
	ret = NVM_Bios(7, &sb, 0L);
	while (ret == 0) {
		printf("%-20s %-10s %-5ld\n", sb.appname, sb.filename, sb.size);
		ret = NVM_Bios(8, &sb, 0L);
	}
	printf("Exit code %ld\n", ret);
}

/*
 * execute a command
 * returns 0 if we are not finished, 1 if the user quit
 */
int
docmd( int cmdlet, char *cmdargs )
{
	char *name;
	long ret, i;
	static char iobuf[1024];

	while (*cmdargs && isspace(*cmdargs))
		cmdargs++;

	switch( cmdlet ) {
	case 'c':
		name = cmdargs;
		while (*cmdargs && !isspace(*cmdargs))
			cmdargs++;
		*cmdargs++ = 0;
		while (*cmdargs && isspace(*cmdargs))
			cmdargs++;
		if (!*cmdargs)
			dohelp();
		else {
			ret = atol(cmdargs);
			ret = NVM_Bios(1, name, ret);
			printf("Exit code: %ld\n", ret);
		}
		break;
	case 'd':
		name = cmdargs;
		while (*cmdargs && !isspace(*cmdargs))
			cmdargs++;
		*cmdargs++ = 0;
		while (*cmdargs && isspace(*cmdargs))
			cmdargs++;
		if (!*cmdargs)
			dohelp();
		else {
			ret = NVM_Bios(4, name, cmdargs);
			printf("Exit code: %ld\n", ret);
		}
		break;
	case 'i':
		if (!*cmdargs)
			dohelp();
		else {
			ret = NVM_Bios(0, cmdargs, scratch);
			printf("Exit code: %ld\n", ret);
		}
		break;
	case 'l':
		dolist();
		break;
	case 'q':		/* quit */
		return 1;
	case 'r':
		name = cmdargs;
		while (*cmdargs && !isspace(*cmdargs))
			cmdargs++;
		*cmdargs++ = 0;
		while (*cmdargs && isspace(*cmdargs))
			cmdargs++;
		if (!*cmdargs)
			dohelp();
		else {
			ret = NVM_Bios(5, (int)atol(name), iobuf, atol(cmdargs));
			printf("Exit code: %ld\n", ret);
			printf("Data read:[");
			for (i = 0; i < ret; i++) {
				putchar(iobuf[i]);
			}
			printf("] (End of Data)\n");
		}
		break;
	case 's':
		name = cmdargs;
		while (*cmdargs && !isspace(*cmdargs))
			cmdargs++;
		*cmdargs++ = 0;
		while (*cmdargs && isspace(*cmdargs))
			cmdargs++;
		if (!*cmdargs)
			dohelp();
		else {
			ret = NVM_Bios(9, (int)atol(name), atol(cmdargs), 0);
			printf("Exit code: %ld\n", ret);
		}
		break;

	case 'w':
		while (*cmdargs && isspace(*cmdargs))
			cmdargs++;
		if (!*cmdargs)
			dohelp();
		else {
			printf("Enter data to write:\n");
			i = 0;
			do {
				iobuf[i] = getchar();
				i++;
			} while (iobuf[i-1] != '\n');
			--i;
			ret = NVM_Bios(6, (int)atol(cmdargs), iobuf, i);
			printf("Exit code: %ld\n", ret);
		}
		break;
	case 'x':
		ret = NVM_Bios(3, (int)atol(cmdargs));
		printf("Exit code: %ld\n", ret);
		break;
	case 'F':
		dumpfat();
		break;
	case '?':
		dohelp();
		break;
	case '!':
		debug_flag = 1-debug_flag;
		printf("Debug flag now %s\n", debug_flag ? "ON" : "OFF");
		break;
	}
	return 0;
}

int
main()
{
	char cmdbuf[80];
	char cmd;
	char *cmdargs;

	int done = 0;
	while(!done) {
		printf("Cmd> ");
		fflush(stdout);
		gets(cmdbuf);
		cmd = cmdbuf[0];
		cmdargs = cmdbuf+2;
		done = docmd(cmd, cmdargs);
	}
	return 0;
}

