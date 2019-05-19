#!/bin/sh

#  build.sh
#  Disk Fragmenter
#
#  Created by Tom Metzger on 5/7/19.
#  Copyright © 2019 Tom. All rights reserved.


# Because it's easier than a makefile
cd ./Disk\ Fragmenter/

g++ -I./include/ ./include/stdtom/stdtom.cpp ./include/fatutils/fatutils.cpp ./main.cpp -std=c++11 -o ../fragger

cd -
