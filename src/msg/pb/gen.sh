#!/bin/bash

protoc --cpp_out . *.proto

for file in `ls *.cc`
do
    mv $file ${file/%.cc/.cpp}
done
