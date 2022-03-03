void
_main()
{
	main();
	asm("illegal");
}

#define NVM_BIOS ((long (*)(int,...))0x2404L)

char buf[16*1024];

void
main()
{
	int handle;

	NVM_BIOS(0, "APP0", buf);
	handle = NVM_BIOS(1, "File 16K", 16*1024L);	/* create file */
	NVM_BIOS(3,handle);				/* close it */

	handle = NVM_BIOS(1, "File 512", 512L);
	NVM_BIOS(3, handle);
}
