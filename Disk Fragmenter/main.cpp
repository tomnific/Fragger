//
//  main.cpp
//  Disk Fragmenter
//
//  Created by Tom Metzger on 5/7/19.
//  Copyright © 2019 Tom. All rights reserved.
//

#include <stdtom/stdtom.hpp>
#include <fatutils/fatutils.hpp>

#include <unistd.h>
#include <sysexits.h>

#include "./fragmenter.hpp"





using namespace tom;
using namespace fat;
using namespace std;




string image_path = "";
WORD* FAT;




void virtualize_fat(void* base, disk_info_t disk_info)
{
	lprintf("Virtualizing FAT...");
	
	
	long total_cluster_count = (disk_info.region_sizes.fat_region / 2) / 2; // each FAT entry is 2 bytes and there are 2 FATs
	
	
	lprintf("   Initializing FAT...");
	
	FAT = (WORD*)malloc(total_cluster_count);
	memset(FAT, 0, total_cluster_count);
	
	lprintf("   Done.");
	

	// triple check that this is right
	lprintf("   Reading FAT...");
	
	int cluster_number = 3;
	WORD fat_entry = 3;
	
	read(&fat_entry, base, disk_info.region_offsets.fats[0] + (cluster_number * 2), 2);
	FAT[cluster_number] = fat_entry;
	cluster_number++;

	while (cluster_number <= total_cluster_count)
	{
		read(&fat_entry, base, disk_info.region_offsets.fats[0] + (cluster_number * 2), 2);
		
		FAT[cluster_number] = fat_entry;
		
		cluster_number++;
	}
	
	lprintf("   Done.");
	
	
	lprintf("Done.");
}




void traverse_subdirectory(void* base, disk_info_t* disk_info, directory_entry_t subdirectory)
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
			int current_cluster = *it - 2;
			base_directory_offset = (unsigned int)(disk_info->region_offsets.data_region + (current_cluster * disk_info->region_sizes.cluster_size));
			
			for (int offset = 0; offset < disk_info->region_sizes.cluster_size; offset += 32)
			{
				directory_entry_t entry = get_directory_entry_at_offset(base_directory_offset + offset, base);
				
				if (entry.start_location >= disk_info->capacity)//63967) // definitely not "right" to hardcode this - but its the best I can do for now...
				{
					return;
				}
				
				if (entry.short_name != "NULL")
				{
					if ((entry.attributes & 0x10))// && entry.size == 0 && !(entry.extended_attributes & 0x0001))
					{
						disk_info->directory_count++;
						list<int> clusers_used = file_clusters_for_entry_at(entry.start_location, base);
						disk_info->allocated_space += ((clusers_used.size() - 1) * disk_info->region_sizes.cluster_size);
						traverse_subdirectory(base, disk_info, entry); // yay recursion
					}
					else
					{
						disk_info->file_count++;
						list<int> clusers_used = file_clusters_for_entry_at(entry.start_location, base);
						disk_info->allocated_space += ((clusers_used.size() - 1) * disk_info->region_sizes.cluster_size);
						disk_info->space_used_by_files += entry.size;
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




disk_info_t initialize_fragmenter(void* base)
{
	disk_info_t disk_info;
	boot_sector_info_t boot_sector_info;
	
	unsigned int sectors_per_fat;
	
	
	lprintf("Configuring...");
	lprintf("   Reading Boot Sector...");
	
	
	// make sure there's no chance of odd start values
	memset(&disk_info, 0, sizeof(disk_info_t));
	
	
	boot_sector_info = get_boot_sector_info(base);
	
	
	if (boot_sector_info.sectors_per_fat_word == 0)
	{
		sectors_per_fat = boot_sector_info.sectors_per_fat_dword;
	}
	else
	{
		sectors_per_fat = boot_sector_info.sectors_per_fat_word;
	}
	
	
	disk_info.region_sizes.cluster_size = boot_sector_info.sectors_per_cluster * boot_sector_info.bytes_per_sector;
	
	disk_info.region_sizes.reserved_sectors = boot_sector_info.reserved_sectors_count * boot_sector_info.bytes_per_sector;
//	disk_info.region_sizes.boot_sector = 0xDEAD;
	
	disk_info.region_sizes.single_fat = sectors_per_fat * boot_sector_info.bytes_per_sector;
	
	disk_info.region_sizes.fat_region = boot_sector_info.fat_count * sectors_per_fat * boot_sector_info.bytes_per_sector;
	
	disk_info.region_sizes.root_directory_region = boot_sector_info.max_root_directory_entry_count * 32;
	
	disk_info.region_sizes.data_region = 0xDEAD; //fix sometime
	
	
	disk_info.region_offsets.boot_sector = 0x00;
	
	disk_info.region_offsets.fats[0] = boot_sector_info.reserved_sectors_count * boot_sector_info.bytes_per_sector;
	disk_info.region_offsets.fats[1] = disk_info.region_offsets.fats[0] + disk_info.region_sizes.single_fat;
	
	disk_info.region_offsets.root_directory = (disk_info.region_sizes.single_fat * boot_sector_info.fat_count) + (boot_sector_info.reserved_sectors_count * boot_sector_info.bytes_per_sector);
	
	disk_info.region_offsets.data_region = disk_info.region_offsets.fats[1] + disk_info.region_sizes.single_fat + disk_info.region_sizes.root_directory_region;
	
	
	if (boot_sector_info.logical_sector_count_word == 0)
	{
		disk_info.capacity = boot_sector_info.logical_sector_count_dword * boot_sector_info.bytes_per_sector;
	}
	else
	{
		disk_info.capacity = boot_sector_info.logical_sector_count_word * boot_sector_info.bytes_per_sector;
	}
	
	lprintf("   Done.");
	lprintf("   Sanning filesysem...");
	
	for (int i = 0; i < boot_sector_info.max_root_directory_entry_count; i++)
	{
		unsigned int directory_entry_offset = (unsigned int)(disk_info.region_offsets.root_directory + (32 * i));
		
		directory_entry_t root_entry = get_directory_entry_at_offset(directory_entry_offset, base);
		
		if (root_entry.short_name != "NULL")
		{
			if ((root_entry.attributes & 0x10))
			{
				disk_info.directory_count++;
				list<int> clusers_used = file_clusters_for_entry_at(root_entry.start_location, base);
				disk_info.allocated_space += ((clusers_used.size() - 1) * disk_info.region_sizes.cluster_size);
				traverse_subdirectory(base, &disk_info, root_entry);
			}
			else
			{
				disk_info.file_count++;
				list<int> clusers_used = file_clusters_for_entry_at(root_entry.start_location, base);
				disk_info.allocated_space += ((clusers_used.size() - 1) * disk_info.region_sizes.cluster_size);
				disk_info.space_used_by_files += root_entry.size;
			}
		}
	}
	
	lprintf("   Done.");
	lprintf("   Finishing...");
	
	//right?
	disk_info.used_space = disk_info.region_offsets.root_directory + disk_info.space_used_by_files;
	
	disk_info.free_space = disk_info.capacity - disk_info.used_space;
	
	disk_info.unused_allocated_space = disk_info.allocated_space - disk_info.space_used_by_files;
	
	disk_info.unallocated_space = disk_info.capacity - disk_info.allocated_space;
	
	if (disk_info.unallocated_space <= 0)
	{
		disk_info.is_full = true;
	}
	else
	{
		disk_info.is_full = false;
	}
	
	
	lprintf("   Done.");
	lprintf("Done.");
	
	
	return disk_info;
}




//./fragger --image <image>
// TODO: make verbos optional
int main(int argc, char * argv[])
{
	set_lprefix("[fragger] ");
	
	
#ifdef __XCODE__
	if (argc == 1)
	{
		image_path = "/Users/Tom/Garage/School/Projects/Software Development/Cross Platform/C++/OS/Disk Fragmenter/tests/vfs-hidden";
#else
	if (argc == 3)
	{
		if (arg_is_present(argc, argv, "--image"))
		{
			image_path = getarg(argc, argv, "--image");
		}
#endif
		
		
		if (access(image_path.c_str(), F_OK) != 0)
		{
			lprintf("ERROR: Invalid image path.");
			exit(EX_IOERR);
		}
		
		
		void* mappd_image = map_image_to_memory(image_path);
		
		
		disk_info_t disk_info = initialize_fragmenter(mappd_image);
		
		
		virtualize_fat(mappd_image, disk_info);
		
		
		fragment_disk(mappd_image, disk_info, FAT);
	}
	else
	{
		lprintf("Usage: ./fragger --image <image-path>");
		exit(EX_USAGE);
	}
	
	
	return 0;
}
