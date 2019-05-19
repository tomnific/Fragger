/*
 * Copyright (c) 2019 Tom. All Rights Reserved.
 *
 * @TOM_LICENSE_HEADER_START@
 *
 * 1) Credit would be sick, but I really can't control what you do ¯\_(ツ)_/¯
 * 2) I'm not responsible for what you do with this AND I'm not responsible for any damage you cause ("THIS SOFTWARE IS PROVIDED AS IS", etc)
 * 3) I'm under no obligation to provide support. (But if you reach out I'll gladly take a look if I have time)
 *
 * @TOM_LICENSE_HEADER_END@
 */
/*
 * A collection of useful functions, particularly with obscure usecases
 */

#pragma once
#ifndef stdtom_h
#define stdtom_h

#include <stdio.h>
#include <stdbool.h>
#include <string>




#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

// just in case a more explicit name is desired
//#define log(format, ...) lprintf(format, ##__VA_ARGS__)

// TODO
//// alt name for dgblprintf
//#define dbglog(format, ...) dbglprintf(format, ##__VA_ARGS__)
//
//// for debugging with normal printf
//#define dbgprintf(format, ...) printf(__FILE__, __LINE__, format, ##__VA_ARGS__)
#pragma clang diagnostic pop


// do nothing - primarily for use to get rid of the 'variable not used' warning when warnings are treated as errors
#define NOTUSED(x) (void)(x)
#define NOOP(x) (void)(x)



// unsigned
#define BYTE   unsigned char           // 1 Byte
#define WORD   unsigned short int      // 2 Bytes
#define DWORD  unsigned int            // 4 Bytes
#define QWORD  unsigned long long int  // 8 Bytes





using namespace std;





namespace tom
{
	// clears the console
	void printclr(void);
	
	
	// selects bits `from` through `to` from `source`.
	short select_bit_range_from_word(WORD source, int from, int through);
	
	
	// converts a string to an integer - unlike atoi & friends, it tells you if and why it fails
	int strtonum(string numstr, int minval, int maxval, string *errstrp);
	
	
	// creates a string from a format
	string string_with_format(string format, ...);
	
	// appends s2 to s1 and returns the result
	char* cstrapp(const char* s1, const char* s2);
	
	// trims strings down to their real size. Also removes newline characters.
	char* cstrtrim(char* str);
	
	// appends char to string (one small caveat:  if the string isn't big enough this will break)
	void cstr_append_char(char* str, char character);
	
	// reads the contents of a file into a string
	string str_from_file_contents(string filename);
	
	// zeros out string - apprently there's a thing called bzero also does this?
	void zero_cstr(char** str);
	
	// checks if a string contains all 0's (as it won't be equal to NULL)
	bool cstr_is_0d(char* str);
	
	
	// gets the file size for a file
	size_t filesize(string file_path);
	
	
#ifdef __APPLE__
	/*
	 gets the path to the executable belonging the the handle given by dlopen()
	 Warning: this is not a thread safe function
	 */
	string path_from_handle(void* handle);
	
	
	
	// retruns the path to the executable that calls it
	string path_to_current_executable(void);
#endif
	
	
	// appends component to path with "/" inbetween
	string append_path_component(string path, string component);
	
	
	// returns the number of path components in a string - or -1 on error
	int count_path_components(string path);
	
	int count_path_components_cstr(char* path);
	
	
	// returns the path component at the index
	string get_path_component(string path, int index);
	
	string get_path_component(char* path, int index);
	
	char* get_path_component_cstr(char* path, int index);
	
	
	// getopt - but actually resonable to use
	char* getarg_cstr(int argc, char* argv[], char* argname);
	
	string getarg(int argc, char* argv[], char* argname);
	
	string getarg(int argc, char* argv[], string argname);
	
	
	// checks if arg is present 
	bool arg_is_present(int argc, char* argv[], char* argname);
	
	bool arg_is_present(int argc, char* argv[], string argname);
	
	
	// set the prefix used by lprintf
	void set_lprefix(string new_prefix);
	
	// printf, but with a prefix and ending newling - 'l' for 'log'
	void lprintf(const char* format, ...);
	
	
	// print the defined meaning for an error value
	void printerrno(int err);
}


#endif /* stdtom_h */
