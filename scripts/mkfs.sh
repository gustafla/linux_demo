#!/bin/sh
set -e

IN=data/*
OUT=$1

# Clear previous output
echo > $OUT

# Convert every data file to C source with xxd
for file in $IN; do
  xxd -i $file >> $OUT
done

# Write an array of filenames (null-terminated) for indexing
printf 'const char *data_filenames =' >> $OUT
for file in $IN; do
  printf \ \"$file\\\\0\" >> $OUT
done
# Write a final null sentinel to end of array
printf ' "\\0";\n' >> $OUT

# Write an array of pointers to data in the same order as filenames
printf 'const unsigned char *data_ptrs[] = {\n' >> $OUT
for file in $IN; do
  echo "$file," | sed 's/[-\.\/]/_/g' >> $OUT
done
printf '};\n' >> $OUT

# Write an array of data lengths in the same order as filenames
printf 'const unsigned int data_lens[] = {\n' >> $OUT
for file in $IN; do
  stat -c %s $file >> $OUT
  printf ',' >> $OUT
done
printf '};\n' >> $OUT
