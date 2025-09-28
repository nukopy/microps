#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "net.h"
#include "util.h"

#include "driver/dummy.h"

#include "test.h"

static volatile sig_atomic_t terminate;

/* シグナルハンドラ

原則、シグナルハンドラの中では下記以外のことはしない：

- 非同期安全な関数の呼び出し
  - ref: https://www.jpcert.or.jp/sc-rules/c-sig30-c.html
- volatile sig_atomic_t 型の変数への書き込み
*/
static void on_signal(int signal) {
  /*
  一般に、I/O 関数をシグナルハンドラ内で呼び出すのは安全ではない。
  シグナルハンドラ内で I/O 関数を使用する場合は、
  使用するシステムの非同期安全な関数を事前に確認すること。
  */
  // warnf("on_signal: signal received %d", signal);

  /* `(void)signal;` は必要？
  `(void)signal;` は「この引数は使っていない」ことを意図的に示す
  ためのお決まりの書き方。多くのコンパイラは未使用引数に警告を出すので、
  それを抑止する目的で入れている。

  - 既にハンドラ内で `signal` を使って何かするなら不要。
  - 使わないならそのまま残しておけば警告を避けられる。
  - 別の書き方として、
    - `__attribute__((unused))`
    - `void on_signal(int \/\*signal\*\/)`
  - のようにコメントで示す例もある。
  - 単に `(void)signal;` がもっとも広く使われる方法です。
  */

  // (void)signal;
  terminate = 1;
}

int main(int argc, char *argv[]) {
  struct net_device *dev;

  // シグナルハンドラの設定
  // Ctrl + C が押された際に行儀よく終了するようにする
  signal(SIGINT, on_signal);

  // プロトコルスタックの初期化
  if (net_init() == -1) {
    errorf("net_init() failure");
    return -1;
  }

  // ダミーデバイスの初期化（ダミーデバイスの生成、設定、登録）
  dev = dummy_init();

  // プロトコルスタックの起動
  if (net_run() == -1) {
    errorf("net_run() failure");
    return -1;
  }

  // 1 秒おきにデバイスにパケットを書き込む
  // まだパケットを自力で生成できないのでテストデータを用いる
  uint16_t type = 0x0800;
  size_t len = sizeof(test_data);
  // Ctrl + C が押されるとシグナルハンドラ on_signal() の中で
  // terminate に 1 が設定され、while ループを抜ける
  while (!terminate) {
    // Internet Layer -> Network Interface Layer への書き込み
    // Internet Layer から来たパケットをデバイスに書き込む
    if (net_device_output(dev, type, test_data, len, NULL) == -1) {
      errorf("net_device_output() failure");
      break;
    }

    sleep(1);
  }

  // プロトコルスタックの停止
  net_shutdown();

  return 0;
}
