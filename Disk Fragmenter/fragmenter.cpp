//
//  fragmenter.cpp
//  Disk Fragmenter
//
//  Created by Tom Metzger on 5/8/19.
//  Copyright © 2019 Tom. All rights reserved.
//

#include "fragmenter.hpp"

#include <cstdlib>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>





jmp_buf ammend_entry;





void overwrite_fat(void* base, WORD* FAT, disk_info_t disk_info)
{
	lprintf("Overwriting FATs...");
	write(FAT, base, disk_info.region_offsets.fats[0], (unsigned int)disk_info.region_sizes.single_fat);
	write(FAT, base, disk_info.region_offsets.fats[1], (unsigned int)disk_info.region_sizes.single_fat);
	lprintf("Done.");
}




int index_for_cluster(WORD* FAT, unsigned long max_valid_fat_entry, WORD cluster)
{
	if (cluster < 2)
	{
		return -1;
	}
	
	
	for (int i = 2; i < max_valid_fat_entry; i++)
	{
		if (FAT[i] == cluster)
		{
			return i;
		}
	}
	
	
	return -2;
}




bool cluster_is_start_cluster(WORD* FAT, unsigned long max_valid_fat_entry, WORD cluster)
{
	// technically, yes, these are lies. BUT it works
	if (cluster < 2)
	{
		return true;
	}
	
	if (FAT[cluster] == 0)
	{
		return false;
	}
	
	
	
	bool is_start_cluster = true;
	
	for (int i = 2; i < max_valid_fat_entry; i++)
	{
		if (FAT[i] == cluster)
		{
			is_start_cluster = false;
			break;
		}
	}
	
	
	return is_start_cluster;
}




unsigned int high_random_fat_index(unsigned long max_valid_fat_entry)
{
	unsigned long start = ((max_valid_fat_entry) / 2);
	unsigned long end = (max_valid_fat_entry);
	
	
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	
	srand(tv.tv_usec);
	
	
	unsigned int random = (unsigned int)((rand() % end)+start);
	
	while (random < start || random > end)
	{
		random = (unsigned int)((rand() % end)+start);
	}
	
	
	return random;
}




unsigned int low_random_fat_index(unsigned long max_valid_fat_entry)
{
	unsigned long start = 2;
	unsigned long end = ((max_valid_fat_entry) / 2);
	
	
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	
	srand(tv.tv_usec);
	
	
	unsigned int random = (unsigned int)((rand() % end)+start);
	
	while (random > end)
	{
		random = (unsigned int)((rand() % end)+start);
	}
	
	
	return random;
}




void subdir_check_and_swap_start_cluster(void* base, disk_info_t disk_info, directory_entry_t subdirectory, unsigned long target_cluster, unsigned long replacement)
{
	unsigned int base_directory_offset;
	list<int> clusters;
	
	
	clusters = file_clusters_for_entry_at(subdirectory.start_location, base);
	
	
	list<int> :: iterator it;
	for (it = clusters.begin(); it != clusters.end(); ++it)
	{
		if (*it == -1)
		{
			break;
		}
		else
		{
			int current_actual_cluster = *it - 2;
			base_directory_offset = (unsigned int)(disk_info.region_offsets.data_region + (current_actual_cluster * disk_info.region_sizes.cluster_size));
			
			for (int offset = 0; offset < disk_info.region_sizes.cluster_size; offset += 32)
			{
				directory_entry_t entry = get_directory_entry_at_offset(base_directory_offset + offset, base);
				
				long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
				long fat_index_limit = data_region_cluster_count + 2;
				if (entry.start_location >= fat_index_limit)//63967) // definitely not "right" to hardcode this - but its the best I can do for now...
				{
					return;
				}
				
				if (entry.short_name != "NULL")
				{
					if ((entry.attributes & 0x10))
					{
						subdir_check_and_swap_start_cluster(base, disk_info, entry, target_cluster, replacement);
					}
					else
					{
						if (entry.start_location == target_cluster)
						{
							lprintf("START CLUSTER! Found and replacing...");
							memcpy(static_cast<char *>(base) + (base_directory_offset + offset) + 0x1A, &replacement, 2);
							longjmp(ammend_entry, 1);
						}
					}
				}
			}
		}
	}
}




void root_check_and_swap_start_cluster(void* base, disk_info_t disk_info, unsigned long target_cluster, unsigned long replacement)
{
	boot_sector_info_t boot_sector_info = get_boot_sector_info(base);
	
	for (int i = 0; i < boot_sector_info.max_root_directory_entry_count; i++)
	{
		unsigned int directory_entry_offset = (unsigned int)(disk_info.region_offsets.root_directory + (32 * i));
		//
		directory_entry_t root_entry = get_directory_entry_at_offset(directory_entry_offset, base);
		//
		//		print_directory_entry(root_entry);
		if (root_entry.short_name != "NULL")
		{
			if ((root_entry.attributes & 0x10))
			{
				subdir_check_and_swap_start_cluster(base, disk_info, root_entry, target_cluster, replacement);
			}
			else
			{
				if (root_entry.start_location == target_cluster)
				{
					lprintf("START CLUSTER! Found and replacing...");
					memcpy(static_cast<char *>(base) + directory_entry_offset + 0x1A, &replacement, 2);
					longjmp(ammend_entry, 1);
				}
			}
		}
	}
}




void swap_one_cluster(void* base, WORD* FAT, disk_info_t disk_info, directory_entry_t file, unsigned long file_offset)
{
	lprintf("      Moving cluster...");
	//			lprintf("FAT DUMP:");
	//			for (int i = 0; i < fat_index_limit; i++)
	//			{
	//				printf("%d -> %d\n", i, FAT[i]);
	//			}
	long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
	long fat_index_limit = data_region_cluster_count + 2;
	
	list<int> clusters = file_clusters_for_entry_at(file.start_location, base);
	
	auto it = clusters.rbegin();
	it++;
	it++; //this has actual content now?
	
	int current_cluster_index = *it;
	int current_actual_cluster = *it - 2;
	long cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + (current_actual_cluster * disk_info.region_sizes.cluster_size);
	
	WORD destination_cluster_index = 0;
	
	bool first_iteration = true;
	while (first_iteration || cluster_is_start_cluster(FAT, fat_index_limit, destination_cluster_index))
	{
		first_iteration = false;
		destination_cluster_index = (WORD)high_random_fat_index(fat_index_limit);
	}
	
	lprintf("Swapping %d with %d", current_cluster_index, destination_cluster_index);

	lprintf("PRE-SWAP RELEVANT FAT DUMP:");
	lprintf("%d -> %d", *next(it), FAT[*next(it)]);
	lprintf("%d -> %d", current_cluster_index, FAT[current_cluster_index]);
	lprintf("%d -> %d", index_for_cluster(FAT, fat_index_limit, destination_cluster_index), FAT[index_for_cluster(FAT, fat_index_limit, destination_cluster_index)]);
	lprintf("%d -> %d", destination_cluster_index, FAT[destination_cluster_index]);
	
	long destination_cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + ((destination_cluster_index - 2) * disk_info.region_sizes.cluster_size);
	
	unsigned char cluster_data[disk_info.region_sizes.cluster_size];
	memset(cluster_data, 0, disk_info.region_sizes.cluster_size);
	read(&cluster_data, base, cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
	
	unsigned char destination_cluster_data[disk_info.region_sizes.cluster_size];
	memset(destination_cluster_data, 0, disk_info.region_sizes.cluster_size);
	read(&destination_cluster_data, base, destination_cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
	
	// might be unnecessary
	memset(static_cast<char *>(base) + destination_cluster_offset, 0, disk_info.region_sizes.cluster_size);
	memset(static_cast<char *>(base) + cluster_offset, 0, disk_info.region_sizes.cluster_size);
	
	write(&cluster_data, base, destination_cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
	write(&destination_cluster_data, base, cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
	
	
	int backreference_of_destination_cluster = index_for_cluster(FAT, fat_index_limit, destination_cluster_index);
	if (backreference_of_destination_cluster < 0 && FAT[destination_cluster_index] != 0)
	{
		raise(SIGTRAP);
	}
	
	// swap contents
	WORD swap = FAT[destination_cluster_index];
	FAT[destination_cluster_index] = FAT[current_cluster_index];
	FAT[current_cluster_index] = swap;
	
	// swap backreferences
	FAT[backreference_of_destination_cluster] = current_cluster_index;
	FAT[*next(it)] = destination_cluster_index;
	
	
	lprintf("POST-SWAP RELEVANT FAT DUMP:");
	lprintf("%d -> %d", *next(it), FAT[*next(it)]);
	lprintf("%d -> %d", destination_cluster_index, FAT[destination_cluster_index]);
	lprintf("%d -> %d", index_for_cluster(FAT, fat_index_limit, current_cluster_index), FAT[index_for_cluster(FAT, fat_index_limit, current_cluster_index)]);
	lprintf("%d -> %d", current_cluster_index, FAT[current_cluster_index]);
}




void fragment_file_cluster_swap(void* base, WORD* FAT, disk_info_t disk_info, directory_entry_t file, unsigned long file_offset, int forbidden_cluster)
{
	list<int> clusters;
	
	// move to disk_info_t
	long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
	long fat_index_limit = data_region_cluster_count + 2;
	
	
	lprintf("   Fragmenting file...");
	
	
	clusters = file_clusters_for_entry_at(file.start_location, base);
	
	
	if (*clusters.begin() == -1)
	{
		lprintf("      Empty file");
		lprintf("   Done.");
		return;
	}
	
	
	WORD most_recent_cluster_index = 0;
	int iteration = 1;
	for (auto it = clusters.rbegin(); it != clusters.rend(); it++)
	{
		if (*it == -1) //might break right away
		{
			// actually - I think we'd want to write an EOF here?
			// no, we don't..
			continue;
		}
		else
		{
			lprintf("      Moving cluster...");
//			lprintf("FAT DUMP:");
//			for (int i = 0; i < fat_index_limit; i++)
//			{
//				printf("%d -> %d\n", i, FAT[i]);
//			}
			if (*it == 2)
			{
#ifdef DEBUG
				raise(SIGTRAP);
#endif
			}
			int current_cluster_index = *it;
			int current_actual_cluster = *it - 2;
			long cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + (current_actual_cluster * disk_info.region_sizes.cluster_size);
			
			WORD destination_cluster_index = 0;
			
			bool first_iteration = true;
																								//                                                                  this last one is overkill. but, better safe than sorry
//			while (first_iteration || is_fat_eof(destination_cluster_index) || cluster_is_start_cluster(FAT, fat_index_limit, destination_cluster_index) || destination_cluster_index == forbidden_cluster || FAT[destination_cluster_index] == forbidden_cluster)
			// try this tomorrow, tom
			while (first_iteration || destination_cluster_index == forbidden_cluster || FAT[destination_cluster_index] == forbidden_cluster || cluster_is_start_cluster(FAT, fat_index_limit, destination_cluster_index))
			{
				first_iteration = false;
				
				if (iteration % 5 == 0) //optimization: assume start clusters are low in addresses. Also, increase iterator each check so we switch search section with each redo
				{
					destination_cluster_index = (WORD)high_random_fat_index(fat_index_limit);
					iteration++;
				}
				else
				{
					destination_cluster_index = (WORD)low_random_fat_index(fat_index_limit);
					iteration++;
				}
			}
			
			if (current_cluster_index == destination_cluster_index)
			{
				lprintf("Interesting...");
				raise(SIGTRAP);
			}
			
			lprintf("Swapping %d with %d", current_cluster_index, destination_cluster_index);
			
			long destination_cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + ((destination_cluster_index - 2) * disk_info.region_sizes.cluster_size);
			
			unsigned char cluster_data[disk_info.region_sizes.cluster_size];
			memset(cluster_data, 0, disk_info.region_sizes.cluster_size);
			read(&cluster_data, base, cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
			
			unsigned char destination_cluster_data[disk_info.region_sizes.cluster_size];
			memset(destination_cluster_data, 0, disk_info.region_sizes.cluster_size);
			read(&destination_cluster_data, base, destination_cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
			
			// might be unnecessary
			memset(static_cast<char *>(base) + destination_cluster_offset, 0, disk_info.region_sizes.cluster_size);
			memset(static_cast<char *>(base) + cluster_offset, 0, disk_info.region_sizes.cluster_size);
			
			write(&cluster_data, base, destination_cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
			write(&destination_cluster_data, base, cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
			
			auto true_beginning = clusters.rend();
			true_beginning--;
			// rend() - 1 didn't work - operator error?
//			if (it != true_beginning && it != clusters.rend())
//			{
				int destination_cluster_backreference_index = index_for_cluster(FAT, fat_index_limit, destination_cluster_index);
				if (destination_cluster_backreference_index < 0 && FAT[destination_cluster_index] != 0)
				{
					raise(SIGTRAP);
				}
				
				// swap contents
				WORD swap = FAT[destination_cluster_index];
				FAT[destination_cluster_index] = FAT[current_cluster_index];
				FAT[current_cluster_index] = swap;

				// swap backreferences
				FAT[destination_cluster_backreference_index] = current_cluster_index;
			
			if (it != true_beginning && it != clusters.rend())
			{
				FAT[*next(it)] = destination_cluster_index;
			}
			if (setjmp(ammend_entry) == 0)
			{
				root_check_and_swap_start_cluster(base, disk_info, current_cluster_index, destination_cluster_index);
			}
//			}
			
			most_recent_cluster_index = destination_cluster_index;
			
			iteration++;
			
//			lprintf("FAT DUMP:");
//			for (int i = 0; i < fat_index_limit; i++)
//			{
//				printf("%d -> %d\n", i, FAT[i]);
//			}
			
//			raise(SIGTRAP);
			
			lprintf("      Done.");
		}
	}
	
	if (most_recent_cluster_index < 2)
	{
		raise(SIGTRAP);
	}
	
	memcpy(static_cast<char *>(base) + file_offset + 0x1A, &most_recent_cluster_index, 2);
	
	lprintf("   Done.");
}




void fragment_file_no_collision(void* base, WORD* FAT, disk_info_t disk_info, directory_entry_t file, unsigned long file_offset, int forbidden_cluster)
{
	/*
	 for each cluster of file
	 	read data
	 	choose random free fat entry
	 	write data
	 	delete old data
	 	overwrite directory ency
	 	update fat entries (old to 0, new to EOF)
	 */
	list<int> clusters;
	
	
	lprintf("   Fragmenting file...");
	
	
	clusters = file_clusters_for_entry_at(file.start_location, base);
	
	
	if (*clusters.begin() == -1)
	{
		lprintf("DEBUG: totally empty entry");
		return;
	}
	
	
	long most_recent_cluster_index = 0;
	int iteration = 1;
	for (auto it = clusters.rbegin(); it != clusters.rend(); it++)
	{
		if (*it == -1) //might break right away
		{
			// actually - I think we'd want to write an EOF here?
			// no, we don't..
			continue;
		}
		else
		{
			lprintf("      Moving cluster...");
			if (*it == 2)
			{
				raise(SIGTRAP);
			}
			int current_cluster_index = *it;
			int current_actual_cluster = *it - 2;
			long cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + (current_actual_cluster * disk_info.region_sizes.cluster_size);
			
			unsigned char cluster_data[disk_info.region_sizes.cluster_size];
			read(&cluster_data, base, cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size); //is & what we want??
			
			WORD destination_cluster_index = 0;
			
			bool first_iteration = true;
			while (first_iteration || (FAT[destination_cluster_index] != 0))
			{
				first_iteration = false;
				
				if (iteration % 2 == 0) // does this make it more random? I think it does...
				{
					// move these so we don't have to calculate each time - maybe even make it part of disk_info_t
					//                                      because the last address is not the start of a cluster
					long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
					long fat_index_limit = data_region_cluster_count + 2;
					destination_cluster_index = (WORD)high_random_fat_index(fat_index_limit);
				}
				else
				{
					long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
					long fat_index_limit = data_region_cluster_count + 2;
					destination_cluster_index = (WORD)low_random_fat_index(fat_index_limit);
				}
			}
			
			long destination_cluster_offset = disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region + ((destination_cluster_index - 2) * disk_info.region_sizes.cluster_size);
			
			write(&cluster_data, base, destination_cluster_offset, (unsigned int)disk_info.region_sizes.cluster_size);
			memset(static_cast<char *>(base) + cluster_offset, 0, disk_info.region_sizes.cluster_size);
			
			if (it != clusters.rend())
			{
				FAT[destination_cluster_index] = FAT[current_cluster_index];
				FAT[*next(it)] = destination_cluster_index;
				FAT[current_cluster_index] = 0;
			}
			
			most_recent_cluster_index = destination_cluster_index;
			
			iteration++;
			
			lprintf("      Done.");
		}
	}
	
//	lprintf("Writing cluster index: %d", (WORD)most_recent_cluster_index);

	if (most_recent_cluster_index < 2)
	{
		raise(SIGTRAP);
	}
	
	memcpy(static_cast<char *>(base) + file_offset + 0x1A, &most_recent_cluster_index, 2);
	
//	directory_entry_t test = get_directory_entry_at_offset((int)file_offset, base);
	
//	print_directory_entry(test);
	
	lprintf("   Done.");
}




void traverse_subdirectory(void* base, WORD* FAT, disk_info_t disk_info, directory_entry_t subdirectory)
{
	unsigned int base_directory_offset;
	list<int> clusters;
	
	
	clusters = file_clusters_for_entry_at(subdirectory.start_location, base);
	
	
	list<int> :: iterator it;
	for (it = clusters.begin(); it != clusters.end(); ++it)
	{
		if (*it == -1)
		{
			break;
		}
		else
		{
			int current_actual_cluster = *it - 2;
			base_directory_offset = (unsigned int)(disk_info.region_offsets.data_region + (current_actual_cluster * disk_info.region_sizes.cluster_size));
			
			for (int offset = 0; offset < disk_info.region_sizes.cluster_size; offset += 32)
			{
				directory_entry_t entry = get_directory_entry_at_offset(base_directory_offset + offset, base);
				
				long data_region_cluster_count = ((disk_info.capacity - disk_info.region_sizes.cluster_size) - (disk_info.region_offsets.root_directory + disk_info.region_sizes.root_directory_region)) / disk_info.region_sizes.cluster_size;
				long fat_index_limit = data_region_cluster_count + 2;
				if (entry.start_location >= fat_index_limit)//63967) // definitely not "right" to hardcode this - but its the best I can do for now...
				{
					return;
				}
				
				if (entry.short_name != "NULL")
				{
					if ((entry.attributes & 0x10))// && entry.size == 0 && !(entry.extended_attributes & 0x0001))
					{
						traverse_subdirectory(base, FAT, disk_info, entry); // yay recursion
					}
					else
					{
						fragment_file_cluster_swap(base, FAT, disk_info, entry, (base_directory_offset + offset), *it);
					}
				}
#ifdef DEBUG
				else
				{
					//					lprintf("ISSUE: Bad directory entry.");
				}
#endif
			}
		}
	}
}




void fragment_disk(void* base, disk_info_t disk_info, WORD* FAT)
{
	lprintf("Fragmenting disk...");
	/*
	 if !disk.full  //close to full?
	    for file on disc
	 		fragment_file(file)
	 else
	 	//fileswapping method
	 */
	
	if (base == NULL)
	{
		raise(SIGTRAP);
	}
	
	// ugghhh, nope we can't escape having to use this
	boot_sector_info_t boot_sector_info = get_boot_sector_info(base);
	
	if (!disk_info.is_full)
	{
		for (int i = 0; i < boot_sector_info.max_root_directory_entry_count; i++)
		{
			unsigned int directory_entry_offset = (unsigned int)(disk_info.region_offsets.root_directory + (32 * i));
//
			directory_entry_t root_entry = get_directory_entry_at_offset(directory_entry_offset, base);
//
//		print_directory_entry(root_entry);
			if (root_entry.short_name != "NULL")
			{
				if ((root_entry.attributes & 0x10))
				{
					traverse_subdirectory(base, FAT, disk_info, root_entry);
				}
				else
				{
					fragment_file_cluster_swap(base, FAT, disk_info, root_entry, (disk_info.region_offsets.root_directory + (32 * i)), -42);
//					swap_one_cluster(base, FAT, disk_info, root_entry, (disk_info.region_offsets.root_directory + (32 * 1)));
		
//		root_entry = get_directory_entry_at_offset(directory_entry_offset, base);
//		print_directory_entry(root_entry);
				}
			}
		}
	}
	else
	{
		lprintf("ERROR: Disk Full");
	}
	
	lprintf("Done.");
	
	
	overwrite_fat(base, FAT, disk_info);
}
