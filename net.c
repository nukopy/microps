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

static int net_device_open(struct net_device *dev) {
  // デバイスの状態を確認（既に UP 状態の場合はエラーを返す）
  if (NET_DEVICE_IS_UP(dev)) {
    errorf("device is already opened: dev=%s", dev->name);
    return -1;
  }

  // デバイスドライバのオープン関数を呼び出す
  // - オープン関数が設定されていない場合は呼び出しをスキップ
  // - エラーが返されたらこの関数もエラーを返す
  if (dev->ops->open) {
    if (dev->ops->open(dev) == -1) {
      errorf("failed to open device: dev=%s", dev->name);
      return -1;
    }
  }

  // デバイスのオープンに成功したら UP フラグを立てる
  dev->flags |= NET_DEVICE_FLAG_UP;
  infof("opened device status: dev=%s, state=%s", dev->name,
        NET_DEVICE_STATE(dev));

  return 0;
}

static int net_device_close(struct net_device *dev) {
  // デバイスの状態を確認
  // UP 状態でないデバイスを close しようとしたときはエラーを返す
  if (!NET_DEVICE_IS_UP(dev)) {
    errorf("device is not opened: dev=%s", dev->name);
  }

  // デバイスドライバのクローズ関数を呼び出す
  // - クローズ関数が設定されていない場合は呼び出しをスキップ
  // - エラーが返されたらこの関数もエラーを返す
  if (dev->ops->close) {
    if (dev->ops->close(dev) == -1) {
      errorf("failed to close device: dev=%s");
      return -1;
    }
  }

  // UP フラグを落とす
  dev->flags &= ~NET_DEVICE_FLAG_UP;
  infof("closed device status: dev=%s, state=%s", dev->name,
        NET_DEVICE_STATE(dev));

  return 0;
}

// デバイスへの出力
int net_device_output(struct net_device *dev, uint16_t type,
                      const uint8_t *data, size_t len, const void *dst) {
  // デバイスの状態を確認
  // UP 状態でないデバイスに出力する場合はエラーを返す
  if (!NET_DEVICE_IS_UP(dev)) {
    errorf("device is not opened on transmit: dev=%s", dev->name);
    return -1;
  }

  // データのサイズを確認
  // デバイスの MTU を超えるサイズのデータは送信できないのでエラーを返す
  if (len > dev->mtu) {
    errorf("data is too long on transmit: dev=%s, mtu=%u, len=%zu", dev->name,
           dev->mtu, len);
    return -1;
  }
  /* note: フォーマット指定子 %04x について

  - `%04x`: 値を 16 進数で出力する。ゼロ 4 桁で padding。
    - 幅を 4 桁に指定し、ゼロで埋める (`%0` + `4` + `x`)
    - 16 進数で足りない桁は先頭を 0 で埋めて 4 桁で出力する
  */
  debugf("output to device: dev=%s, type=0x%04x, len=%zu", dev->name, type,
         len);
  debugdump(data, len);

  // デバイスドライバの出力関数を呼び出す
  // エラーが返されたらこの関数もエラーを返す
  if (dev->ops->transmit(dev, type, data, len, dst) == -1) {
    errorf("device transmit failure on output: dev=%s, len=%zu", dev->name,
           len);
    return -1;
  }

  return 0;
}

/* デバイスからの入力: デバイスが受信したパケットをプロトコルスタックに渡す関数

・プロトコルスタックへのデータの入口であり、デバイスドライバから呼び出されることを想定している
・複数のデバイスドライバからの入力を一手に受け取る

 デバイスドライからこの関数が呼び出されるイメージ:

           +----------------------+
           | net_input_handler()  |
           +-----------^----------+
                       |
          +------------+------------+
          |                         |
   +--------------+         +--------------+
   | Device Driver|         | Device Driver|
   +------^-------+         +------^-------+
          |                         |
        Device                   Device
*/
int net_input_handler(uint16_t type, const uint8_t *data, size_t len,
                      struct net_device *dev) {
  // TODO: 今の段階では呼び出されたことがわかればよいのでデバッグ出力のみ
  debugf("received data from device: dev=%s, type=0x%04x, len=%zu", dev->name,
         type, len);
  debugdump(data, len);

  return 0;
}

int net_run(void) {}

void net_shutdown(void) {}

int net_init(void) {}
