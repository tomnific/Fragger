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
 * Basic functions for operating on a FAT filesystem
 */

#ifndef fatutils_hpp
#define fatutils_hpp

#ifdef __XCODE__
#include <stdtom/stdtom.hpp>
#else
#include "../stdtom/stdtom.hpp"
#endif

#include <stdio.h>
#include <list>





namespace fat
{
	typedef struct {
		string oem;                            // 8 bytes
		WORD  bytes_per_sector;                // 2 bytes
		BYTE  sectors_per_cluster;             // 1 byte
		WORD  reserved_sectors_count;          // 2 bytes
		BYTE  fat_count;                       // 1 byte
		WORD  max_root_directory_entry_count;  // 2 bytes
		WORD  logical_sector_count_word;       // 2 bytes
		DWORD logical_sector_count_dword;      // 4 bytes
		BYTE  media_descriptor;                // 1 byte
		WORD  sectors_per_fat_word;            // 2 bytes
		DWORD sectors_per_fat_dword;           // 4 bytes
	} boot_sector_info_t;


	typedef struct {
		// currently, parallell data structures don't make sense.
	} boot_sector_info_offsets_t;




	typedef struct {
		string short_name;                     // 8 bytes
		BYTE short_extension[3];               // 3 bytes
		BYTE attributes;                       // 1 byte
		BYTE first_character_of_deleted_file;  // 1 byte    // Same offset as create_time_ms, so we won't used this.
		BYTE create_time_ms;                   // 1 byte
		WORD create_time_hr_m_s;               // 2 bytes (15-11 = hours; 10-5 = minutes; 4-0 = seconds) [seconds are divided in half]
		WORD create_date;                      // 2 bytes
		WORD access_date;                      // 2 bytes
		WORD extended_attributes;              // 2 bytes
		WORD last_modified_time_hr_m_s;        // 2 bytes (15-11 = hours; 10-5 = minutes; 4-0 = seconds) [seconds are divided in half]
		WORD last_modified_date;               // 2 bytes
		WORD start_location;                   // 2 bytes
		DWORD size;                            // 4 bytes
	} directory_entry_t;




	typedef struct {
		unsigned int milliseconds;
		unsigned int seconds;
		unsigned int minutes;
		unsigned int hours;
		unsigned int day;
		unsigned int month;
		unsigned int year;
	} timestamp_t;




	// reads size amount of memory from an offset into base to the buffer
	void read(void* buffer, void* base, long offset, unsigned int size);


	// writes size amount of memory from a buffer to an offset in the base
	void write(void* buffer, void* base, long offset, unsigned int size);


	// maps an image into memory
	void* map_image_to_memory(string image_path);


	// gets contents of a file from its directory entry
	string get_contents_of_file_from_directory_entry(directory_entry_t file_entry, void* base);


	// returns array of all clusters from a starting cluster
	list<int> file_clusters_for_entry_at(int start_cluster, void* base);


	// checks if the value is an EOF value
	bool is_fat_eof(unsigned int value);


	// tests if the first timestamp is larger than the second. (false on equal)
	bool timestamp_is_older_than(timestamp_t first, timestamp_t second);


	// makes a partial timestamp (for deleted fileS) into a string
	string partial_timestamp_to_str(timestamp_t timestamp);

	// makes a fulle timestamp into a string
	string full_timestamp_to_str(timestamp_t timestamp);


	// converts raw bytes into a timestamp
	timestamp_t raw_to_timestamp(BYTE milliseconds, WORD time, WORD calendar_date);


	// prints a directory entry to the console
	void print_directory_entry(directory_entry_t directory_entry);

	// prints boot sector info to the console
	void print_boot_sector_info(boot_sector_info_t boot_sector_info);


	// gets the boot sector info for an image
	boot_sector_info_t get_boot_sector_info(void* mapped_image);


	// get directory entry at offset
	directory_entry_t get_directory_entry_at_offset(int offset, void* base);

	// find directory entry by its file name
	directory_entry_t find_directory_entry_by_name(string filename, void* base);
}


#endif /* fatutils_hpp */
