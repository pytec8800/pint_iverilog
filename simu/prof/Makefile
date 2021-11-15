#
#    This source code is free software; you can redistribute it
#    and/or modify it in source code form under the terms of the GNU
#    Library General Public License as published by the Free Software
#    Foundation; either version 2 of the License, or (at your option)
#    any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Library General Public License for more details.
#
#    You should have received a copy of the GNU Library General Public
#    License along with this program; if not, write to the Free
#    Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#    Boston, MA 02110-1301, USA.
#
SHELL = /bin/sh

TOP_PATH=$(shell pwd)
HOME_DIRECTORY=$(TOP_PATH)
CC=g++
INCLUDE_OPTION=-I/usr/local/pint/include -I../common/
COMPILE_OPTION=-g -std=c++11 -mcmodel=large -fpermissive 

all: 
	$(CC) $(COMPILE_OPTION) -o $(HOME_DIRECTORY)/pint_simu_prof $(INCLUDE_OPTION) -O2 $(HOME_DIRECTORY)/main.cc -L/usr/local/pint/lib -lpint_api -Wl,--rpath=/usr/local/pint/lib

clean:
	rm -rf $(HOME_DIRECTORY)/pint_simu_stub 
