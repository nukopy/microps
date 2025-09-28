#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"

#include "util.h"

// 割り込み要求（IRQ, Interrupt Request）の構造体
// ネットワークデバイス（net_device 構造体）と同様に linked list 構造で管理する
struct irq_entry {
  // 次の IRQ 構造体
  struct irq_entry *next;
  // 割り込み番号（IRQ 番号）
  unsigned int irq;
  // 割り込みハンドラ（割り込みが発生した際に呼び出す関数へのポインタ）
  int (*handler)(unsigned int irq, void *dev);
  // フラグ（INTR_IRQ_SHARED が指定された場合は IRQ 番号を共有可能）
  int flags;
  // デバッグ出力で識別するための名前
  char name[16];
  /* 割り込みの発生元となるデバイス

  ----------

  struct net_device 以外にも対応できるように void * で保持
  */
  void *dev;
};

/* NOTE: if you want to add/delete the entries after intr_run(), you need to
 * protect these lists with a mutex. */
// IRQ リスト（リストの先頭を指すポインタ）
static struct irq_entry *irqs;

// シグナルマスク用のシグナル集合
static sigset_t sigmask;

// 割り込みスレッドのスレッド ID
static pthread_t tid;
// スレッド間の同期のためのバリア
static pthread_barrier_t barrier;
// バリアで同期するスレッドの数
// （今回はメインスレッド、割り込み処理スレッドの 2 つ）
static const int NUM_THREADS_FOR_BARRIER = 2;

/* 割り込み要求エントリ（IRQ エントリ）を IRQ リストに登録する
 *
 * ----------
 *
 * ## microps における「割り込み」の実装
 *
 * microps では、IRQ 番号にシグナル番号を使用する。
 * IRQ 番号は、ハードウェア割り込みにおいては、
 * 「どのハードウェアによって割り込み要求（IRQ）が発生したか」を識別するための数値。
 * microsはユーザ空間で動くので、カーネル空間で動くハードウェア割り込みの仕組みは使えない。
 * そこで処理の割り込みを行うためにシグナルを使ったソフトウェア割り込みを実装する。
 * ハードウェア割り込みを模倣した割り込みの仕組みをユーザ空間で実装する。
 *
 * ## シグナルによりソフトウェア割り込みにマルチスレッドを使用する理由
 *
 * シグナル受信時に非同期に実行されるシグナルハンドラでは実行できる処理が大きく制限される。
 * シグナル安全（signal-safe）、非同期安全な処理の範囲で処理を実装しなければならない。
 *
 * ref: https://www.jpcert.or.jp/sc-rules/c-sig30-c.html
 *
 * そのため、割り込み処理のために専用のスレッドを起動してシグナルの発生を待ち受けて処理する。
 * 割り込みの仕組みをマルチスレッドで実装する理由はこれ。
 *
 */
int intr_register_irq_entry(unsigned int irq,
                            int (*handler)(unsigned int irq, void *dev),
                            int flags, const char *name, void *dev) {
  struct irq_entry *entry;

  debugf("registering new IRQ: irq=%u, flags=%d, name=%s", irq, flags, name);

  // --------------------------------------------------
  // IRQ 番号が既に登録されている場合、
  // IRQ 番号の共有が許可されているかどうかチェック
  // どちらかが共有を許可していない場合はエラーを返す
  // --------------------------------------------------

  // IRQ リストを走査
  for (entry = irqs; entry; entry = entry->next) {
    // IRQ リストに新規 IRQ エントリの IRQ 番号が既に登録されている場合、
    // その IRQ 番号に対して追加のハンドラを登録できるかどうか確認する。
    if (entry->irq == irq) {
      /*
      XOR(^) はビット単位の排他的論理和（2 つの値が異なるとき 1 になる）
      真理値表 (A ^ B):
      A B | A^B
      0 0 | 0
      0 1 | 1
      1 0 | 1
      1 1 | 0
      */
      /*
      ・既存の IRQ のフラグが `INTR_IRQ_SHARED` と完全に一致しているか
        - flags と INTR_IRQ_SHARED が一致している場合は XOR の結果は偽になる
          - e.g. flags ^ INTR_IRQ_SHARED = 0001 ^ 0001 = 0000 -> 偽
        - flags と INTR_IRQ_SHARED が一致していない場合は XOR の結果は真
          - e.g. flags ^ INTR_IRQ_SHARED = 0000 ^ 0001 = 0001 -> 真
      ・新しく登録しようとしているフラグが`INTR_IRQ_SHARED`と完全に一致しているか
        -

      をチェックしていて、どちらか一方でも値が違えば衝突とみなす。

      注意点として、XOR は「`INTR_IRQ_SHARED`
      ビットが立っているかどうか」ではなく
      「値がまるごと同一かどうか」を判定している。
      フラグにほかのビットが含まれる場合は真になってしまうので、共有可否をビット単位で見る意図なら
      `flags & INTR_IRQ_SHARED` のようにマスクした方が適切かもしれない。
      */

      const int is_existing_irq_allows_share =
          (entry->flags ^ INTR_IRQ_SHARED) == 0;
      const int is_new_irq_allows_share = (flags ^ INTR_IRQ_SHARED) == 0;
      const int is_irq_allows_share =
          is_existing_irq_allows_share || is_new_irq_allows_share;
      if (!is_irq_allows_share) {
        errorf("conflicts with already registered IRQs");
        return -1;
      }
    }
  }

  // --------------------------------------------------
  // IRQ リストへ新しいエントリを追加
  // --------------------------------------------------

  // 新しい IRQ エントリのメモリを確保
  entry = memory_alloc(sizeof(*entry));
  if (!entry) {
    errorf("memory_alloc() on registering IRQ entry failure");
    return -1;
  }

  // IRQ エントリ構造体に値を設定
  entry->irq = irq;
  entry->handler = handler;
  entry->flags = flags;
  strncpy(entry->name, name, sizeof(entry->name) - 1);
  entry->dev = dev;

  // 新しい IRQ エントリを IRQ リストの先頭へ挿入
  entry->next = irqs;
  irqs = entry;

  // シグナル集合へ新しいシグナルを追加
  // ここって既存のシグナルの被らないの？
  // -> 被らない。既存のシグナルとの OR 演算なので。
  sigaddset(&sigmask, irq);
  debugf("registered new IRQ entry: irq=%u, name=%s", entry->irq, entry->name);

  return 0;
}

int intr_raise_irq(unsigned int irq) {
  // 割り込み処理用のスレッドへシグナルを送信
  // microps では、IRQ 番号にシグナル番号を使用する
  // 後続の処理では、
  // 発生したシグナルのシグナル番号と IRQ エントリの IRQ 番号が一致したときに
  // IRQ エントリに登録されているハンドラが実行される
  return pthread_kill(tid, (int)irq);
}

// 割り込み処理を実行するスレッドのエントリーポイント
static void *intr_thread(void *arg) {
  int terminate = 0;
  int sig;
  int err;

  struct irq_entry *entry;

  infof("start thread for interrupt");

  // メインスレッドと同期を取るための処理
  pthread_barrier_wait(&barrier);
  debugf("barrier unlocked on intr_thread!");

  // ハードウェア割り込みに見立てたシグナルを処理するループ
  while (!terminate) {
    // ハードウェア割り込みに見立てたシグナルが発生するまで待機
    err = sigwait(&sigmask, &sig);
    if (err) {
      errorf("sigwait() failure: %s", strerror(err));
      break;
    }

    // 発生したシグナルの種類に応じて処理
    switch (sig) {
    // SIGHUP シグナルの処理:
    // 割り込み処理用のスレッドへ終了を通知するためのシグナル
    case SIGHUP:
      terminate = 1;
      break;
    // デバイス割り込み用のシグナルを処理
    default:
      // IRQ リストを巡回
      for (entry = irqs; entry; entry = entry->next) {
        // IRQ 番号が一致するエントリの割り込みハンドラを呼び出す
        // microps では、IRQ 番号にシグナル番号を使用する
        if (entry->irq == (unsigned int)sig) {
          debugf("call ISR: irq=%d, name=%s", entry->irq, entry->name);
          entry->handler(entry->irq, entry->dev);
        }
      }
      break;
    }
  }
  warnf("terminated thread for interrupt");

  return NULL;
}

// 割り込み機構を起動する
// intr_init が呼ばれている状態でこの関数を呼ぶこと
int intr_run(void) {
  int err;

  // シグナルマスクの設定
  // intr_init 内で sigmask の設定ができていることが前提
  err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
  if (err) {
    errorf("pthread_sigmask");
    return -1;
  }

  // 割り込み処理スレッドの起動
  err = pthread_create(&tid, NULL, intr_thread, NULL);
  if (err) {
    errorf("pthread_create() failed on intr_run: %s", strerror(err));
    return -1;
  }

  // スレッドが動き出すまで待つ
  // 他のスレッドが同じように pthread_barrier_wait() を呼び出し、
  // バリアのカウントが指定の数になるまでスレッドを停止する
  infof("waiting for starting thread for interrupt with barrier...");
  pthread_barrier_wait(&barrier);
  infof("thread for interrupt started!");

  return 0;
}

void intr_shutdown(void) {
  int err;

  // 割り込み処理スレッドが起動済みかどうか確認
  /* Q. なぜ以下のコードで割り込み処理スレッドが起動済みかどうか確認できるのか？
   * 前提：関数の実行順序は intr_init -> intr_run -> intr_shutdown
   *
   * `pthread_equal(tid, pthread_self())`は
   *「今の呼び出し元スレッド（メインスレッド）の
   * ID」と、グローバルに保持している `tid` を比較している。
   *
   * ここの比較は `intr_init` の段階で `tid` に「メインスレッド（=呼び出し元）
   * の `pthread_t`」を代入しておく前提になっている。
   * `intr_run` で割り込み処理スレッドを起動するときに
   * `pthread_create(&tid, …)` として `tid` を新しいスレッドの ID で上書きする。
   *
   * つまり、`intr_shutdown` をメインスレッドから呼ぶときに、
   * `tid` がまだメインスレッドと同じ ID のままなら
   * 「新しいスレッドに置き換わっていない → スレッドを作れていない」＝
   *`pthread_equal` が真になる、という判定になる。
   *
   * 逆に、スレッドが正常に起動済みなら `tid` は別の ID に差し替わっているので、
   * `pthread_equal` は偽になり、 `pthread_kill` や `pthread_join`
   * に進む、という流れとなっている。

   * 注意点として、このロジックは
   *
   * - `intr_init` をメインスレッドで呼ぶこと
   * - 初期値として `tid = pthread_self()` をセットしていること
   * - `intr_shutdown` をメインスレッドで呼ぶこと
   *
   * の 3 つが前提なので、別スレッドから呼び出したり、
   * 初期化を忘れたりすると意図通りにならない点には気をつける必要がある。
   */
  if (pthread_equal(tid, pthread_self()) != 0) {
    warnf("thread not created");
    return;
  }

  // 割り込み処理スレッドにシグナル（SIGHUP）を送信
  err = pthread_kill(tid, SIGHUP);
  if (err) {
    warnf("failed to kill process with SIGHUP: tid=%d, err=%s", tid,
          strerror(err));
    return;
  }

  // 割り込み処理スレッドが完全に終了するのを待つ
  err = pthread_join(tid, NULL);
  if (err) {
    warnf("failed to pthread_join: tid=%d, err=%s", tid, strerror(err));
    return;
  }

  infof("shutdown interrupt successfully!");

  return;
}

int intr_init(void) {
  infof("init interrupt");

  // スレッド ID の初期値にメインスレッドの ID を設定する
  tid = pthread_self();

  // pthread_barrier の初期化（カウントを 2 に設定）
  infof("init pthread barrier for %d threads", NUM_THREADS_FOR_BARRIER);
  pthread_barrier_init(&barrier, NULL, NUM_THREADS_FOR_BARRIER);
  debugf("barrier unlocked on intr_init!");

  // グローバルな割り込み処理用のシグナル集合（シグナルマスク）を初期化（空にする）
  infof("init sigmask");
  sigemptyset(&sigmask);

  // シグナル集合に SIGHUP を追加（割り込みスレッド終了通知用）
  sigaddset(&sigmask, SIGHUP);
  // 注意：この段階ではまだプロセス（メインスレッド）にシグナルマスクは設定されていない
  // pthread_sigmask が実行されて初めてシグナルマスクがプロセスに適用される

  return 0;
}
