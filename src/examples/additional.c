#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main(int argc, char** argv) {
    if(argc != 5) 
        return -1;
    int arr[4], i;
    for(i = 0; i < 4; i++)
        arr[i] = atoi(argv[i + 1]);

    printf("%d %d\n", fibonacci(arr[0]), max_of_four_int(arr[0], arr[1], arr[2], arr[3]));
}