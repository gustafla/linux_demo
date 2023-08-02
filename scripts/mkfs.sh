#!/bin/sh
set -e

IN=data/*

# Convert every data file to C source with xxd
for file in $IN; do
  xxd -i $file
done

# Write an array of filenames (null-terminated) for indexing
printf 'const char *data_filenames ='
for file in $IN; do
  printf \ \"$file\\\\0\"
done
# Write a final null sentinel to end of array
printf ' "\\0";\n'

# Write an array of pointers to data in the same order as filenames
printf 'const unsigned char *data_ptrs[] = {\n'
for file in $IN; do
  echo "$file," | sed 's/[-\.\/]/_/g'
done
printf '};\n'

# Write an array of data lengths in the same order as filenames
printf 'const unsigned int data_lens[] = {\n'
for file in $IN; do
  stat -c %s $file
  printf ','
done
printf '};\n'
