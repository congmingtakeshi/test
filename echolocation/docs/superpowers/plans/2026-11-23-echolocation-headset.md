# エコーロケーション・ヘッドセット Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 40kHz超音波のエコーを録音し、10倍に時間伸長して左右そのまま両耳に流すヘッドセットのファームウェアを作る。

**Architecture:** 純粋なDSP関数（時間伸長、ブランキング、リミッタ、距離換算）をハード非依存のCライブラリに分け、ホストでユニットテストする。ESP-IDF側は、送信・録音・再生の薄いハードウェア層から成り、`app_main` が約4Hzのループで DSPライブラリを呼ぶ。

**Tech Stack:** PlatformIO（ESP-IDF フレームワーク）、C、Unity（`native` 環境のホストテスト）、Python 3（スペクトル確認ツール）、ESP32-S3、I2S。

**Spec:** `/home/user/echolocation/design.md`（部品は `/home/user/echolocation/parts-list.md`）

## Global Constraints

- マイコンは ESP32-S3
- 送信は 40kHz前後のバースト、8〜10周期、繰り返し約4Hz
- 録音窓は約20ms（往復3.4m）、マイクは左右2個で間隔約15cm
- 時間伸長の初期倍率は10倍、調整範囲は8〜20倍
- 出力は骨伝導またはオープンイヤー型。出力音量は必ずソフトのリミッタで制限する
- 送信直後はブランキングして、残響によるマイクの飽和を避ける
- ステージ0（マイクの録音検証）を最初に行い、結果で取り込み経路を決める
- 案B（物体検出・空間音合成）は含めない

## Review Focus

1. 送信直後の残響が大きく、ブランキングしないと再生音が耳に痛い大音量になる
2. 録音窓より再生バッファが小さい、または倍率が範囲外で、バッファを超えて書き込む
3. 再生音がリミッタを超え、ヘッドホンで大音量になる
4. 倍率の調整が範囲外（8未満、20超）に行き、音が壊れる
5. 左右のチャンネルが入れ替わる、または片側だけ鳴る

---

### Task 1: プロジェクト土台と純粋DSPライブラリ

**Files:**
- Create: `platformio.ini`、`src/main.c`（空の `app_main`）
- Create: `lib/dsp/dsp.h`、`lib/dsp/dsp.c`
- Test: `test/test_dsp/test_dsp.c`

**Interfaces:**
- Produces（すべて `lib/dsp/dsp.h`）:
  - `size_t dsp_time_stretch(const int16_t *in, size_t n_in, float factor, int16_t *out, size_t out_cap)`
    入力を factor 倍に引き伸ばして線形補間し、書いたサンプル数を返す。`factor` が 1.0 未満または 20.0 超なら 0 を返す。`out_cap` を超えては書かず、そこで打ち切る。
  - `void dsp_apply_blanking(int16_t *buf, size_t n, uint32_t fs_hz, uint32_t blank_us)` : 先頭 `blank_us` 分を0にする。
  - `void dsp_limit_peak(int16_t *buf, size_t n, int16_t ceiling)` : 絶対値が `ceiling` を超える分を `ceiling` に丸める。
  - `void dsp_interleave_lr(const int16_t *l, const int16_t *r, size_t n, int16_t *out)` : out は L,R,L,R… の順。
  - `uint32_t dsp_delay_to_distance_mm(uint32_t delay_us)` : 音速343m/sで、往復遅延から片道距離(mm)を返す。
  - `float dsp_step_factor(float current, int dir)` : dir が +1 / -1 で1.0刻みで増減し、8.0〜20.0 に丸める。

- [ ] **Step 1: `git init`、`platformio.ini` に `native` と `esp32-s3` の2環境を定義して、空の `app_main` がビルドできることを確認する**

Run: `pio run -e esp32-s3`
Expected: SUCCESS

- [ ] **Step 2: 失敗するテストを書く（`test/test_dsp/test_dsp.c`）**

```c
void test_stretch_length_is_n_in_times_factor(void) {   // 検証しやすい倍率2.0で長さを確認
    int16_t in[4] = {0, 100, 200, 300};
    int16_t out[16] = {0};
    TEST_ASSERT_EQUAL(8, dsp_time_stretch(in, 4, 2.0f, out, 16));
}
void test_stretch_rejects_out_of_range_factor(void) {
    int16_t in[4] = {0}, out[64];
    TEST_ASSERT_EQUAL(0, dsp_time_stretch(in, 4, 0.5f, out, 64));
    TEST_ASSERT_EQUAL(0, dsp_time_stretch(in, 4, 21.0f, out, 64));
}
void test_stretch_truncates_at_out_cap(void) {
    int16_t in[100] = {0}, out[10];
    TEST_ASSERT_EQUAL(10, dsp_time_stretch(in, 100, 10.0f, out, 10));
}
void test_blanking_zeroes_leading_samples_only(void) {
    int16_t b[10]; for (int i = 0; i < 10; i++) b[i] = 1000;
    dsp_apply_blanking(b, 10, 100000, 50);   // 100kHzで50us = 5サンプル
    TEST_ASSERT_EQUAL(0, b[4]);
    TEST_ASSERT_EQUAL(1000, b[5]);
}
void test_limiter_clips_both_polarities(void) {
    int16_t b[4] = {20000, -20000, 5000, -5000};
    dsp_limit_peak(b, 4, 8000);
    TEST_ASSERT_EQUAL(8000, b[0]); TEST_ASSERT_EQUAL(-8000, b[1]);
    TEST_ASSERT_EQUAL(5000, b[2]); TEST_ASSERT_EQUAL(-5000, b[3]);
}
void test_interleave_order_is_l_then_r(void) {
    int16_t l[2] = {1, 2}, r[2] = {10, 20}, o[4];
    dsp_interleave_lr(l, r, 2, o);
    TEST_ASSERT_EQUAL(1, o[0]); TEST_ASSERT_EQUAL(10, o[1]);
    TEST_ASSERT_EQUAL(2, o[2]); TEST_ASSERT_EQUAL(20, o[3]);
}
void test_delay_to_distance_for_1m_wall(void) {
    uint32_t mm = dsp_delay_to_distance_mm(5831);   // 2m往復の時間
    TEST_ASSERT_UINT32_WITHIN(5, 1000, mm);
}
void test_step_factor_clamps_to_8_20(void) {
    TEST_ASSERT_EQUAL_FLOAT(8.0f, dsp_step_factor(8.0f, -1));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, dsp_step_factor(20.0f, +1));
    TEST_ASSERT_EQUAL_FLOAT(11.0f, dsp_step_factor(10.0f, +1));
}
```

- [ ] **Step 3: テストが失敗することを確認する**

Run: `pio test -e native`
Expected: FAIL（`dsp_*` が未定義）

- [ ] **Step 4: `lib/dsp/dsp.c` に上記6関数を実装する**

線形補間でストレッチする。他は素直に実装する。音速は 343 m/s の定数で定義する。

- [ ] **Step 5: テストが通ることを確認する**

Run: `pio test -e native`
Expected: 8 tests PASS

- [ ] **Step 6: Commit**

```bash
git add platformio.ini src lib test
git commit -m "feat: add host-testable DSP library"
```

### Task 2: ステージ0 — マイク録音の検証（ゲート）

**Files:**
- Create: `src/stage0_capture.c`（`app_main` を差し替えるビルドフラグ `STAGE0`）
- Create: `tools/spectrum.py`

**Interfaces:**
- Produces: `bool stage0_capture_dump(uint32_t fs_hz, uint32_t ms)`（シリアルにバイナリで1ch分のPCMを出力）
- Produces: `tools/spectrum.py <dump.bin> <fs_hz>` : 40kHz付近のピーク周波数と、ノイズフロアからの高さ(dB)を表示する。

- [ ] **Step 1: 2台目の送信器（または別の超音波源）を40kHzで鳴らし、マイクの正面30cmに置く**

- [ ] **Step 2: `stage0_capture_dump` を実装し、マイクをESP32-S3のI2S（PDM）に接続して 200kHz 以上で取り込む**

200kHzに届かなければ、PDM生ビット列をI2S標準モードで取り込んでソフトでデシメーションする。

- [ ] **Step 3: `tools/spectrum.py` を実装し、ダンプを読んでFFTにかけ、40kHz付近のピークを出力する**

- [ ] **Step 4: 実行して結果を確認する**

Run: `python3 tools/spectrum.py dump.bin 200000`
Expected: ピークが 40kHz ±1kHz にあり、ノイズフロアより 20dB 以上高い

- [ ] **Step 5: ゲート判定を `design.md` の末尾に記録する**

通れば PDM 経路で続行。通らなければ、設計書の代替（生ビット取り込み → 圧電受波器 + PCM1808 → Teensy）を順に試し、採用した経路を記録する。

- [ ] **Step 6: Commit**

```bash
git add src/stage0_capture.c tools design.md
git commit -m "feat: stage0 microphone capture check"
```

### Task 3: 送信バースト

**Files:**
- Create: `src/tx.c`、`src/tx.h`

**Interfaces:**
- Consumes: なし
- Produces: `void tx_init(int gpio_a, int gpio_b)`、`void tx_burst(uint8_t cycles)`（2本のGPIOで逆相を出力し、40kHzのバーストを `cycles` 周期出す）

- [ ] **Step 1: `tx_init` と `tx_burst` を実装する**

`MCPWM` または `RMT` で逆相の40kHz（周期25us）を生成する。`cycles` は 8〜10 に丸める。

- [ ] **Step 2: ロジックアナライザまたはオシロで波形を確認する**

Run: `pio run -e esp32-s3 -t upload`（`tx_burst(10)` を250msごとに呼ぶテスト用 `app_main`）
Expected: 40kHz ±0.5kHz のパルスが10周期、約4Hzで出る

- [ ] **Step 3: Commit**

```bash
git add src/tx.c src/tx.h
git commit -m "feat: add 40kHz transmit burst"
```

### Task 4: 同期録音と距離の確認

**Files:**
- Create: `src/capture.c`、`src/capture.h`

**Interfaces:**
- Consumes: Task 1 の `dsp_apply_blanking`、`dsp_delay_to_distance_mm`、Task 3 の `tx_burst`
- Produces: `size_t capture_window(int16_t *left, int16_t *right, size_t cap)` : 送信と同時に約20ms録音し、ブランキング済みの左右サンプルを返す。サンプル数は `fs_hz` × 20ms。

- [ ] **Step 1: `capture_window` を実装する**

`tx_burst` の開始と録音開始を同じタイミングにし、`dsp_apply_blanking` で先頭を落とす。`blank_us` は初期値 300us とし、実機で調整する。

- [ ] **Step 2: 壁まで1.0mの位置で、最初に大きなエコーが来る時刻を確認する**

Run: テスト用 `app_main` が、最大振幅のサンプル位置から `dsp_delay_to_distance_mm` で距離を計算してシリアルに出す
Expected: 1000mm ±50mm

- [ ] **Step 3: 0.5m と 2.0m でも確認し、結果を `design.md` の末尾に記録する**

- [ ] **Step 4: Commit**

```bash
git add src/capture.c src/capture.h design.md
git commit -m "feat: add synchronized capture window"
```

### Task 5: 時間伸長再生

**Files:**
- Create: `src/playback.c`、`src/playback.h`
- Modify: `src/main.c`（約4Hzのループ: 送信 → 録音 → 伸長 → リミッタ → 再生）

**Interfaces:**
- Consumes: Task 1 の `dsp_time_stretch`、`dsp_limit_peak`、`dsp_interleave_lr`、Task 4 の `capture_window`
- Produces: `void playback_start(uint32_t fs_hz)`、`void playback_write(const int16_t *interleaved_lr, size_t frames)`（PCM5102A へI2Sで出力）

- [ ] **Step 1: 再生側を実装し、`main.c` のループを組む**

再生サンプルレートは44.1kHzで固定し、`dsp_time_stretch` の出力を再生用に使う。リミッタの `ceiling` は全スケールの25%（8192）を初期値とする。

- [ ] **Step 2: 音の確認（耳で）**

Run: `pio run -e esp32-s3 -t upload`
Expected: 手で超音波センサ正面を覆うと、エコーの音が約4Hzで鳴り、聞こえ方が変わる

- [ ] **Step 3: 方向確認**

壁の左側だけにセンサを向けると、左の耳で大きく聞こえることを確認し、結果を `design.md` に記録する。

- [ ] **Step 4: Commit**

```bash
git add src
git commit -m "feat: time-stretched binaural playback"
```

### Task 6: 倍率の調整

**Files:**
- Modify: `src/main.c`
- Create: `src/buttons.c`、`src/buttons.h`

**Interfaces:**
- Consumes: Task 1 の `dsp_step_factor`、Task 5 のループ
- Produces: `int buttons_poll(void)` : +1（上ボタン）、-1（下ボタン）、0（なし）。

- [ ] **Step 1: `buttons_poll` を実装し、押した結果を `dsp_step_factor` に反映する**

倍率が変わるのはループの境界だけにする（再生の途中では変えない）。

- [ ] **Step 2: 動作確認**

Run: アップロードして、ボタンで8〜20の間を往復する
Expected: 倍率が8未満・20超に出ず、音の高さが変わる

- [ ] **Step 3: Commit**

```bash
git add src
git commit -m "feat: adjustable stretch factor"
```

### Task 7: 聞き取り試験とウェアラブル化

**Files:**
- Create: `docs/listening-trials.md`

- [ ] **Step 1: ブレッドボード上の構成を、ヘッドバンドに装着できる形にまとめる（電池はLiPo、出力は骨伝導かオープンイヤー）**

- [ ] **Step 2: 聞き取り試験の手順を `docs/listening-trials.md` に書く**

目隠しで壁と物の有無を10回ずつ当てる試験を、1日30分×数日行う。毎回の正解率を記録する。

- [ ] **Step 3: 試験を行い、正解率の推移を記録する**

Expected: 練習を重ねると、壁の有無の正解率が上がっていく（成功の目安は設計書のとおり）

- [ ] **Step 4: Commit**

```bash
git add docs
git commit -m "docs: add listening trials"
```
