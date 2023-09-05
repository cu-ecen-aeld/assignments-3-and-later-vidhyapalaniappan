#! /usr/bin/bash
echo "Hello World"
   
if [[ "$#" -eq 2 ]];
then
   echo "Two parameters are passed."
else
   echo "Two parameters are NOT passed"  
   exit 1
fi

arg_1=$1
arg_2=$2

echo "arg_1 is $arg_1"
echo "arg_2 is $arg_2"

#exctract name of file ..
name_file="${arg_1##*/}"
#extract name of directory
name_directory="${arg_1%/*}"

if ! [ -d "$name_directory" ]
then
   echo "The given path is not a valid file directory, so new directory created"
   mkdir -p $name_directory
fi


if [ -f "$arg_1" ]
then
	#writing writestr to file created
	echo "$arg_2" > "$arg_1"
	echo File Exists.
	exit 0 #successful operation
else
	echo file doesnt exist, creating file
	touch "$arg_1"
fi

if [ -f "$arg_1" ]
then
	#writing writestr to file created
	echo file created successfully
	echo "$arg_2" > "$arg_1"
	exit 0 #successful operation
else
	#else print an error message
	echo Error creating file.
	exit 1 #failed operationtouch "$arg_1"
fi

	
