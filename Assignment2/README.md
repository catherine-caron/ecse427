To test the sut.c library using the provided test files, run the following commands:
`gcc -pthread test1.c sut.c -o output`
`./output`
You can replace test1.c with any other file you want to test with. 

To run tests that require one C-EXEC thread, change the `int c_exec_number` variable to 1 in the `sut.c` file. It is on line 20. Switch `THREAD_STACK_SIZE` to `1024x64` on line 21. 

To run tests that require two C-EXEC thread, change the `int c_exec_number` variable to 2 in the `sut.c` file. It is on line 20. Switch `THREAD_STACK_SIZE` to `4096x64` on line 22. 

Please note that you must have the text files created and empty before you can run any IO tasks. This program does not create the file, nor does it erase it before writing to it. Failure to do this can cause errors. 