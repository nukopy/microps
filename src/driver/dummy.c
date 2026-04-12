#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "platform.h"

#include "net.h"
#include "util.h"

#define DUMMY_MTU UINT16_MAX /* maximum size of IP datagram (0 ~ 65535) */

// ダミーデバイスが使う IRQ 番号
// microps の IRQ 番号は 35 始まり
/* platform/linux/platform.h
#define INTR_IRQ_BASE (SIGRTMIN+1)
*/
#define DUMMY_IRQ INTR_IRQ_BASE

static int dummy_transmit(struct net_device *dev, uint16_t type,
                          const uint8_t *data, size_t len, const void *dst) {
    debugf("dummy transmit to device: dev=%s, type=0x%04x, len=%zu", dev->name,
           type, len);
    debugdump(data, len);

    // データを破棄
    // ダミーデバイスなのでデータに対しては何もしない

    // テスト用に割り込みを発生させる
    intr_raise_irq(DUMMY_IRQ);

    return 0;
}

// デバイスドライバに実装されている関数へのポインタを格納する構造体
// ダミーデバイス用に transmit のみ登録
static struct net_device_ops dummy_ops = {
    .transmit = dummy_transmit,
};

static int dummy_isr(unsigned int irq, void *id) {
    // 現段階ではダミーデバイス用の割り込みハンドラが呼び出されたことがわかれば良いのでデバッグ出力のみ
    // FIXME: 本来は net_device 以外にも対応するため、キャストは行わない。
    // あくまで現段階ではデバッグ用
    struct net_device *dev = (struct net_device *)id; // 無理やりキャスト
    debugf("ISR called: irq=%u, dev=%s", irq, dev->name);

    return 0;
}

/* ダミーデバイスの初期化

## ダミーデバイスの仕様

- 入力：なし（データを受信することはない）
- 出力：データを破棄
*/
struct net_device *dummy_init(void) {
    struct net_device *dev;

    // ダミーデバイスを生成
    dev = net_device_alloc();
    if (!dev) {
        errorf("net_device_alloc() failure on initializing dummy device");
        return NULL;
    }

    // ダミーデバイスの設定
    // note: dev->name は net_device_register
    // 内で更新されるのでここでは設定しない
    dev->type = NET_DEVICE_TYPE_DUMMY;
    dev->mtu = DUMMY_MTU;
    // ヘッダもアドレスも存在しない
    dev->hlen = 0;
    dev->alen = 0;
    // デバイスドライバに実装されている関数へのポインタを格納する構造体
    dev->ops = &dummy_ops;

    // デバイスを登録
    if (net_device_register(dev) == -1) {
        debugf("net_device_register() failure on initializaing dummy device: "
               "dev=%s, type=0x%04x",
               dev->name, dev->type);
        return NULL;
    }
    debugf("dummy device initialized: dev=%s", dev->name);

    // デバイスの割り込みハンドラとして dummy_isr を登録する
    infof("register interrupt handler for dummy device");
    intr_register_irq_entry(DUMMY_IRQ, dummy_isr, INTR_IRQ_SHARED, dev->name,
                            dev);

    return dev;
}
