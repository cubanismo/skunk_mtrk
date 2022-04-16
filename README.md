Skunkboard Memory Track Emulator
================================

This project contains files to generate a ROM image that, when flashed to a
Revision 2 or higher Skunkboard running BIOS verion 3.0.3 or higher (For board
revisions 2-4) or BIOS version 4.0.1 or higher (For board revisions 5+), will
cause the Skunkboard to emulate the behavior of the original Atari Memory Track
NVRAM cartridge. Additionally, it includes most of the source code for the
original Atari Memory Track cartridge, with some modifications to adapt it to
the new use case and modern compilers.

To get started as a user:
-------------------------

* First make sure you have the latest version of JCP and have used it to update
your Skunkboard to the latest BIOS. Head over to the
[skunk_jcp](https://github.com/cubanismo/skunk_jcp) repository to get JCP,
download the variant for your operating system from the "Releases" section,
install it, and run `jcp -u` to update your Skunkboard BIOS.

* Now, a word of caution: **Flashing the Skunkboard Memory Track ROM to the
first bank of your Skunkboard as described here will also clear any content in
the second bank of the Skunkboard the first time it runs.** This is because the
first bank is used for the Skunkboard Memory Track code, while the second bank
is used for the Skunkboard Memory Track content, i.e. game save and settings
files.

* Next, back on this page, look for the
"[Releases](https://github.com/cubanismo/skunk_mtrk/releases/)" section on the
right. Click on the link for the latest release, download the zip file, extract
it, and flash "skunk_mtrk-\<version\>.rom" to the first bank of your Skunkboard
as follows, using version 1.0.0 as an example:

    `jcp -f skunk_mtrk-01_00_00.rom`

* Note that if your Skunkboard is plugged in to a Jaguar CD unit when this
command is run, it will load the Skunkboard Memory Track flash driver and then
likely crash and display some random screen content. This is because the ROM
expects to be run in a special mode that allows it to return to the Jaguar CD
BIOS when a Jaguar CD unit is present, and this is not possible when run from
the Skunkboard BIOS interface as the jcp flasher does. Regardless, this is
harmless. Just run `jcp -r` to reboot your Skunkboard after flashing completes.
If run without a Jaguar CD unit installed, you should see the Skunkboard Memory
Track Manager screen after flashing completes.

* To actually use the memory track manager with a game, be sure you have the
Skunkboard plugged in to a Jaguar CD unit, put a CD game that supports the
Memory Track cartridge in it, hold down the C button on your Jaguar
controller, and turn on the Jaguar system. The game should boot and detect the
presence of a memory track cartridge.

* To use the memory track manager interface to view or delete the list of files
on the memory track, either hold down both the Option and C buttons while
powering on your Jaguar system, or remove the Jaguar CD unit, plug the
Skunkboard directly into the Jaguar system, turn it on, and use whatever method
you normally use to boot the first bank of your Skunkboard (E.g., press Up on
the Jaguar controller when the Skunkboard green bootscreen appears).

* The file stand_mtrk-\<version\>.cof in release packages is a stand-alone build
of the memory track file manager interface. It can be loaded into memory and run
using JCP as described below if desired, but generally end users do not need to
use this file. It is included for completeness. **WARNING:** As with the ROM
version, running this program on your Jaguar will clobber the contents of the
second bank of your Skunkboard if it has not previously been initialized to
contain a valid Skunkboard Memory Track filesystem!

Transfering Memory Track data to/from a computer
------------------------------------------------

Currently there is no mechanism to transfer individual files. However, it is
possible to save and restore the entire Memory Track data region:

* To save all the Skunkboard Memory Track content, dump the second bank of the
Skunkboard back to your computer using the following command:

    `jcp -2d backup.mtdd`

* Later, restore it by flashing the same file back to the second bank:

    `jcp -2f backup.mtdd`

That's it! Enjoy your CD games!

Compatibility
-------------

The Skunkboard Memory Track cartridge is compatible with all the original run of
Atari Jaguar CD games that support the original Memory Track cartridge, which
includes the following games:

1. Baldies
2. Battlemorph
3. Blue Lightning
4. Brain Dead 13\*
5. Highlander: The Last of the MacLeods
6. Hover Strike: Unconquered Lands
7. Iron Soldier 2
8. Myst
9. Primal Rage
10. Vid Grid

It is known to be incompatible with the following games:

1. ULS boot games, such as the freeware Reboot Games releases.

Other games have not been tested thus far. Feel free to report
working/non-working games as issues or pull requests modifying this file.

\* The Memory Track functionality in Brain Dead 13 is broken, on both the
original Memory Track cartridge and with the Skunkboard Memory Track ROM. Games
can be saved, but will not load.

To get started as a developer:
------------------------------

* Fetch and set up my [Jaguar SDK](https://github.com/cubanismo/jaguar-sdk).
* Source the env.sh file from the jaguar-sdk
* Run "make" in the skunk_mtrk directory.

This should leave you with many object, bin, abs, cof, and rom files. The
important ones are rom.rom (The Skunkboard Memory Track rom) and stand.cof (The
standalone version of the memory track manager). You can test any changes you
make by running stand.cof in RAM with a Skunkboard attached:

    jcp stand.cof

Note this will clobber the second bank of your Skunkboard unless it contains a
valid memory track filesystem.

If you edit the Makefile to enable Skunklib support, you can use the printf
function in your code to aid in debugging. For this to work, you must launch
the JCP console using the -c option:

    jcp -c stand.cof

When you think you're done debugging, flash the ROM to the first bank of the
Skunkboard for final testing, following the getting started directions for users
above, replacing "skunk_mtrk-\<version\>.rom" with "rom.rom".

Original Memory Track Documentation:
------------------------------------

The NVRAM boot rom automatically copies the NVRAM
BIOS into low memory when it starts up.

If you hold down the "Option" key (and keep it held down)
during the boot sequence, you will be presented with the
"Save Cartridge Manager" application. This application
allows users to delete files and to see how much
free space is available on the cartridge. The "Save
Cartridge Manager" application understands the
following keys:

Normal commands
---------------
* up arrow/down arrow	selects files
* A,B,C			to delete a file
* OPTION		to choose how to sort files
* OPTION+1		to save preferences in a file
* OPTION+\*+\#		to erase all files
* \*+\#			to exit the manager

Debugging commands
------------------
* OPTION+\*+0+\#	to do a test of free memory
* OPTION+7+9		to create a (dummy) file

Self-test function
------------------

To run the diagnostic self-test at the factory:

1. Plug the cartridge into a machine with no CD-ROM.
2. Turn the machine on.
3. Hold down the \*, 0, and # keys (on controller 1)
   while keeping all of these down, press the Option
   key.
4. Press the right arrow (to select "Yes" on the
   dialog box).
5. Press the B button.

The screen will turn blue, and a dialog box will appear
that will indicate the progress of the test. 8 blocks
will be written, and then read back. If an error
occurs, the screen will turn red and a message will
be printed. If no error occurs, the screen will turn
green.

Licensing Issues
----------------

Many of these files contain Atari copyright notices. However, Atari released all
IP rights associated with the Atari Jaguar into the public domain while under
the ownership of Hasbro:

https://www.atariage.com/Jaguar/archives/HasbroRights.html

My interpretation of that statement is that these files are effectively in the
public domain. Please consult a lawyer if you have any concerns about the
licensing of the relevant files. I have left the copyright notices in place to
make them easy to identify.
