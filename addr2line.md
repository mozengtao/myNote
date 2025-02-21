> addr2line translates addresses into file names and line numbers. Given an address in an executable or an offset in a section of a relocatable object, it uses the debugging information to figure out which file name and line number are associated with it.

- [addr2line(1)](https://linux.die.net/man/1/addr2line)
- [The addr2line Command](https://www.baeldung.com/linux/addr2line)

- [obtain a backtrace programmatically](https://www.gnu.org/software/libc/manual/html_node/Backtraces.html)
  ```c
  #include <execinfo.h>
  #include <stdio.h>
  #include <stdlib.h>

  /* Obtain a backtrace and print it to stdout. */
  void
  print_trace (void)
  {
    void *array[10];
    char **strings;
    int size, i;

    size = backtrace (array, 10);
    strings = backtrace_symbols (array, size);
    if (strings != NULL)
    {

      printf ("Obtained %d stack frames.\n", size);
      for (i = 0; i < size; i++)
        printf ("%s\n", strings[i]);
    }

    free (strings);
  }
  ```

- Example 1
  ```c
  // callstack.c：
    1  #include <stdio.h>
    2
    3  void foo(void)
    4  {
    5    printf("Inside foo\n");
    6  }
    7
    8  void bar(void)
    9  {
  10    printf("Inside bar\n");
  11    foo();
  12  }
  13
  14  void baz(void)
  15  {
  16    printf("Inside baz\n");
  17    bar();
  18  }
  19
  20  int main(void)
  21  {
  22    printf("Inside main\n");
  23    baz();
  24    return 0;
  25  }

  gcc -g -o callstack.out callstack.c

  // displays the lines that call other functions, also the addresses of these instructions and the names of the functions they call.
  $ objdump -d -j .text callstack.out | grep "call.*<"
    107f:       ff 15 53 2f 00 00       call   *0x2f53(%rip)        # 3fd8 <__libc_start_main@GLIBC_2.34>
    1122:       e8 19 ff ff ff          call   1040 <__cxa_finalize@plt>
    1127:       e8 64 ff ff ff          call   1090 <deregister_tm_clones>
    115b:       e8 f0 fe ff ff          call   1050 <puts@plt>
    1175:       e8 d6 fe ff ff          call   1050 <puts@plt>
    117a:       e8 ca ff ff ff          call   1149 <foo>
    1194:       e8 b7 fe ff ff          call   1050 <puts@plt>
    1199:       e8 c5 ff ff ff          call   1163 <bar>
    11b3:       e8 98 fe ff ff          call   1050 <puts@plt>
    11b8:       e8 c5 ff ff ff          call   1182 <baz>
  
  $ addr2line -f -e callstack.out 0x117a 0x1199 0x11b8
  bar
  /home/morrism/nodejs/t1.c:11
  baz
  /home/morrism/nodejs/t1.c:17
  main
  /home/morrism/nodejs/t1.c:23
  ```