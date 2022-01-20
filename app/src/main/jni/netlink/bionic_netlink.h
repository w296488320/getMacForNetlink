/*
 * Copyright (C) 2016 The Android Open Source Project
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

#pragma once

#include <sys/types.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <cstring>
#include <linux/if.h>
#include <linux/if_packet.h>
#include "ifaddrs.h"
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

struct nlmsghdr;


struct ifaddrs_storage {
    // Must come first, so that `ifaddrs_storage` is-a `ifaddrs`.
    ifaddrs ifa;

    // The interface index, so we can match RTM_NEWADDR messages with
    // earlier RTM_NEWLINK messages (to copy the interface flags).
    int interface_index;

    // Storage for the pointers in `ifa`.
    sockaddr_storage addr;
    sockaddr_storage netmask;
    sockaddr_storage ifa_ifu;
    char name[IFNAMSIZ + 1];

    explicit ifaddrs_storage(ifaddrs** list) {
        memset(this, 0, sizeof(*this));

        // push_front onto `list`.
        ifa.ifa_next = *list;
        *list = reinterpret_cast<ifaddrs*>(this);
    }

    void SetAddress(int family, const void* data, size_t byteCount) {
        // The kernel currently uses the order IFA_ADDRESS, IFA_LOCAL, IFA_BROADCAST
        // in inet_fill_ifaddr, but let's not assume that will always be true...
        if (ifa.ifa_addr == nullptr) {
            // This is an IFA_ADDRESS and haven't seen an IFA_LOCAL yet, so assume this is the
            // local address. SetLocalAddress will fix things if we later see an IFA_LOCAL.
            ifa.ifa_addr = CopyAddress(family, data, byteCount, &addr);
        } else {
            // We already saw an IFA_LOCAL, which implies this is a destination address.
            ifa.ifa_dstaddr = CopyAddress(family, data, byteCount, &ifa_ifu);
        }
    }

    void SetBroadcastAddress(int family, const void* data, size_t byteCount) {
        // ifa_broadaddr and ifa_dstaddr overlap in a union. Unfortunately, it's possible
        // to have an interface with both. Keeping the last thing the kernel gives us seems
        // to be glibc 2.19's behavior too, so our choice is being source compatible with
        // badly-written code that assumes ifa_broadaddr and ifa_dstaddr are interchangeable
        // or supporting interfaces with both addresses configured. My assumption is that
        // bad code is more common than weird network interfaces...
        ifa.ifa_broadaddr = CopyAddress(family, data, byteCount, &ifa_ifu);
    }

    void SetLocalAddress(int family, const void* data, size_t byteCount) {
        // The kernel source says "for point-to-point IFA_ADDRESS is DESTINATION address,
        // local address is supplied in IFA_LOCAL attribute".
        //   -- http://lxr.free-electrons.com/source/include/uapi/linux/if_addr.h#L17

        // So copy any existing IFA_ADDRESS into ifa_dstaddr...
        if (ifa.ifa_addr != nullptr) {
            ifa.ifa_dstaddr = reinterpret_cast<sockaddr*>(memcpy(&ifa_ifu, &addr, sizeof(addr)));
        }
        // ...and then put this IFA_LOCAL into ifa_addr.
        ifa.ifa_addr = CopyAddress(family, data, byteCount, &addr);
    }

    // Netlink gives us the prefix length as a bit count. We need to turn
    // that into a BSD-compatible netmask represented by a sockaddr*.
    void SetNetmask(int family, size_t prefix_length) {
        // ...and work out the netmask from the prefix length.
        netmask.ss_family = family;
        uint8_t* dst = SockaddrBytes(family, &netmask);
        memset(dst, 0xff, prefix_length / 8);
        if ((prefix_length % 8) != 0) {
            dst[prefix_length/8] = (0xff << (8 - (prefix_length % 8)));
        }
        ifa.ifa_netmask = reinterpret_cast<sockaddr*>(&netmask);
    }

    void SetPacketAttributes(int ifindex, unsigned short hatype, unsigned char halen) {
        sockaddr_ll* sll = reinterpret_cast<sockaddr_ll*>(&addr);
        sll->sll_ifindex = ifindex;
        sll->sll_hatype = hatype;
        sll->sll_halen = halen;
    }

private:
    sockaddr* CopyAddress(int family, const void* data, size_t byteCount, sockaddr_storage* ss) {
        // Netlink gives us the address family in the header, and the
        // sockaddr_in or sockaddr_in6 bytes as the payload. We need to
        // stitch the two bits together into the sockaddr that's part of
        // our portable interface.
        ss->ss_family = family;
        memcpy(SockaddrBytes(family, ss), data, byteCount);

        // For IPv6 we might also have to set the scope id.
        if (family == AF_INET6 && (IN6_IS_ADDR_LINKLOCAL(data) || IN6_IS_ADDR_MC_LINKLOCAL(data))) {
            reinterpret_cast<sockaddr_in6*>(ss)->sin6_scope_id = interface_index;
        }

        return reinterpret_cast<sockaddr*>(ss);
    }

    // Returns a pointer to the first byte in the address data (which is
    // stored in network byte order).
    uint8_t* SockaddrBytes(int family, sockaddr_storage* ss) {
        if (family == AF_INET) {
            sockaddr_in* ss4 = reinterpret_cast<sockaddr_in*>(ss);
            return reinterpret_cast<uint8_t*>(&ss4->sin_addr);
        } else if (family == AF_INET6) {
            sockaddr_in6* ss6 = reinterpret_cast<sockaddr_in6*>(ss);
            return reinterpret_cast<uint8_t*>(&ss6->sin6_addr);
        } else if (family == AF_PACKET) {
            sockaddr_ll* sll = reinterpret_cast<sockaddr_ll*>(ss);
            return reinterpret_cast<uint8_t*>(&sll->sll_addr);
        }
        return nullptr;
    }
};

class NetlinkConnection {
 public:
  NetlinkConnection();
  ~NetlinkConnection();

  bool SendRequest(int type);
  bool ReadResponses(void callback(void*, nlmsghdr*), void* context);

 private:
  int fd_;
  char* data_;
  size_t size_;
};
