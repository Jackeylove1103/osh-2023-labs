#define _GNU_SOURCE
       #include <unistd.h>
       #include <sys/syscall.h>
       #include <sys/types.h>
       #include <signal.h>
       #include <stdio.h>
       #include <linux/kernel.h>
       int
       main()
       {  char buffer[100];
          char string[20];
           long int a=syscall(548,buffer,100);//buffer enough
           long int b=syscall(548,string,20);// buffer not enough
           if(a==-1) 
           
           printf("The systemcall return value is -1\n");
          
           else
           printf("The systemcall return value is 0\n");
        
           if(b==-1)
           
           printf("The systemcall return value is -1\n");
           
           else
           printf("The systemcall return value is 0\n");
           return 0;
           
       }