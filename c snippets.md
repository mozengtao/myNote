- 获取当前执行程序的名称
    ```c
    /**
    * A simple program to compare different ways of getting your program's name.
    */
    #include <stdio.h>
    #include <stdlib.h>

    extern char* __progname;
    extern char* program_invocation_name;
    extern char* program_invocation_short_name;

    int main(int argc, char** argv)
    {
        printf("argv[0]\n\t%s\n", argv[0]);
        printf("__progname\n\t%s\n", __progname);
        printf("program_invocation_name\n\t%s\n", program_invocation_name);
        printf("program_invocation_short_name\n\t%s\n", program_invocation_short_name);
        return 0;
    }

    Result:
        morrism@localhost /tmp/test $ ./a.out 
        argv[0]
                ./a.out
        __progname
                a.out
        program_invocation_name
                ./a.out
        program_invocation_short_name
                a.out
    ```
    - [setprogname.c](https://opensource.apple.com/source/Libc/Libc-1158.30.7/gen/FreeBSD/setprogname.c.auto.html)

- pretty print

        ```c
        #define pretty_print(fmt, ...) \
           do { printf("(%s:%s:%d) " fmt,\
           __FILE__, __func__, __LINE__, ##__VA_ARGS__); } while (0)
        ```
- [Standard Predefined Macros](https://gcc.gnu.org/onlinedocs/gcc-4.3.6/cpp/Standard-Predefined-Macros.html)