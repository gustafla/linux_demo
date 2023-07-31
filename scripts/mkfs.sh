#!/bin/sh
set -e

IN=data/*
OUT_C=src/data.c
OUT_H=src/data.h

# Clear previous output
echo > $OUT_C

# Write inclusion guard
printf '#ifndef DATA_H\n#define DATA_H\n\n' > $OUT_H

# Convert every data file to C source with xxd
for file in $IN; do
  xxd -i $file >> $OUT_C
done

# Write an array of filenames (null-terminated) for indexing
printf 'const char *data_filenames =' >> $OUT_C
printf 'extern const char *data_filenames;\n' >> $OUT_H
for file in $IN; do
  printf \ \"$file\\\\0\" >> $OUT_C
done
# Write a final null sentinel to end of array
printf ' "\\0";\n' >> $OUT_C

# Write an array of pointers to data in the same order as filenames
printf 'const unsigned char *data_ptrs[] = {\n' >> $OUT_C
printf 'extern unsigned char *data_ptrs[];\n' >> $OUT_H
for file in $IN; do
  echo "$file," | sed 's/[-\.\/]/_/g' >> $OUT_C
done
printf '};\n' >> $OUT_C

# Write an array of data lengths in the same order as filenames
printf 'const unsigned int data_lens[] = {\n' >> $OUT_C
printf 'extern unsigned int data_lens[];\n' >> $OUT_H
for file in $IN; do
  stat -c %s $file >> $OUT_C
  printf ',' >> $OUT_C
done
printf '};\n' >> $OUT_C

# Write inclusion guard endif
printf '\n#endif\n' >> $OUT_H
