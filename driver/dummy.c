#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "net.h"
#include "util.h"

#define DUMMY_MTU UINT16_MAX /* maximum size of IP datagram (0 ~ 65535) */

static int dummy_transmit(struct net_device *dev, uint16_t type,
                          const uint8_t *data, size_t len, const void *dst) {
  debugf("dummy transmit to device: dev=%s, type=0x%04x, len=%zu", dev->name,
         type, len);
  debugdump(data, len);

  // データを破棄
  // ダミーデバイスなのでデータに対しては何もしない
  return 0;
}

// デバイスドライバに実装されている関数へのポインタを格納する構造体
// ダミーデバイス用に transmit のみ登録
static struct net_device_ops dummy_ops = {
    .transmit = dummy_transmit,
};

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
  // note: dev->name は net_device_register 内で更新されるのでここでは設定しない
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

  return dev;
}
