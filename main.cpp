#include <windows.h>
#include <stdio.h>
#include "wcwidth.h"

int main(int argc, char** argv)
{
    printf("wcwidth('A') == %d\n", wcwidth('A'));
    return 0;
}
