//
//  fragmenter.hpp
//  Disk Fragmenter
//
//  Created by Tom Metzger on 5/8/19.
//  Copyright © 2019 Tom. All rights reserved.
//

#ifndef fragmenter_hpp
#define fragmenter_hpp

#include <stdtom/stdtom.hpp>
#include <fatutils/fatutils.hpp>





using namespace tom;
using namespace fat;
using namespace std;




typedef struct
{
	unsigned long boot_sector;
	unsigned long fats[2]; // technically, this should be more flexible and based on boot_sector_info.fat_count
	unsigned long root_directory;
	unsigned long data_region;
} region_offsets_t;




typedef struct
{
	unsigned long reserved_sectors;
	unsigned long boot_sector;
	
	unsigned long single_fat;
	unsigned long fat_region;
	
	unsigned long root_directory_region;
	
	unsigned long data_region;
	
	unsigned long cluster_size; // I know - technically not a region, but we want to track this and under sizes makes the most sense
} region_sizes_t;




typedef struct
{
	bool is_full;
	
	region_offsets_t region_offsets;
	region_sizes_t region_sizes;
	
	unsigned long capacity;
	unsigned long used_space;
	unsigned long free_space;
	unsigned long allocated_space;
	long unallocated_space;
	unsigned long unused_allocated_space;
	
	unsigned long space_used_by_files;
	
	unsigned long file_count;
	unsigned long directory_count;
	
//	boot_sector_info_t boot_sector_info;
} disk_info_t;




void fragment_disk(void* base, disk_info_t disk_info, WORD* FAT);

#endif /* fragmenter_hpp */
