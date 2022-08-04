/*
 *  terminal_io.h
 *  Author: Jakash3
 *  Date: 22 Dec 2011
 *  Some conio.h functions for GNU/Linux
 */
#ifndef TERMINAL_IO
#define TERMINAL_IO
 
#ifndef WIN32

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
 
// Turns terminal line buffering on or off
void terminal_lnbuf(int yn);
 
// Turns terminal keyboard echo on or off
void terminal_echo(int yn);
 
// Get next immediate character input (no echo)
int getch();
 
// Get next immediate character input (with echo)
int getche();
 
// Check if a key has been pressed at terminal
int kbhit();
 
// Set cursor position
void gotoxy(int x, int y);
 
// Clear terminal screen and set cursor to top left
void clrscr();

#endif

#endif

