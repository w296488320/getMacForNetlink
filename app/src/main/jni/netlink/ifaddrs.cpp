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

#include <ifaddrs.h>

#include <errno.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//#include "private/ErrnoRestorer.h"

#include "bionic_netlink.h"

// The public ifaddrs struct is full of pointers. Rather than track several
// different allocations, we use a maximally-sized structure with the public
// part at offset 0, and pointers into its hidden tail.

static void __getifaddrs_callback(void* context, nlmsghdr* hdr) {
  ifaddrs** out = reinterpret_cast<ifaddrs**>(context);

  //首先先判断消息类型是不是RTM_NEWLINK类型
  if (hdr->nlmsg_type == RTM_NEWLINK) {
      //LOGE ("开始执行正常 发现消息是 11111111");

    ifinfomsg* ifi = reinterpret_cast<ifinfomsg*>(NLMSG_DATA(hdr));

    // Create a new ifaddr entry, and set the interface index and flags.
    // 创建一份新的进行拷贝和赋值
    // 将两个**的封装成 ifaddrs_storage
    ifaddrs_storage* new_addr = new ifaddrs_storage(out);

    new_addr->interface_index = ifi->ifi_index;
    new_addr->ifa.ifa_flags = ifi->ifi_flags;

    // Go through the various bits of information and find the name.
    rtattr* rta = IFLA_RTA(ifi);
    //获取这个消息的长度
    size_t rta_len = IFLA_PAYLOAD(hdr);
    //这块是判断这个消息是否是合格的消息
    while (RTA_OK(rta, rta_len)) {
      if (rta->rta_type == IFLA_ADDRESS) {
          if (RTA_PAYLOAD(rta) < sizeof(new_addr->addr)) {
            new_addr->SetAddress(AF_PACKET, RTA_DATA(rta), RTA_PAYLOAD(rta));
            new_addr->SetPacketAttributes(ifi->ifi_index, ifi->ifi_type, RTA_PAYLOAD(rta));
          }
      } else if (rta->rta_type == IFLA_BROADCAST) {

          if (RTA_PAYLOAD(rta) < sizeof(new_addr->ifa_ifu)) {
            new_addr->SetBroadcastAddress(AF_PACKET, RTA_DATA(rta), RTA_PAYLOAD(rta));
            new_addr->SetPacketAttributes(ifi->ifi_index, ifi->ifi_type, RTA_PAYLOAD(rta));
          }
      } else if (rta->rta_type == IFLA_IFNAME) {

          if (RTA_PAYLOAD(rta) < sizeof(new_addr->name)) {
            memcpy(new_addr->name, RTA_DATA(rta), RTA_PAYLOAD(rta));
            new_addr->ifa.ifa_name = new_addr->name;
          }
      }
      rta = RTA_NEXT(rta, rta_len);
    }


  } else if (hdr->nlmsg_type == RTM_NEWADDR) {

      //消息封装
    ifaddrmsg* msg = reinterpret_cast<ifaddrmsg*>(NLMSG_DATA(hdr));

    // We should already know about this from an RTM_NEWLINK message.
    const ifaddrs_storage* addr = reinterpret_cast<const ifaddrs_storage*>(*out);

    while (addr != nullptr && addr->interface_index != static_cast<int>(msg->ifa_index)) {
      addr = reinterpret_cast<const ifaddrs_storage*>(addr->ifa.ifa_next);
    }
    // If this is an unknown interface, ignore whatever we're being told about it.
    if (addr == nullptr) return;

    // Create a new ifaddr entry and copy what we already know.
    ifaddrs_storage* new_addr = new ifaddrs_storage(out);

    // We can just copy the name rather than look for IFA_LABEL.
    strcpy(new_addr->name, addr->name);

    new_addr->ifa.ifa_name = new_addr->name;

    new_addr->ifa.ifa_flags = addr->ifa.ifa_flags;
    new_addr->interface_index = addr->interface_index;

    // Go through the various bits of information and find the address
    // and any broadcast/destination address.
    rtattr* rta = IFA_RTA(msg);
    size_t rta_len = IFA_PAYLOAD(hdr);


    while (RTA_OK(rta, rta_len)) {

      if (rta->rta_type == IFA_ADDRESS) {
        if (msg->ifa_family == AF_INET || msg->ifa_family == AF_INET6) {
          new_addr->SetAddress(msg->ifa_family,RTA_DATA(rta) , RTA_PAYLOAD(rta));
          new_addr->SetNetmask(msg->ifa_family, msg->ifa_prefixlen);
        }
      } else if (rta->rta_type == IFA_BROADCAST) {
          if (msg->ifa_family == AF_INET) {
          new_addr->SetBroadcastAddress(msg->ifa_family, RTA_DATA(rta), RTA_PAYLOAD(rta));
        }
      } else if (rta->rta_type == IFA_LOCAL) {
          if (msg->ifa_family == AF_INET || msg->ifa_family == AF_INET6) {
          new_addr->SetLocalAddress(msg->ifa_family, RTA_DATA(rta), RTA_PAYLOAD(rta));
        }
      }
      rta = RTA_NEXT(rta, rta_len);
    }
  }
}
void freeifaddrs(ifaddrs* list) {
  while (list != nullptr) {
    ifaddrs* current = list;
    list = list->ifa_next;
    free(current);
  }
}
int myGetifaddrs(ifaddrs** out) {
  // We construct the result directly into `out`, so terminate the list.
  *out = nullptr;

  // Open the netlink socket and ask for all the links and addresses.
  NetlinkConnection nc;
  bool okay = nc.SendRequest(RTM_GETLINK) && nc.ReadResponses(__getifaddrs_callback, out) &&
          nc.SendRequest(RTM_GETADDR) && nc.ReadResponses(__getifaddrs_callback, out);

  if (!okay) {
    //out = nullptr;
    ifaddrs *pIfaddrs = *out;
    if(pIfaddrs!= nullptr){
      freeifaddrs(pIfaddrs);
    }
    // Ensure that callers crash if they forget to check for success.
    *out = nullptr;
    return -1;
  }

  return 0;
}


