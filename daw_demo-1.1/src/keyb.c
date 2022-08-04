/*
kbhit() and getch() for Linux/UNIX
Chris Giese <geezer@execpc.com>	http://my.execpc.com/~geezer
*/

#include "keyb.h"
#include <stdio.h>
#include <io.h>

/* COMMON FUNCTIONS */

int stdinterm() { // detects whether the stdin is connected to a terminal or is being redirected - PORTABLE
	fpos_t pos;
	fgetpos(stdin,&pos);
	pos++;
	if (fsetpos(stdin,&pos)) return 1;
	rewind(stdin);
	return 0;

	/* This may seem a bit long-winded but I suspect it's the most reliable way, since a simple
						return _isatty(_fileno(stdin));
	   might give incorrect results if the program is run from a batch file with input redirection.
	   Not that it's likely that it'll ever be so, but we can never know.
	*/
}


#ifdef LINUX

/* LINUX-ONLY FUNCTIONS */

    #include <sys/time.h> /* struct timeval, select() */
    #include <termios.h> /* tcgetattr(), tcsetattr() */
    #include <stdlib.h> /* atexit(), exit() */
    #include <unistd.h> /* read() */
    #include <stdio.h> /* printf() */
    #include <string.h> /* memcpy() */

static struct termios g_old_kbd_mode;

/*****************************************************************************/
static void cooked(void)
{
	tcsetattr(0, TCSANOW, &g_old_kbd_mode);
}

static void raw(void)
{
	static char init;
/**/
	struct termios new_kbd_mode;

	if(init)
		return;
/* put keyboard (stdin, actually) in raw, unbuffered mode */
	tcgetattr(0, &g_old_kbd_mode);
	memcpy(&new_kbd_mode, &g_old_kbd_mode, sizeof(struct termios));
	new_kbd_mode.c_lflag &= ~(ICANON | ECHO);
	new_kbd_mode.c_cc[VTIME] = 0;
	new_kbd_mode.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_kbd_mode);
/* when we exit, go back to normal, "cooked" mode */
	atexit(cooked);

	init = 1;
}

/*****************************************************************************/
/*  GETCH  */
/*****************************************************************************/
int getch(void)
{
	unsigned char temp;

	raw();
    /* stdin = fd 0 */
	if(read(0, &temp, 1) != 1)
		return 0;
	return temp;

}


/*****************************************************************************/
/*  KBHIT  */
/*****************************************************************************/
int kbhit()
{

	struct timeval timeout;
	fd_set read_handles;
	int status;

	raw();
    /* check stdin (fd 0) for activity */
	FD_ZERO(&read_handles);
	FD_SET(0, &read_handles);
	timeout.tv_sec = timeout.tv_usec = 0;
	status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
	if(status < 0)
	{
		printf("select() failed in kbhit()\n");
		exit(1);
	}
    return (status);
}

void funcsetup() {
	getch_ = getch;
	kbhit_ = kbhit;
}

#else  // Windows

/* WINDOWS-ONLY FUNCTIONS */

#include <conio.h>
#include <time.h>
#include <Windows.h>



static int peekpipe() { // strictly Win32
	DWORD avail;
	PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), NULL, 0, NULL, &avail, NULL);
	return avail; 
}
	

void funcsetup() {  // sets the correct stdin functions depending on the result of stdinterm
	if (stdinterm()) {
		getch_ = _getch;
		kbhit_ = _kbhit;
	} else {
		getch_ = getchar;
		kbhit_ = peekpipe;
	}

}

#endif

