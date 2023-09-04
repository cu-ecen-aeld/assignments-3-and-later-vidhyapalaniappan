#! /usr/bin/bash
echo "Hello World"

if [[ "$#" -eq 2 ]];
then
   echo "Two parameters are passed."
else
   echo "Two parameters are NOT passed"
   exit 1
fi

if [ ! -d "$1" ]
then
   echo "The given path is not a valid file directory"
   exit 1
fi


arg_1="$1"
arg_2="$2"

echo "arg_1 is $arg_1"
echo "arg_2 is $arg_2"


#grep -o -i linux linux_test.txt | wc -l

X="$(find "$arg_1" -type f | wc -l)"
        #assign value to Y as number of matches
Y="$(grep -rnw "$arg_1" -e "$arg_2" | wc -l)"

echo The number of files are $X and the number of matching lines are $Y
exit 0 #successful operation


 






























