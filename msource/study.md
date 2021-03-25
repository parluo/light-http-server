### select

select示例：
```C++

    while(1){
        cpy_reads = reads; // select是通过FD_SET后的fd_set结构体的bit知道要监视哪些描述符，
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;
        if((fd_num=select(fd_max+1, &cpy_reads, 0, 0, &timeout))==-1){ // select是阻塞的，timeout防止一直阻塞下去，到点也能执行下面的，但函数返回是0
            break; 
        }
        if(fd_num == 0){
            continue;
        }
        for(i=0; i<fd_max+1; ++i){
            if(FD_ISSET(i, &cpy_reads)){ // 是否准备好 即使有很多文件描述在此范围内，但其实FD_ISSET并不监听
                if(i==serv_sock){
                    addr_sz = sizeof(clnt_addr);
                    clnt_sock = accept(serv_sock,(struct sockaddr*)&clnt_addr, &addr_sz);
                    FD_SET(clnt_sock, &reads);
                    if(fd_max<clnt_sock){ // 从监听的sock描述符开始，总是取最大的，select在此范围内监视，保证总不会超过此范围
                        fd_max = clnt_sock;
                    }
                    printf("connected client: %d \n",clnt_sock);
                    
                }
                else{
                    str_len = read(i, buf, BUF_SIZE);
                    if(str_len == 0){
                        FD_CLR(i, &reads); // 从内核监视数组fd_set中复位
                        close(i);
                        printf("closed client: %d \n",i);
                    }
                    else{
                        write(i, buf, str_len);
                    }
                }
            }
        }
    }


```


### epoll
1. epoll做什么的？
作为一种I/O复用技术， 解决select, poll性能不足的问题;
> select的问题：
> 1. 对所有文件描述符的轮询;
> 2. 调用select需要传递监视对象信息;
> 3. select相关函数： fd_set结构体，用FD_ZERO(&set)清零，FD_SET(pos, &set)设置, FD_CLR(pos,&set)复位清零


2. 如何实现epoll
- epoll_create：创建epoll文件描述符空间, 同样返回一个文件描述符fd
- epoll_ctl：向空间注册并注销文件描述符
- epoll_wait：等待文件描述符的变化
- epoll_event: 结构体，把发生事件的文件描述符单独集中到一起，
    - EPOLLET: 边缘触发，即一次来自client的写或者连接仅触发一次server的epoll_wait
    - EPOLLIN：需要读取数据的事件,也即输入缓冲有数据就会触发epoll_wait;
    - EPOLLOUT：输出为空，可以立即发送数据的情况,也即输出缓冲有数据就会触发epoll_wait;
    - EPOLLPRI：收到OOB数据(加急数据)的情况
    - EPOLLRDHUP： 断开连接或者两边都是写半关闭（仅接受不发送）的情况。常用来判断对端是否关闭。一般关闭连接时，EPOLLIN和EPOLLRDHUP都会触发，记得判断EPOLLREDHUP药房在EPOLLIN前面。
    - EPOLLONESHOT: 即使边缘触发也有可能出现：一个连接fd触发EPOLLIN被一个线程处理，读完了数据正在处理时，又来了数据结果又触发EPOLLIN,于是另一个线程来处理它，最后两个线程同时处理一个fd，产生错误。EPOLLONESHOT保证了一个fd的触发仅产生一次，但同样的也要求了在处理完这个fd后，线程要重新注册这个fd到epoll监视空间

epoll示例代码： 条件触发
```C++
    epfd = epoll_create(EPOLL_SIZE);
    ep_events = malloc(sizeof(struct epoll_event)* EPOLL_SIZE);
    event.events = EPOLLIN; // 有读取数据的情况时
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event); // 向epoll例程内部注册监视的文件描述符， add、del

    while(1){
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1); // timeout -1就是一直等待，0：不等待，直接返回
        if(event_cnt == -1){
            puts("epoll_wait() error");
            exit(1);
        }
        puts("return epoll_wait");
        for(i =0; i<event_cnt; ++i){
            if(ep_events[i].data.fd == serv_sock){
                addr_sz = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_sz);
                event.events = EPOLLIN; //  条件触发
                // event.events = EPOLLIN|EPOLLET; //
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event); // 新的监听事件仍然要注册到epfd中，就像select重要用FD_SET注册到fd_set中去
                printf("connect client: %d \n", clnt_sock);
            }
            else{
                //while(1){
                    str_len = read(ep_events[i].data.fd, buf, BUF_SIZE); // 一次接受没有读取完时，输入缓冲仍然有数据，就会再次注册，接着读取
                    // 条件触发，这里条件指的也就是输入缓冲，有就触发
                    if(str_len==0){
                        epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                        close(ep_events[i].data.fd);
                        printf("closed client: %d \n", ep_events[i].data.fd);
                    }
                    // else if(str_len==-1){
                    //     if(errno==EAGAIN) break;
                    // }
                    else{
                        write(ep_events[i].data.fd, buf, str_len);
                    }
                //}
            }
        }
    }

```

3. 理解边缘触发与条件触发
**条件触发**：有事件，有输入就会激活epoll_wait, 比如监听的fd（注册了EPOLLIN）中还有数据没读完就会一直触发。
**边缘触发**：同一事件，仅触发一次。所以对于可能的输入事件必须一次读完，如果没读完也不会提醒了。必须的等下次消息来了，才有机会继续读。常用while(1)， 文件描述符相关时，-1表错误返回，但errno变量为EAGAIN时表没有输入数据

> **fcntl**: 常用来修改客户端描述符为非阻塞， 不然，while(1)中的read write会卡在这  
一般在read和write时会阻塞（read会等待输入缓冲有数据，write会等待内核写入缓冲区容量足以copy写入）。但设置O_NONBLOCK后会形成非阻塞套接字， -1的原因可能很多，但是EAGAIN表示没有可读数据错误。
```C++
int preflags = fcntl(fd, F_GETFL, 0); // F_GETFL 获取原先的描述符状态
fcntl(fd, F_SETFL, flat|O_NONBLOCK); // F_SETFL 设置
```

> **dup**: 用来实现文件描述符的复制
```C++
int b = dup(a); //随即给复制的一个文件描述符，数字随机
int b = dup(a,7); // 复制a的描述符为7,给b
// 复制出来的相当于多个出口，关闭这个不影响别的出口使用
```

> tips: 文件描述符1是自动打开的标准输出。

4. 半关闭套接字指的是什么？
指完全关闭发送接受，变成关闭发送，暂时不关闭接收，即半关闭。
这取决于在关闭描述符发送EOF后，对方是否还要发送数据（结束对话或者正在发送，结果这边却关闭了），是的话那这一遍自然还要继续接受的，最后才完全关闭
实现示例：
```C++
...
shutdown(clnt_fd, SHUT_WR); // 关闭写，继续开放读
read(clnt_fd, buf, BUF_SIZE);
...
close(clnt_fd); // 最后关闭
...
```

所以和优雅断开套接字的区别？
优雅断开指的是server调用close关闭一个fd时，有数据在发送，则会等待发送完再关闭，   
半关闭指等待接收 
[ref](https://blog.csdn.net/z735640642/article/details/84306589) 
```C++
SO_BROADCAST    BOOL    允许套接口传送广播信息。
SO_DEBUG    BOOL    记录调试信息。
SO_DONTLINER    BOOL    不要因为数据未发送就阻塞关闭操作。设置本选项相当于将SO_LINGER的l_onoff元素置为零。
SO_DONTROUTE    BOOL    禁止选径；直接传送。
SO_KEEPALIVE    BOOL    发送“保持活动”包。
SO_LINGER   struct linger FAR*  如关闭时有未发送数据，则逗留。
SO_OOBINLINE    BOOL    在常规数据流中接收带外数据。
SO_RCVBUF   int 为接收确定缓冲区大小。
SO_REUSEADDR    BOOL    允许套接口和一个已在使用中的地址捆绑（参见bind()）。
SO_SNDBUF   int 指定发送缓冲区大小。
TCP_NODELAY BOOL    禁止发送合并的Nagle算法
```
5. 标准IO
**fdopen**: 把文件描述符转换为FIEL*
```C++
FILE *fp = fdopen(fd,"w");
```
**fielno**: 转换FILE*为文件描述符
6. 一些知识点
epoll_wait返回的条件

1、等待时间到期

2、发生信号事件，例如ctrl+c

3、The associated file is available for read(2) operations，如果注册了EPOLLIN， socket接收缓冲区，有新的数据到来

4、The associated file is available for write(2) operations，如果注册了EPOLLOUT， socket发送缓冲区可写时
————————————————
原文链接：https://blog.csdn.net/xxb249/article/details/105217956/

5. 端口号 vs 文件描述符
描述符：应用程序用来识别套接字的机制  
IP 地址和端口号：客户端和服务器之间用来识别对方套接字的机制  
[ref](https://blog.csdn.net/ck784101777/article/details/103746921?utm_medium=distribute.pc_relevant.none-task-blog-baidujs_title-4&spm=1001.2101.3001.4242)

### 信号量Segment
1. 用法
```C++
/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
int sem_init (sem_t *__sem, int __pshared, unsigned int __value)

sem_post(sem_t *__sem) // 相当加1

sem_wait(sem_t *__sem) // 相当于内部引用减1,=0时表示没有准备好的，阻塞
```


### [MYSQL的使用](https://blog.csdn.net/hjl240/article/details/75578004)
1. mysql_library_init() 初始化库
2. mysql_init() 初始化MYSQL对象
3. mysql_real_connect() 连接
3. mysql_close() 关闭对象
4. mysql_library_end() 关闭库连接



### Http使用
客户端发送的其实就是一串字符串消息。
服务端要先解析，再相应请求
request/response消息结构如下：
1.
- 请求行
    - GET/POST  方式
    - XXX.html  资源
    - HTTP/1.1  协议  
- 状态行
    - HTTP
    - 200  状态码
    - 
2. 消息头
    - 一些浏览器信息，用户认证啥的
3. 空行
4. 消息体
    - 客户端向服务端传的数据

5. 示例
```
下面是一个HTTP请求的例子：
GET/sample.jspHTTP/1.1
Accept:image/gif.image/jpeg,*/*
Accept-Language:zh-cn
Connection:Keep-Alive
Host:localhost
User-Agent:Mozila/4.0(compatible;MSIE5.01;Window NT5.0)
Accept-Encoding:gzip,deflate

username=jinqiao&password=1234
```

```
下面是一个HTTP响应的例子：
HTTP/1.1 200 OK
Server:Apache Tomcat/5.0.12
Date:Mon,6Oct2003 13:23:42 GMT
Content-Length:112
Content-Type:text/html
Last-Moified:Mon,6 Oct 2003 13:23:42 GMT
Content-Length:112

<html>
　　<head>
　　<title>HTTP响应示例<title>
　　</head>
　　<body>
　　　　Hello HTTP!
　　</body>
</html>
```


### C++知识
1. 左值 vs 右值
左值即关联了名称的内存位置， 右值则是一个临时值，不能被程序其他位置访问。
不能简单当做等号左边/右边的值。
**根本区别:** 左值右值根本区别在于能否获取内存地址。 一般临时变量为右值。


2. [正则表达式使用](http://help.locoy.com/Document/Learn_Regex_For_30_Minutes.htm)
ref2: https://blog.csdn.net/l357630798/article/details/78235307/
- 表达式：
. 除换行符任意字符
\w 字母 数字 下划线 汉字  
\s 匹配任意空白符  
\d 匹配数字    
\b 匹配单词开始或结尾  词  
^ 匹配字符串开始  串  
$ 匹配字符串结束  串  

- 转义
\  
\.用来匹配.  

- 重复
\* 重复0次或者多次  
\+ 重复一次或多次  
? 重复一次或者0次  
{n} 重复n次  
{n,} 重复n次或更多次  
{n,m} 重复n次到m次  

- 字符类
用[]框住，选择性匹配  
例如 [a-z0-9A-Z_]  

- 分支条件
用 | 隔开  
对|左右两边的正则一次测试，符合就不再继续匹配了

- 分组
用()把里面的当作一个组，可以使用{}多次匹配  
例如ip地址匹配： ((2[0-4]\d|25[0-5]|[01]?\d\d?)\.){3}(2[0-4]\d|25[0-5]|[01]?\d\d?)  

- 反义
表达式的大写
\W 
\S 不是空白符的字符
\D 非数字任意字符
\B 不是单词开头或结束的位置
[\^abc] 除了abc以外任意字符

- 向后引用
对小括号指定的某组子表达式进行重复搜索， \2 第二组的

- 零宽断言
用于查带某些内容的之前或之后的东西，
\b\w+(?=ing\b)  singing and dancing 会匹配sing danc  

- 负向零宽断言
匹配不能带有某些字符的项

std::regex

std::regex_match
std::regex_search
std::regex_replace
std::regex_iterator
std::regx_token_iterator