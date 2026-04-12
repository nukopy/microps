#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define NET_DEVICE_TYPE_DUMMY 0x0000
#define NET_DEVICE_TYPE_LOOPBACK 0x0001
#define NET_DEVICE_TYPE_ETHERNET 0x0002

// NET_DEVICE_FLAG_UP:        0000_0000_0000_0001 = 1
#define NET_DEVICE_FLAG_UP 0x0001
// NET_DEVICE_FLAG_LOOPBACK:  0000_0000_0001_0000 = 16
#define NET_DEVICE_FLAG_LOOPBACK 0x0010
// NET_DEVICE_FLAG_BROADCAST: 0000_0000_0010_0000 = 32
#define NET_DEVICE_FLAG_BROADCAST 0x0020
// NET_DEVICE_FLAG_P2P:       0000_0000_0100_0000 = 64
#define NET_DEVICE_FLAG_P2P 0x0040
// NET_DEVICE_FLAG_NEED_ARP:  0000_0001_0000_0000 = 256 (= 16^2 * 1)
#define NET_DEVICE_FLAG_NEED_ARP 0x0100

#define NET_DEVICE_ADDR_LEN 16

// UP flag が立っていればビット演算の論理積 AND の値が非 0 になり、
// truthy な値となる
#define NET_DEVICE_IS_UP(x) ((x)->flags & NET_DEVICE_FLAG_UP)
#define NET_DEVICE_STATE(x) (NET_DEVICE_IS_UP(x) ? "up" : "down")

// ネットワークデバイスの定義
// ref:
// https://docs.google.com/presentation/d/1ID6ggxASfc_1bWiJfDy1IFKIwzxvfYy8rUBrWYTRFj8/edit?slide=id.gd328c3072b_0_1161#slide=id.gd328c3072b_0_1161
struct net_device {
    // 次のデバイスへのポインタ
    struct net_device *next;
    unsigned int index;
    char name[IFNAMSIZ];

    // デバイスの種別（net.h に NET_DEVICE_TYPE_XXX として定義)
    uint16_t type;

    // ----- デバイスの種別によって変化する値 ---------------
    uint16_t mtu; // デバイスの MTU (Maximum Transmission Unit) の値
    uint16_t flags;
    uint16_t hlen; /* header length */
    uint16_t alen; /* address length */
    // ------------------------------------------------

    // デバイスのハードウェアアドレス
    // - デバイスによってアドレスサイズが異なるので大きめのバッファを用意
    // - アドレスを持たないデバイスでは値は設定されない
    uint8_t addr[NET_DEVICE_ADDR_LEN];
    union {
        uint8_t peer[NET_DEVICE_ADDR_LEN];
        uint8_t broadcast[NET_DEVICE_ADDR_LEN];
    };

    // デバイスドライバに実装されている関数を
    // 格納している構造体 struct net_device_ops へのポインタ
    struct net_device_ops *ops;

    // デバイスドライバが使用するプライベートなデータへのポインタ
    void *priv;
};

// デバイスドライバに実装されている関数へのポインタを格納
struct net_device_ops {
    // optional
    int (*open)(struct net_device *dev);
    // optional
    int (*close)(struct net_device *dev);
    // required: 送信関数 transmit は必須
    int (*transmit)(struct net_device *dev, uint16_t type, const uint8_t *data,
                    size_t len, const void *dst);
};

extern struct net_device *net_device_alloc(void);
extern int net_device_register(struct net_device *dev);
extern int net_device_output(struct net_device *dev, uint16_t type,
                             const uint8_t *data, size_t len, const void *dst);

extern int net_input_handler(uint16_t type, const uint8_t *data, size_t len,
                             struct net_device *dev);

extern int net_run(void);
extern void net_shutdown(void);
extern int net_init(void);

#endif
