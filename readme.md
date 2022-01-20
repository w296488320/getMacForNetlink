## 设备核心网卡mac获取方法深入分析

很多设备指纹是通过netlink的方式去获取的网卡地址，这种方式可以直接绕过android的权限。

在不给app授权的时候也可以直接获取到网卡信息。因为很难进行mock，所以很多大厂app也都是采用这种办法去获取。

下面主要介绍获取方法和获取流程。



### netlink简介：

- Netlink是linux提供的用于内核和用户态进程之间的通信方式。
  - 但是注意虽然Netlink主要用于**用户空间**和**内核空间**的通信，但是也能用于**用户空间的两个进程通信**。
  - 只是进程间通信有其他很多方式，一般不用Netlink。除非需要用到Netlink的广播特性时。
  - NetLink机制是一种特殊的socket，**它是Linux特有的**，由于传送的消息是暂存在socket接收缓存中，并不为接受者立即处理，所以netlink是一种异步通信机制。系统调用和ioctl是同步通信机制。



一般来说用户空间和内核空间的通信方式有三种：

> proc
>
> ioctl
>
> Netlink

而前两种都是单向的，但是Netlink可以实现**双工通信**。

Netlink协议基于BSD socket和AF_NETLINK地址簇(address family)。



使用32位的端口号寻址(以前称为PID)，每个Netlink协议(或称作总线，man手册中则称之为netlink family)，通常与一个或者一组内核服务/组件相关联，如NETLINK_ROUTE用于获取和设置路由与链路信息、NETLINK_KOBJECT_UEVENT用于内核向用户空间的udev进程发送通知等。

### netlink特点：

> 1,支持全双工、异步通信
>
> 2,用户空间可以使用标准的BSD socket接口(但netlink并没有屏蔽掉协议包的构造与解析过程，推荐使用libnl等第三方库)
>
> 3,在内核空间使用专用的内核API接口
>
> 4,支持多播(因此支持“总线”式通信，可实现消息订阅)
>
> 5,在内核端可用于进程上下文与中断上下文

### netlink优点：

> - netlink使用简单，只需要在include/linux/netlink.h中增加一个新类型的 netlink 协议定义即可,(如 #define NETLINK_TEST 20 然后，内核和用户态应用就可以立即通过 socket API 使用该 netlink 协议类型进行数据交换);
> - netlink是一种异步通信机制，在内核与用户态应用之间传递的消息保存在socket缓存队列中，发送消息只是把消息保存在接收者的socket的接收队列，而不需要等待接收者收到消息；
> - 使用 netlink 的内核部分可以采用模块的方式实现，使用 netlink 的应用部分和内核部分没有编译时依赖;
> - netlink 支持多播，内核模块或应用可以把消息多播给一个netlink组，属于该neilink 组的任何内核模块或应用都能接收到该消息，内核事件向用户态的通知机制就使用了这一特性；
> - 内核可以使用 netlink 首先发起会话;



## 如何通过netlink获取网卡信息？

### android 是如何通过netlink获取网卡地址的？

不管是ip命令行还是Java的network接口，最终都是调用到ifaddrs.cpp -> getifaddrs

### getifaddrs方法介绍

源码摘抄自：

http://aospxref.com/android-10.0.0_r47/xref/bionic/libc/bionic/ifaddrs.cpp#236

```
//传入对应的结构体指针
int getifaddrs(ifaddrs** out) {
  // We construct the result directly into `out`, so terminate the list.
  *out = nullptr;

  // Open the netlink socket and ask for all the links and addresses.
  NetlinkConnection nc;
  //判断get addresses 和 get link是否打开成功，返回成功则返回0
  bool okay = nc.SendRequest(RTM_GETLINK) && nc.ReadResponses(__getifaddrs_callback, out) &&
              nc.SendRequest(RTM_GETADDR) && nc.ReadResponses(__getifaddrs_callback, out);
  if (!okay) {
    out = nullptr;
    freeifaddrs(*out);
    // Ensure that callers crash if they forget to check for success.
    *out = nullptr;
    return -1;
  }

  return 0;
}
```

NetlinkConnection这个结构体是一个netlink的封装类

重点看一下ReadResponses的实现过程



代码摘抄自：

http://aospxref.com/android-10.0.0_r47/xref/bionic/libc/bionic/bionic_netlink.cpp



```

/**
 * @param type  发送参数的类型,具体获取的内容参考
 * @see rtnetlink.h
 * @return
 */
bool NetlinkConnection::SendRequest(int type) {
  // Rather than force all callers to check for the unlikely event of being
  // unable to allocate 8KiB, check here.
  // NetlinkConnection构造方法 的时候生成的8kb的data内存
  if (data_ == nullptr) return false;

  // Did we open a netlink socket yet?
  if (fd_ == -1) {
    //尝试建立socket netlink 链接
    fd_ = socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd_ == -1) return false;
  }

  // Construct and send the message.
  // 构造要发送的消息
  struct NetlinkMessage {
    nlmsghdr hdr;
    rtgenmsg msg;
  } request;

  memset(&request, 0, sizeof(request));
  request.hdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
  request.hdr.nlmsg_type = type;
  request.hdr.nlmsg_len = sizeof(request);
  // All families
  request.msg.rtgen_family = AF_UNSPEC;
  //使用socket数据发送
  return (TEMP_FAILURE_RETRY(send(fd_, &request, sizeof(request), 0)) == sizeof(request));
}
```



```
/*
 * 获取socket的返回结果
 */
bool NetlinkConnection::ReadResponses(void callback(void*, nlmsghdr*), void* context) {
  // Read through all the responses, handing interesting ones to the callback.
  ssize_t bytes_read;
  while ((bytes_read = TEMP_FAILURE_RETRY(recv(fd_, data_, size_, 0))) > 0) {
    //将拿到的data数据进行赋值
    auto* hdr = reinterpret_cast<nlmsghdr*>(data_);

    for (; NLMSG_OK(hdr, static_cast<size_t>(bytes_read)); hdr = NLMSG_NEXT(hdr, bytes_read)) {
      //判断是否读取结束,否则读取callback
      if (hdr->nlmsg_type == NLMSG_DONE) return true;
      if (hdr->nlmsg_type == NLMSG_ERROR) {
        auto* err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(hdr));
        errno = (hdr->nlmsg_len >= NLMSG_LENGTH(sizeof(nlmsgerr))) ? -err->error : EIO;
        return false;
      }
      //处理具体逻辑
      callback(context, hdr);
    }
  }

  // We only get here if recv fails before we see a NLMSG_DONE.
  return false;
}
```



### 使用流程：

通过遍历拿到我们需要的内容，输出即可。

```
int listmacaddrs(void) {
    struct ifaddrs *ifap, *ifaptr;

    if (getifaddrs(&ifap) == 0) {
        for (ifaptr = ifap; ifaptr != NULL; ifaptr = (ifaptr)->ifa_next) {
#ifdef __linux__
            char macp[INET6_ADDRSTRLEN];
            if(ifaptr->ifa_addr!= nullptr) {
                if (((ifaptr)->ifa_addr)->sa_family == AF_PACKET) {
                    auto *sockadd = (struct sockaddr_ll *) (ifaptr->ifa_addr);
                    int i;
                    int len = 0;
                    for (i = 0; i < 6; i++) {
                        len += sprintf(macp + len, "%02X%s", sockadd->sll_addr[i],( i < 5 ? ":" : ""));
                    }
                    if(strcmp(ifaptr->ifa_name,"wlan0")== 0){
                        LOG(INFO) << (ifaptr)->ifa_name << "  " << macp   ;
                        return 1;
                    }
                }
            }
#else
            if (((ifaptr)->ifa_addr)->sa_family == AF_LINK) {
                ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifaptr)->ifa_addr);
                printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n",
                                    (ifaptr)->ifa_name,
                                    *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
            }
#endif
        }
        freeifaddrs(ifap);
        return 1;
    } else {
        return 0;
    }
}
```



参考文章：
https://blog.csdn.net/zhizhengguan/article/details/120448337