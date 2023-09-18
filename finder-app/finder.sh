#!/bin/sh


: '
 @file    		:   finder.sh
 @brief   		:   Script to find number of files in a directory having the given input search string
 
 @author  		:   Vidhya Palaniappan
 @date    		:   Sep 04, 2023
  
 @references    	:   https://www.geeksforgeeks.org/how-to-pass-and-parse-linux-bash-script-arguments-and-parameters/
 			    https://www.geeksforgeeks.org/grep-command-in-unixlinux/
 		
'

if [[ "$#" -eq 2 ]]; then
	echo Two arguments are passed, so proceed further.
else
	echo Two arguments are NOT passed, give proper arguments
   	exit 1      #Failure Operation
fi


#Checking if the given directory is valid or not
if [ ! -d "$1" ]; then
   	echo The given path is not a valid file directory.
   	exit 1      #Failure Operation
fi


filesdir="$1"
searchstr="$2"


#grep -o -i linux linux_test.txt | wc -l

X="$(find "$filesdir" -type f | wc -l)"                   #storing the number of files in directory in variable X
Y="$(grep -rnw "$filesdir" -e "$searchstr" | wc -l)"	  #storing the number of matching lines in variable Y

echo The number of files are $X and the number of matching lines are $Y
exit 0 						          #successful operation


 






























