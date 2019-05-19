/*
 * Copyright (c) 2019 Tom. All Rights Reserved.
 *
 * @TOM_LICENSE_SOURCE_START@
 *
 * 1) Credit would be sick, but I really can't control what you do ¯\_(ツ)_/¯
 * 2) I'm not responsible for what you do with this AND I'm not responsible for any damage you cause ("THIS SOFTWARE IS PROVIDED AS IS", etc)
 * 3) I'm under no obligation to provide support. (But if you reach out I'll gladly take a look if I have time)
 *
 * @TOM_LICENSE_SOURCE_END@
 */
/*
 * Basic functions for operating on a FAT filesystem
 */

#include "fatutils.hpp"

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__
#include <string.h>
#endif


#include "../stdtom/stdtom.hpp"





using namespace tom;
using namespace std;




namespace fat
{
	void read(void* buffer, void* base, long offset, unsigned int size)
	{
		memcpy(buffer, (static_cast<char *>(base) + offset), size);
	}




	void write(void* buffer, void* base, long offset, unsigned int size)
	{
		memcpy((static_cast<char *>(base) + offset), buffer, size);
	}




	void* map_image_to_memory(string image_path)
	{
		int image_fd = open(image_path.c_str(), O_RDWR, 0);
		
		if (image_fd == -1)
		{
			lprintf("ERROR: Cannot open image at path '%s'.", image_path.c_str());
			
			
			exit(EXIT_FAILURE);
		}
		
		
		size_t image_size = filesize(image_path);
		
		
		void* mapped_image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, image_fd, 0); //was MAP_PRIVATE
		
		if (mapped_image != MAP_FAILED)
		{
			return mapped_image;
		}
		
		
		return NULL;
	}




	string get_contents_of_file_from_directory_entry(directory_entry_t file_entry, void* base)
	{
		string file_contents = "";
		
		// definitely a little redundant...
		unsigned int number_of_reserved_sectors = 0;
		unsigned int number_of_fats = 0;
		unsigned int sectors_per_fat = 0;
		unsigned int bytes_per_sector = 0;
		unsigned int cluster_size = 0;
		unsigned int fat_offset = 0;
		unsigned int fat_size = 0;
		unsigned int root_directory_offset = 0;
		unsigned int root_directory_size = 0;
		unsigned int data_region_offset = 0;
		unsigned int start_cluster = file_entry.start_location;
		
		boot_sector_info_t boot_sector_info;
		
		boot_sector_info = get_boot_sector_info(base);
		
		
		number_of_reserved_sectors = boot_sector_info.reserved_sectors_count;
		
		number_of_fats = boot_sector_info.fat_count;
		
		if (boot_sector_info.sectors_per_fat_word == 0)
		{
			sectors_per_fat = boot_sector_info.sectors_per_fat_dword;
		}
		else
		{
			sectors_per_fat = boot_sector_info.sectors_per_fat_word;
		}
		
		bytes_per_sector = boot_sector_info.bytes_per_sector;
		
		cluster_size = boot_sector_info.sectors_per_cluster * boot_sector_info.bytes_per_sector;
		
		
		fat_offset = (boot_sector_info.reserved_sectors_count * boot_sector_info.bytes_per_sector);
		fat_size = (boot_sector_info.fat_count * sectors_per_fat * boot_sector_info.bytes_per_sector);
		
		root_directory_offset = ((sectors_per_fat * number_of_fats) * bytes_per_sector) + (number_of_reserved_sectors * bytes_per_sector);
		root_directory_size = (boot_sector_info.max_root_directory_entry_count * 32);
		
		
		data_region_offset = fat_offset + fat_size + root_directory_size;
		
		
		if (start_cluster == 0 || start_cluster == 1)
		{
			//		printf("EOF\n");
			
			
			return "";
		}
		
		
		number_of_reserved_sectors = boot_sector_info.reserved_sectors_count;
		bytes_per_sector = boot_sector_info.bytes_per_sector;
		
		
		fat_offset = (number_of_reserved_sectors * bytes_per_sector);// + 2; // + 2 factored into cluster number
		
		
		WORD fat_entry = start_cluster;
		long cluster_size_in_bytes = (boot_sector_info.sectors_per_cluster * boot_sector_info.bytes_per_sector);
		long bytes_left = file_entry.size;
		
		while (!is_fat_eof(fat_entry) && fat_entry != 0)
		{
			char cluster[cluster_size_in_bytes];
			
			int sectors_per_fat = boot_sector_info.sectors_per_fat_word;
			
			if (sectors_per_fat == 0)
			{
				sectors_per_fat = boot_sector_info.sectors_per_fat_dword;
			}
			
			unsigned long file_offset = data_region_offset + ((fat_entry - 2) * cluster_size_in_bytes);
			
			if (bytes_left > 0)
			{
				if (bytes_left < cluster_size_in_bytes)
				{
					read(cluster, base, file_offset, (int)bytes_left);
					
					for (int i = 0; i < bytes_left; i++)
					{
						file_contents.push_back(cluster[i]);
					}
				}
				else
				{
					read(cluster, base, file_offset, (int)cluster_size_in_bytes);
					
					for (int i = 0; i < cluster_size_in_bytes; i++)
					{
						file_contents.push_back(cluster[i]);
					}
				}
			}
			
			bytes_left -= cluster_size_in_bytes;
			
			memset(cluster, 0, cluster_size_in_bytes);
			read(&fat_entry, base, fat_offset + (fat_entry * 2), 2);
		}
		
		//	printf("%s\n\n", file_contents.c_str());
		return file_contents;
	}




	list<int> file_clusters_for_entry_at(int start_cluster, void* base)
	{
		list<int> clusters;
		
		unsigned int number_of_reserved_sectors;
		unsigned int bytes_per_sector;
		unsigned int fat_offset;
		
		
		if (start_cluster == 0 || start_cluster == 1)
		{
			clusters.push_back(-1);
			
			
			return clusters;
		}
		
		
		boot_sector_info_t boot_sector_info = get_boot_sector_info(base);
		
		number_of_reserved_sectors = boot_sector_info.reserved_sectors_count;
		bytes_per_sector = boot_sector_info.bytes_per_sector;
		
		
		fat_offset = (number_of_reserved_sectors * bytes_per_sector);// + 2; // + 2 factored into cluster number
		
		
		WORD fat_entry;
		read(&fat_entry, base, fat_offset + (start_cluster * 2), 2);
		clusters.push_back(start_cluster);
		
		while (!is_fat_eof(fat_entry) && fat_entry != 0)
		{
			clusters.push_back(fat_entry);
			read(&fat_entry, base, fat_offset + (fat_entry * 2), 2);
		}
		
		clusters.push_back(-1);
		
		
		return clusters;
	}




	bool is_fat_eof(unsigned int value)
	{
		if (value == 0xFFF8 || value == 0xFFF9 || value == 0xFFFA || value == 0xFFFB || value == 0xFFFC || value == 0xFFFD || value == 0xFFFE || value == 0xFFFF)
		{
			return true;
		}
		else
		{
			return false;
		}
	}





	bool timestamp_is_older_than(timestamp_t first, timestamp_t second)
	{
		if (first.year < second.year)
		{
			return true;
		}
		else if (first.year > second.year)
		{
			return false;
		}
		else if (first.year == second.year)
		{
			if (first.month < second.month)
			{
				return true;
			}
			else if (first.month > second.month)
			{
				return false;
			}
			else if (first.month == second.month)
			{
				if (first.day < second.day)
				{
					return true;
				}
				else if (first.day > second.day)
				{
					return false;
				}
				else if (first.day == second.day)
				{
					if (first.hours < second.hours)
					{
						return true;
					}
					else if (first.hours > second.hours)
					{
						return false;
					}
					else if (first.hours == second.hours)
					{
						if (first.minutes < second.minutes)
						{
							return true;
						}
						else if (first.minutes > second.minutes)
						{
							return false;
						}
						else if (first.minutes == second.minutes)
						{
							if (first.seconds < second.seconds)
							{
								return true;
							}
							else if (first.seconds > second.seconds)
							{
								return false;
							}
							else if (first.seconds == second.seconds)
							{
								if (first.milliseconds < second.milliseconds)
								{
									return true;
								}
								else if (first.milliseconds > second.milliseconds)
								{
									return false;
								}
								else if (first.milliseconds == second.milliseconds)
								{
									// exact same date
									return false;
								}
							}
						}
					}
				}
			}
		}
		
		
		printf("[fatutils] ERROR: timestamps were neither larger the each other nor equal to each other...\n");
		
		
		return false;
	}




	string partial_timestamp_to_str(timestamp_t timestamp)
	{
		string timestamp_string;
		
		
		timestamp_string = string_with_format("%d/%02d/%02d %02d:%02d:%02d.000", timestamp.year, timestamp.month, timestamp.day, timestamp.hours, timestamp.minutes, timestamp.seconds);
		
		
		return timestamp_string;
	}




	string full_timestamp_to_str(timestamp_t timestamp)
	{
		string timestamp_string;
		
		
		timestamp_string = string_with_format("%d/%02d/%02d %02d:%02d:%02d.%03d", timestamp.year, timestamp.month, timestamp.day, timestamp.hours, timestamp.minutes, timestamp.seconds, timestamp.milliseconds);
		
		
		return timestamp_string;
	}




	timestamp_t raw_to_timestamp(BYTE milliseconds, WORD time, WORD calendar_date)
	{
		timestamp_t date;
		
		
		date.milliseconds = milliseconds;
		
		
		date.seconds = 2 * select_bit_range_from_word(time, 0, 4); // the algorith defnitly only state 4 - but our thing this working poorly - also x 2 is defintely not how to do it?
		
		
		date.minutes = select_bit_range_from_word(time, 5, 10);
		
		
		date.hours = select_bit_range_from_word(time, 11, 15);
		
		
		date.day = select_bit_range_from_word(calendar_date, 0, 4);
		
		
		date.month = select_bit_range_from_word(calendar_date, 5, 8);
		
		
		int fat_year = select_bit_range_from_word(calendar_date, 9, 15);
		int real_year = 1980;
		
		while (fat_year-- > 0)
		{
			real_year++;
		}
		
		date.year = real_year;
		
		
		return date;
	}




	void print_directory_entry(directory_entry_t directory_entry)
	{
		if (directory_entry.short_name == "")
		{
			//		printf("Empty entry\n");
			return;
		}
		
		
		if (directory_entry.short_name[0] == '?')
		{
			printf("Previously erased entry\n");
		}
		
		
		char short_extension_string[4];
		short_extension_string[0] = directory_entry.short_extension[0];
		short_extension_string[1] = directory_entry.short_extension[1];
		short_extension_string[2] = directory_entry.short_extension[2];
		short_extension_string[3] = '\0';
		printf("Name: %s.%s\n", directory_entry.short_name.c_str(), short_extension_string);
		
		
		printf("File Attributes: ");
		if ((directory_entry.attributes & 0x01)) // >> 5) & 0x01
		{
			printf("readon only");
		}
		if ((directory_entry.attributes & 0x02)) // >> 5) & 0x01
		{
			printf("hidden ");
		}
		if ((directory_entry.attributes & 0x04)) // >> 5) & 0x01
		{
			printf("system ");
		}
		if ((directory_entry.attributes & 0x08)) // >> 5) & 0x01
		{
			printf("volume label ");
		}
		if ((directory_entry.attributes & 0x10)) // >> 5) & 0x01
		{
			printf("subdir ");
		}
		if ((directory_entry.attributes & 0x20)) // >> 5) & 0x01
		{
			printf("archive ");
		}
		if ((directory_entry.attributes & 0x40)) // >> 5) & 0x01
		{
			printf("device ");
		}
		if ((directory_entry.attributes & 0x80)) // >> 5) & 0x01
		{
			printf("reserved ");
		}
		printf("\n");
		
		
		timestamp_t create_date = raw_to_timestamp(directory_entry.create_time_ms, directory_entry.create_time_hr_m_s, directory_entry.create_date);
		
		// WE MAY JUST BE ABLE TO PRINT 000 FOR NONEXISTENT MILLISECONDS
		if (directory_entry.short_name[0] != '?')
		{
			// separating these into two separate functions is 10,000% superfluous
			string create_date_string = full_timestamp_to_str(create_date);
			printf("Create time: %s\n", create_date_string.c_str());
		}
		else
		{
			// I could have just called tom::string_with_format directly. Why didn't? Just in case more processing needed to be done - which it didn't
			string create_date_string = partial_timestamp_to_str(create_date);
			printf("Create time: %s\n", create_date_string.c_str());
		}
		
		
		timestamp_t access_date = raw_to_timestamp(0, 0, directory_entry.access_date);
		
		printf("Access date: %d/%02d/%02d\n", access_date.year, access_date.month, access_date.day);
		
		
		printf("Extended attributes:");
		if (directory_entry.extended_attributes != 0)
		{
			if (directory_entry.extended_attributes & 0x0001)
			{
				printf(" owner delete");
			}
			// and so on - not relevant for this test
		}
		else
		{
			printf(" 0");
		}
		printf("\n");
		
		
		timestamp_t modify_date = raw_to_timestamp(0, directory_entry.last_modified_time_hr_m_s, directory_entry.last_modified_date);
		
		string modify_date_string = full_timestamp_to_str(modify_date);
		
		printf("Modify time: %s\n", modify_date_string.c_str());
		
		
		printf("Start cluster: %d\n", directory_entry.start_location);
		
		
		printf("Bytes: %d\n", directory_entry.size);
	}




	boot_sector_info_t get_boot_sector_info(void* mapped_image)
	{
		boot_sector_info_t boot_sector_info;
		
		char raw_oem[9];
		read(&raw_oem, mapped_image, 0x003, 8);  //might work?
		raw_oem[8] = '\0';
		boot_sector_info.oem = raw_oem;
		
		read(&boot_sector_info.reserved_sectors_count, mapped_image, 0x00E, 2);
		
		read(&boot_sector_info.bytes_per_sector, mapped_image, 0x00B, 2);
		
		read(&boot_sector_info.sectors_per_cluster, mapped_image, 0x00D, 1);
		
		read(&boot_sector_info.reserved_sectors_count, mapped_image, 0x00E, 2);
		
		read(&boot_sector_info.fat_count, mapped_image, 0x010, 1);
		
		read(&boot_sector_info.max_root_directory_entry_count, mapped_image, 0x011, 2);
		
		read(&boot_sector_info.logical_sector_count_word, mapped_image, 0x013, 2);
		
		if (boot_sector_info.logical_sector_count_word == 0)
		{
			read(&boot_sector_info.logical_sector_count_dword, mapped_image, 0x020, 4);
		}
		
		read(&boot_sector_info.media_descriptor, mapped_image, 0x015, 1);
		
		read(&boot_sector_info.sectors_per_fat_word, mapped_image, 0x016, 2);
		
		if (boot_sector_info.sectors_per_fat_word == 0)
		{
			read(&boot_sector_info.sectors_per_fat_dword, mapped_image, 0x024, 4);
		}
		
		
		return boot_sector_info;
	}




	directory_entry_t get_directory_entry_at_offset(int offset, void* base)
	{
		directory_entry_t directory_entry;
		
		
		char raw_filename[9];
		read(&raw_filename, base, offset + 0x00, 8);
		raw_filename[8] = '\0';
		
		if (*raw_filename == '\0' || ((unsigned char)(*raw_filename)) == 0x2E || ((unsigned char)(*raw_filename)) == 0xE5 || ((unsigned char)(*raw_filename)) == 0x05)
		{
			memset(&directory_entry, 0, sizeof(directory_entry_t));
			
			directory_entry.short_name = "NULL";
			
			
			return directory_entry;
		}
		
		directory_entry.short_name = raw_filename;
		
		// could/should? do this sooner, but comparing strings is nicer than comparing c strings
		if (directory_entry.short_name == "")
		{
			memset(&directory_entry, 0, sizeof(directory_entry_t));
			
			directory_entry.short_name = "NULL";
			
			
			return directory_entry;
		}
		
		
		read(&directory_entry.short_extension, base, offset + 0x08, 3);
		
		
		// More info can be grabbed from this value when actually interpreting - no need to do that here?
		read(&directory_entry.attributes, base, offset + 0x0B, 1);
		
		
		if (directory_entry.short_name[0] == '?')
		{
			read(&directory_entry.first_character_of_deleted_file, base, offset + 0x0D, 1);
		}
		else
		{
			read(&directory_entry.create_time_ms, base, offset + 0x0D, 1);
		}
		
		
		read(&directory_entry.create_time_hr_m_s, base, offset + 0x0E, 2);
		
		
		read(&directory_entry.create_date, base, offset + 0x10, 2);
		
		
		read(&directory_entry.access_date, base, offset + 0x12, 2);
		
		
		read(&directory_entry.extended_attributes, base, offset + 0x14, 2);
		
		
		read(&directory_entry.last_modified_time_hr_m_s, base, offset + 0x16, 2);
		
		
		read(&directory_entry.last_modified_date, base, offset + 0x18, 2);
		
		
		read(&directory_entry.start_location, base, offset + 0x1A, 2);
		
		
		read(&directory_entry.size, base, offset + 0x1C, 4);
		
		
		return directory_entry;
	}




	directory_entry_t find_directory_entry_by_name(string filename, void* base)
	{
		directory_entry_t found_entry;
		
		boot_sector_info_t boot_sector_info;
		unsigned int number_of_reserved_sectors = 0;
		unsigned int number_of_fats = 0;
		unsigned int sectors_per_fat = 0;
		unsigned int bytes_per_sector = 0;
		unsigned int root_directory_offset;
		
		
		boot_sector_info = get_boot_sector_info(base);
		
		number_of_reserved_sectors = boot_sector_info.reserved_sectors_count;
		
		number_of_fats = boot_sector_info.fat_count;
		
		if (boot_sector_info.sectors_per_fat_word == 0)
		{
			sectors_per_fat = boot_sector_info.sectors_per_fat_dword;
		}
		else
		{
			sectors_per_fat = boot_sector_info.sectors_per_fat_word;
		}
		
		bytes_per_sector = boot_sector_info.bytes_per_sector;
		
		root_directory_offset = ((sectors_per_fat * number_of_fats) * bytes_per_sector) + (number_of_reserved_sectors * bytes_per_sector);
		
		
		int base_directory_offset = root_directory_offset; // start looking in root
		int next_cluster = 0;
		
		int path_depth = count_path_components(filename);
		
		//	jmp_buf next_path_component; // since it's local, goto may be easier
		for (int i = 0; i < path_depth; i++)
		{
			//		setjmp(next_path_component);
			string component = get_path_component(filename, i);
			
			if (component.back() == '/')
			{
				component.back() = '\0';
			}
			
			if (component.front() == '/')
			{
				component.erase(component.begin());
			}
			
			// possibly wrap in if i == 0
			if (i == 0)
			{
				for (int offset = 0; (offset / 32) < boot_sector_info.max_root_directory_entry_count; offset += 32)
				{
					char raw_filename[9];
					read(&raw_filename, base, base_directory_offset + offset, 8);
					raw_filename[8] = '\0';
					
					char compare_filename[9];
					strcpy(compare_filename, raw_filename);
					for (int j = 0; j < 8; j++)
					{
						if (compare_filename[j] == ' ')
						{
							compare_filename[j] = '\0';
							break;
						}
					}
					
					char raw_extension[4];
					read(&raw_extension, base, base_directory_offset + offset + 0x08, 3);
					raw_extension[3] = '\0';
					
					char compare_fileextension[5];
					if (strcmp(raw_extension, "   ") == 0)
					{
						compare_fileextension[0] = '\0';
					}
					else
					{
						compare_fileextension[0] = '.';
						compare_fileextension[1] = raw_extension[0];
						compare_fileextension[2] = raw_extension[1];
						compare_fileextension[3] = raw_extension[2];
						compare_fileextension[4] = raw_extension[3];
						for (int j = 0; j < 4; j++)
						{
							if (compare_fileextension[j] == ' ')
							{
								compare_fileextension[j] = '\0';
								break;
							}
						}
					}
					
					string entry_filename = cstrapp(compare_filename, compare_fileextension);
					
					//								lprintf("Comparing '%s' to '%s'", component.c_str(), entry_filename.c_str());
					
					// we may want to use a setjmp
					if (strcmp(entry_filename.c_str(), component.c_str()) == 0) // redundant? yes. Do I care? no.
					{
						//					lprintf("FOUND: %s", entry_filename.c_str());
						// More info can be grabbed from this value when actually interpreting - no need to do that here?
						read(&found_entry.attributes, base, base_directory_offset + offset + 0x0B, 1);
						
						if ((found_entry.attributes & 0x10)) // >> 5) & 0x01
						{
							//						lprintf("   ITS A FOLDER");
							read(&found_entry.start_location, base, base_directory_offset + offset + 0x1A, 2);
							next_cluster = found_entry.start_location;
							//update base directory offset
							goto next_path_component; //longjmp(next_path_component, 0);
						}
						else
						{
							found_entry.short_name = raw_filename; // no need to check for empty string
							
							read(&found_entry.short_extension, base, base_directory_offset + offset + 0x08, 3);
							
							// More info can be grabbed from this value when actually interpreting - no need to do that here?
							read(&found_entry.attributes, base, base_directory_offset + offset + 0x0B, 1);
							
							if (found_entry.short_name[0] == '?')
							{
								read(&found_entry.first_character_of_deleted_file, base, base_directory_offset + offset + 0x0D, 1);
							}
							else
							{
								read(&found_entry.create_time_ms, base, base_directory_offset + offset + 0x0D, 1);
							}
							
							
							read(&found_entry.create_time_hr_m_s, base, base_directory_offset + offset + 0x0E, 2);
							
							
							read(&found_entry.create_date, base, base_directory_offset + offset + 0x10, 2);
							
							
							read(&found_entry.access_date, base, base_directory_offset + offset + 0x12, 2);
							
							
							read(&found_entry.extended_attributes, base, base_directory_offset + offset + 0x14, 2);
							
							
							read(&found_entry.last_modified_time_hr_m_s, base, base_directory_offset + offset + 0x16, 2);
							
							
							read(&found_entry.last_modified_date, base, base_directory_offset + offset + 0x18, 2);
							
							
							read(&found_entry.start_location, base, base_directory_offset + offset + 0x1A, 2);
							
							
							read(&found_entry.size, base, base_directory_offset + offset + 0x1C, 4);
						}
					}
				}
			}
			else
			{
				unsigned int fat_offset = (boot_sector_info.reserved_sectors_count * boot_sector_info.bytes_per_sector);// + 2; // + 2 factored into cluster number
				
				base_directory_offset = fat_offset + (boot_sector_info.max_root_directory_entry_count * 32) + (next_cluster * boot_sector_info.sectors_per_cluster * boot_sector_info.bytes_per_sector); //data_region_offset + ((next_cluster - 1)? * sectors_per_cluster * bytes_per_sector)
				
				while (!is_fat_eof(next_cluster) && next_cluster != 0)
				{
					for (int offset = 0; (offset / 32) < (boot_sector_info.sectors_per_cluster * boot_sector_info.bytes_per_sector); offset += 32)
					{
						char raw_filename[9];
						read(&raw_filename, base, base_directory_offset + offset, 8);
						raw_filename[8] = '\0';
						
						char compare_filename[9];
						strcpy(compare_filename, raw_filename);
						for (int j = 0; j < 8; j++)
						{
							if (compare_filename[j] == ' ')
							{
								compare_filename[j] = '\0';
								break;
							}
						}
						
						char raw_extension[4];
						read(&raw_extension, base, base_directory_offset + offset + 0x08, 3);
						raw_extension[3] = '\0';
						
						char compare_fileextension[5];
						if (strcmp(raw_extension, "   ") == 0)
						{
							compare_fileextension[0] = '\0';
						}
						else
						{
							compare_fileextension[0] = '.';
							compare_fileextension[1] = raw_extension[0];
							compare_fileextension[2] = raw_extension[1];
							compare_fileextension[3] = raw_extension[2];
							compare_fileextension[4] = raw_extension[3];
							for (int j = 0; j < 4; j++)
							{
								if (compare_fileextension[j] == ' ')
								{
									compare_fileextension[j] = '\0';
									break;
								}
							}
						}
						
						string entry_filename = cstrapp(compare_filename, compare_fileextension);
						
						//										lprintf("Comparing '%s' to '%s'", component.c_str(), entry_filename.c_str());
						
						if (strcmp(entry_filename.c_str(), component.c_str()) == 0) // redundant? yes. Do I care? no.
						{
							//						lprintf("FOUND: %s", entry_filename.c_str());
							// More info can be grabbed from this value when actually interpreting - no need to do that here?
							read(&found_entry.attributes, base, base_directory_offset + offset + 0x0B, 1);
							
							if ((found_entry.attributes & 0x10)) // >> 5) & 0x01
							{
								//							lprintf("   ITS A FOLDER");
								read(&found_entry.start_location, base, base_directory_offset + offset + 0x1A, 2);
								next_cluster = found_entry.start_location;
								//update base directory offset
								goto next_path_component; //longjmp(next_path_component, 0);
							}
							else
							{
								found_entry.short_name = raw_filename; // no need to check for empty string
								
								read(&found_entry.short_extension, base, base_directory_offset + offset + 0x08, 3);
								
								// More info can be grabbed from this value when actually interpreting - no need to do that here?
								read(&found_entry.attributes, base, base_directory_offset + offset + 0x0B, 1);
								
								if (found_entry.short_name[0] == '?')
								{
									read(&found_entry.first_character_of_deleted_file, base, base_directory_offset + offset + 0x0D, 1);
								}
								else
								{
									read(&found_entry.create_time_ms, base, base_directory_offset + offset + 0x0D, 1);
								}
								
								
								read(&found_entry.create_time_hr_m_s, base, base_directory_offset + offset + 0x0E, 2);
								
								
								read(&found_entry.create_date, base, base_directory_offset + offset + 0x10, 2);
								
								
								read(&found_entry.access_date, base, base_directory_offset + offset + 0x12, 2);
								
								
								read(&found_entry.extended_attributes, base, base_directory_offset + offset + 0x14, 2);
								
								
								read(&found_entry.last_modified_time_hr_m_s, base, base_directory_offset + offset + 0x16, 2);
								
								
								read(&found_entry.last_modified_date, base, base_directory_offset + offset + 0x18, 2);
								
								
								read(&found_entry.start_location, base, base_directory_offset + offset + 0x1A, 2);
								
								
								read(&found_entry.size, base, base_directory_offset + offset + 0x1C, 4);
							}
						}
					}
					
					read(&next_cluster, base, fat_offset + (next_cluster * 2), 2);
				}
				
			}
			
		next_path_component: // just a place at the end of the loop, so we don't manualy have to increment and compare X
			NOOP(i); //just statement ot silence errors
		}
		
		
		return found_entry;
	}




	void print_boot_sector_info(boot_sector_info_t boot_sector_info)
	{
		printf("OEM: %s\n", boot_sector_info.oem.c_str());
		
		printf("Bytes per sector: %d\n", boot_sector_info.bytes_per_sector);
		
		printf("Sectors per cluster: %d\n", boot_sector_info.sectors_per_cluster);
		
		printf("Reserved sectors: %d\n", boot_sector_info.reserved_sectors_count);
		
		printf("Num FATs: %d\n", boot_sector_info.fat_count);
		
		printf("Max root directory entries: %d\n", boot_sector_info.max_root_directory_entry_count);
		
		if (boot_sector_info.logical_sector_count_word != 0)
		{
			printf("Num logical sectors: %d\n", boot_sector_info.logical_sector_count_word);
		}
		else
		{
			printf("Num logical sectors: %d\n", boot_sector_info.logical_sector_count_dword);
		}
		
		printf("Media Descriptor: %x\n", boot_sector_info.media_descriptor);
		
		if (boot_sector_info.sectors_per_fat_word != 0)
		{
			printf("Sectors per FAT: %d\n", boot_sector_info.sectors_per_fat_word);
		}
		else
		{
			printf("Sectors per FAT: %d\n", boot_sector_info.sectors_per_fat_dword);
		}
	}
}
