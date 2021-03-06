#!/bin/bash

if [ ! -d build  ]
then
    mkdir build
fi

cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=`kde4-config --prefix`
make

if [ $? != 0 ]
then
    echo
    echo "An error occured during compilation!"
    echo "Check if you have installed all needed header files."
    exit
fi

if [ ! `whoami` = "root" ]
then
    echo
    echo "Installation requires root privileges - Ctrl+C to cancel."
    sudo make install

    if [ $? != 0 ]
    then
        exit
    fi
else
    make install
fi

if [ `whoami` = "root" ]
then
    exit
fi

kbuildsycoca4
