#define _GNU_SOURCE
       #include <unistd.h>
       #include <sys/syscall.h>
       #include <sys/types.h>
       #include <signal.h>
       #include <stdio.h>
       #include <linux/kernel.h>
       int
       main()
       {  char *buffer;
           long int a=syscall(548,buffer);
           if(a==-1)
           {
           printf("The systemcall return value is -1");
           return -1;}
           else{
           printf("The systemcall return value is 0");
           return 0;
           }
       }