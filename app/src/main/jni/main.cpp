#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <bionic_netlink.h>
#include <SubstrateHook.h>

#ifdef __linux__

#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <jni.h>
#include <Substrate/CydiaSubstrate.h>
#include <arch.h>
#include <Log.h>

void setaddrs();

#else
#include <net/if_dl.h>
#endif

int listmacaddrs() {
    struct ifaddrs *ifap, *ifaptr;

    if (getifaddrs(&ifap) == 0) {

        for (ifaptr = ifap; ifaptr != NULL; ifaptr = (ifaptr)->ifa_next) {
#ifdef __linux__
            char macp[INET6_ADDRSTRLEN];
            if (ifaptr->ifa_addr != nullptr) {
                if (((ifaptr)->ifa_addr)->sa_family == AF_PACKET) {
                    auto *sockadd = (struct sockaddr_ll *) (ifaptr->ifa_addr);
                    int i;
                    int len = 0;
                    for (i = 0; i < 6; i++) {
                        len += sprintf(macp + len, "%02X%s", sockadd->sll_addr[i],
                                       (i < 5 ? ":" : ""));
                    }
                    if (strcmp(ifaptr->ifa_name, "wlan0") == 0) {
                        LOGE ("%s  ", macp);
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


ssize_t (*org_recvfrom)(int fd, void *const buf,
                        size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addr_len) = nullptr;

ssize_t new_recvfrom(int fd, void *const buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addr_len) {

    auto *hdr = reinterpret_cast<nlmsghdr *>(buf);
    //判断是获取mac地址的消息
    if (hdr->nlmsg_type == RTM_NEWADDR) {
        LOGE ("org_recv 发现有人获取mac地址 ");

        ifaddrmsg* msg = reinterpret_cast<ifaddrmsg*>(NLMSG_DATA(hdr));

    }


    return org_recvfrom(fd, buf, len, flags, src_addr, addr_len);
}

static int SDK_INT = -1;

int get_sdk_level() {
    if (SDK_INT > 0) {
        return SDK_INT;
    }
    char sdk[92] = {0};
    __system_property_get("ro.build.version.sdk", sdk);
    SDK_INT = atoi(sdk);
    return SDK_INT;
}

char *getlibcPath() {
    char *libc = nullptr;

    if (sizeof(void *) == 8) {
        if (get_sdk_level() >= ANDROID_R) {
            libc = "/apex/com.android.runtime/lib64/bionic/libc.so";
        } else if (get_sdk_level() >= ANDROID_Q) {
            libc = "/apex/com.android.runtime/lib64/bionic/libc.so";
        } else {
            libc = "/system/lib64/libc.so";
        }
    } else {
        if (get_sdk_level() >= ANDROID_R) {
            libc = "/apex/com.android.runtime/lib/bionic/libc.so";
        } else if (get_sdk_level() >= ANDROID_Q) {
            libc = "/apex/com.android.runtime/lib/bionic/libc.so";
        } else {
            libc = "/system/lib/libc.so";
        }
    }
    return libc;
}


void setaddrs() {
    void *handle = dlopen("libc.so", RTLD_NOW);

    void *sym = dlsym(handle, "recvfrom");
    if (sym == nullptr) {
        LOGE ("recvfrom == null ");

    }
    MSHookFunction((void *) sym,
                   (void *) new_recvfrom,
                   (void **) &org_recvfrom);
    LOGE ("hook成功 ");


}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_jnihook_MainActivity_getmac(JNIEnv *env, jclass clazz) {
    //listmacaddrs();
    //setaddrs();
    listmacaddrs();

}

