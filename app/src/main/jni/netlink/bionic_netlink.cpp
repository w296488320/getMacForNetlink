/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "bionic_netlink.h"
#include "allInclude.h"
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex>
#include <bits/getopt.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <asm/fcntl.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include "Log.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



extern "C" {
    __inline__ __attribute__((always_inline))  long raw_syscall(long __number, ...);
}

/**
 * form
 *
 * http://aospxref.com/android-10.0.0_r47/xref/bionic/libc/bionic/bionic_netlink.cpp
 *
 * @author zhenxi by 2022年1月18日14:07:54
 */
NetlinkConnection::NetlinkConnection() {
  fd_ = -1;

  // The kernel keeps packets under 8KiB (NLMSG_GOODSIZE),
  // but that's a bit too large to go on the stack.
  size_ = 8192;
  data_ = new char[size_];
}

NetlinkConnection::~NetlinkConnection() {
//  ErrnoRestorer errno_restorer;
  if (fd_ != -1) close(fd_);
  delete[] data_;
}

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
/*
 * 获取socket的返回结果
 */
bool NetlinkConnection::ReadResponses(void callback(void*, nlmsghdr*), void* out) {
  // Read through all the responses, handing interesting ones to the callback.
  ssize_t bytes_read;

while ((bytes_read = TEMP_FAILURE_RETRY(raw_syscall(__NR_recvfrom,fd_, data_, size_, 0, NULL,0))) > 0) {
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
      callback(out, hdr);
    }
  }

  // We only get here if recv fails before we see a NLMSG_DONE.
  return false;
}