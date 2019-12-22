The csapp.c and csapp.h is provided by the csapp course group.
The io_mul.c, io_mul.h and my_proxy.c are the necessary source codes for the simple proxy to run.
Other irrelated contents and functions are the requirements of CSAPP LAB course.

my_proxy.c is a simple version with 4 states. 
my_proxy_fair.c provides a relatively fair schedule among clients.

The event automata:

<img src="https://714105382-personal-blog.oss-cn-zhangjiakou.aliyuncs.com/blog-11-03.png" alt="automata_simple_proxy_in_my_blog.png" style="zoom:50%;" />


compile:
	gcc -o proxy my_proxy.c io_mul.c csapp.c io_mul.h csapp.h -lpthread
	

Proxy:
	usage: proxy &lt;port&gt; 

