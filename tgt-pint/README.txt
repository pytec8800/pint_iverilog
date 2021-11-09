/* 
github original version: master
commit 359b2b65c2f015191ec05109d82e91ec22569a9b
Author: Martin Whitaker <icarus@martin-whitaker.me.uk>
Date:   Fri Oct 9 11:38:16 2020 +0100

    Support escaped identifiers as macro names.

*/

install:

cd pint_iverilog
mkdir install
sh autoconf.sh
./configure --prefix=/path/to/pint_iverilog/install
make -j8
make install

test:

/path/to/pint_iverilog/install/bin/iverilog -o pintsim xxx.v -t pint
./pintsim


