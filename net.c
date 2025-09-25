#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "platform.h"

#include "net.h"
#include "util.h"

/* NOTE: if you want to add/delete the entries after net_run(), you need to
 * protect these lists with a mutex. */
// デバイスリスト（リストの先頭を指すポインタ）
static struct net_device *devices;

struct net_device *net_device_alloc(void) {
  // pointer to new net_device
  struct net_device *dev;

  // デバイス構造体のサイズのメモリを確保
  // - memory_alloc() で確保したメモリ領域は 0 で初期化されている
  // - メモリが確保できなかったらエラーとして NULL を返す
  // - メモリの確保と解放には memory_alloc() と memory_free() を使う
  dev = memory_alloc(sizeof(*dev));
  if (!dev) {
    errorf("memory_alloc() failure");
    return NULL;
  }

  return dev;
}

/* NOTE: must not be call after net_run() */
// デバイスの登録
int net_device_register(struct net_device *dev) {
  // （関数の初回実行時だけ動く）デバイスのインデックス番号の初期化
  // note: 関数内で static な変数を一度だけ初期化し、関数呼出し間で保持できる。
  // note: 関数に副作用を入れることになる。
  // note: マルチスレッド環境でのデータ競合に注意。
  static unsigned int index = 0;

  // デバイスのインデックス番号の設定
  // note: 後置インクリメントなので現在の値を代入してからインクリメントされる
  dev->index = index++; // set, then increment

  // デバイス名を生成する（net0, net1, net2, ...）
  snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);

  // デバイスリストの先頭にデバイスを追加
  // ref: Day 1 - デバイスの生成と登録
  // https://docs.google.com/presentation/d/1ID6ggxASfc_1bWiJfDy1IFKIwzxvfYy8rUBrWYTRFj8/edit?slide=id.gd328c3072b_0_1179#slide=id.gd328c3072b_0_1179
  // 新規登録するデバイスの次のデバイスを、現在のデバイスリストの先頭のデバイスに設定する
  dev->next = devices;

  // 現在のデバイスリストの先頭のデバイスを新規登録するデバイスに設定する
  devices = dev;
  infof("registered device: dev=%s, type=0x%04x", dev->name, dev->type);

  return 0;
}


  return 0;
}

static int net_device_open(struct net_device *dev) {}

static int net_device_close(struct net_device *dev) {}

int net_device_output(struct net_device *dev, uint16_t type,
                      const uint8_t *data, size_t len, const void *dst) {}

int net_input_handler(uint16_t type, const uint8_t *data, size_t len,
                      struct net_device *dev) {}

int net_run(void) {}

void net_shutdown(void) {}

int net_init(void) {}
