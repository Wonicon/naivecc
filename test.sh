#!/usr/bin/env bash

TESTCASE=$(find ./test/ -name "*.cmm")
ASM="./tmp.S"

for file in $TESTCASE; do
    echo test $file

    ./cmm $file $ASM
    if [ $? -ne 0 ]; then
        echo "Compilation failed."
    fi

    spim -file $ASM # Only check the validation of the asm file
    if [ $? -ne 0 ]; then
        echo "Compilation incorrect."
    fi
done

rm $ASM
