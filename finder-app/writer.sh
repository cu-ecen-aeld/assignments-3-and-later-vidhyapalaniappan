#! /usr/bin/bash

: '
 @file    		:   writer.sh
 @brief   		:   Script to write input string to a particular file
 
 @author  		:   Vidhya Palaniappan
 @date    		:   Sep 04, 2023
  
 @references    	:   https://java2blog.com/check-number-of-arguments-bash/					#:~:text=Use%20the%20%24%23%20variable%20to,arguments%20passed%20to%20Bash%20script.&text=echo%20%22Arguments%20Count%3A%20No%20arguments%20provided.%22
 		
'


#Checking if exactly two arguments are passed
if [[ "$#" -eq 2 ]]; then
	echo Two arguments are passed, so proceed further.
else
	echo Two arguments are NOT passed, give proper arguments  
  	exit 1
fi


writefile=$1  #storing the first argument in writefile variable
writestr=$2   #storing the second argument in writestr variable

directory_path="${writefile%/*}"  #storing the path of the directory in a variable


if ! [ -d "$directory_path" ]; then    #Checking if the directory already exists
	echo The given path is not a valid file directory, so new directory created
   	mkdir -p $directory_path
fi


if [ -f "$writefile" ]; then                    #Checking if the file already exists
	echo "$writestr" > "$writefile"         #writing the input string the file
	echo File already exists.
	exit 0 					#successful operation
else
	echo File does not exist, so creating the file... 
	touch "$writefile"			#creating a new file
fi


if [ -f "$writefile" ]; then
	echo New file created successfully       
	echo "$writestr" > "$writefile"        #writing the input string the new file created
	exit 0 				       #successful operation
else
	echo Error creating file.              #if the file creation failed
	exit 1 				       #failed operation 
fi

	
