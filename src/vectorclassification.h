#pragma once

#include <cstdint>
#include <span>
void NaiveAdvanceString(const char *&start, const char *end) {
  const char *s = start;
  for (; s < end; s++) {
    if (*s == '<' || *s == '&' || *s == '\r' || *s == '\0') {
      start = s;
      return;
    }
  }
  start = end;
}

#if defined(__aarch64__)
#include <arm_neon.h>

struct neon_match64 {
  neon_match64(const char *start, const char *end) : start(start), end(end) {
    low_nibble_mask = {0, 0, 0, 0, 0, 0, 0x26, 0, 0, 0, 0, 0, 0x3c, 0xd, 0, 0};
    bit_mask = {0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
    v0f = vmovq_n_u8(0xf);
    offset = 0;
    if (start + 64 >= end) {
      carefulUpdate();
    } else {
      update();
    }
  }
  const char *get() const { return start + offset; }
  // Call consume after you have called advance() to move on.
  void consume() {
    offset++;
    matches >>= 1;
  }

  // move to the next match, when starting out, it moves you to the first value
  // (if there is one), otherwise it moves you to the next value. If you are
  // already at a match, you will remain at that match. You need to call consume
  // to move on. It returns false if there are no more matches.
  bool advance() {
    while (matches == 0) {
      start += 64;
      if (start >= end) {
        return false;
      }
      if (start + 64 >= end) {
        carefulUpdate();
        if (matches == 0) {
          return false;
        }
      } else {
        update();
      }
    }
    int off = __builtin_ctzll(matches);
    matches >>= off;
    offset += off;
    return true;
  }

private:
  inline void carefulUpdate() {
    uint8_t buffer[64]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    memcpy(buffer, start, end - start);
    update(buffer);
  }
  inline void update() { update((const uint8_t *)start); }
  inline void update(const uint8_t *buffer) {
    uint8x16_t data1 = vld1q_u8(buffer);
    uint8x16_t data2 = vld1q_u8(buffer + 16);
    uint8x16_t data3 = vld1q_u8(buffer + 32);
    uint8x16_t data4 = vld1q_u8(buffer + 48);

    uint8x16_t lowpart1 = vqtbl1q_u8(low_nibble_mask, data1 & v0f);
    uint8x16_t lowpart2 = vqtbl1q_u8(low_nibble_mask, data2 & v0f);
    uint8x16_t lowpart3 = vqtbl1q_u8(low_nibble_mask, data3 & v0f);
    uint8x16_t lowpart4 = vqtbl1q_u8(low_nibble_mask, data4 & v0f);

    uint8x16_t matchesones1 = vceqq_u8(lowpart1, data1);
    uint8x16_t matchesones2 = vceqq_u8(lowpart2, data2);
    uint8x16_t matchesones3 = vceqq_u8(lowpart3, data3);
    uint8x16_t matchesones4 = vceqq_u8(lowpart4, data4);

    uint8x16_t sum0 =
        vpaddq_u8(matchesones1 & bit_mask, matchesones2 & bit_mask);
    uint8x16_t sum1 =
        vpaddq_u8(matchesones3 & bit_mask, matchesones4 & bit_mask);
    sum0 = vpaddq_u8(sum0, sum1);
    sum0 = vpaddq_u8(sum0, sum0);
    matches = vgetq_lane_u64(vreinterpretq_u64_u8(sum0), 0);

    offset = 0;
  }

  const char *start;
  const char *end;
  size_t offset;
  uint64_t matches{};
  uint8x16_t low_nibble_mask;
  uint8x16_t v0f;
  uint8x16_t bit_mask;
};

struct neon_match64_r {
  neon_match64_r(const char *start, const char *end) : start(start), end(end) {
    low_nibble_mask = {0, 0, 0, 0, 0, 0, 0x26, 0, 0, 0, 0, 0, 0x3c, 0xd, 0, 0};
    bit_mask = {0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
    v0f = vmovq_n_u8(0xf);
    offset = 0;
    if (start + 64 >= end) {
      carefulUpdate();
    } else {
      update();
    }
  }
  const char *get() const { return start + offset; }
  // Call consume after you have called advance() to move on.
  void consume() {
    offset++;
    matches <<= 1;
  }

  // move to the next match, when starting out, it moves you to the first value
  // (if there is one), otherwise it moves you to the next value. If you are
  // already at a match, you will remain at that match. You need to call consume
  // to move on. It returns false if there are no more matches.
  bool advance() {
    while (matches == 0) {
      start += 64;
      if (start >= end) {
        return false;
      }
      if (start + 64 >= end) {
        carefulUpdate();
        if (matches == 0) {
          return false;
        }
      } else {
        update();
      }
    }
    int off = __builtin_clzll(matches);
    matches <<= off;
    offset += off;
    return true;
  }

private:
  inline void carefulUpdate() {
    uint8_t buffer[64]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    memcpy(buffer, start, end - start);
    update(buffer);
  }
  inline void update() { update((const uint8_t *)start); }
  inline void update(const uint8_t *buffer) {
    uint8x16_t data1 = vld1q_u8(buffer);
    uint8x16_t data2 = vld1q_u8(buffer + 16);
    uint8x16_t data3 = vld1q_u8(buffer + 32);
    uint8x16_t data4 = vld1q_u8(buffer + 48);

    uint8x16_t lowpart1 = vqtbl1q_u8(low_nibble_mask, vandq_u8(data1, v0f));
    uint8x16_t lowpart2 = vqtbl1q_u8(low_nibble_mask, vandq_u8(data2, v0f));
    uint8x16_t lowpart3 = vqtbl1q_u8(low_nibble_mask, vandq_u8(data3, v0f));
    uint8x16_t lowpart4 = vqtbl1q_u8(low_nibble_mask, vandq_u8(data4, v0f));

    uint8x16_t matchesones1 = vceqq_u8(lowpart1, data1);
    uint8x16_t matchesones2 = vceqq_u8(lowpart2, data2);
    uint8x16_t matchesones3 = vceqq_u8(lowpart3, data3);
    uint8x16_t matchesones4 = vceqq_u8(lowpart4, data4);

    uint8x16_t sum0 =
        vpaddq_u8(matchesones1 & bit_mask, matchesones2 & bit_mask);
    uint8x16_t sum1 =
        vpaddq_u8(matchesones3 & bit_mask, matchesones4 & bit_mask);
    sum0 = vpaddq_u8(sum0, sum1);
    sum0 = vpaddq_u8(sum0, sum0);
    matches = vgetq_lane_u64(vreinterpretq_u64_u8(sum0), 0);
    matches = __builtin_bitreverse64(matches);
    offset = 0;
  }

  const char *start;
  const char *end;
  size_t offset;
  uint64_t matches{};
  uint8x16_t low_nibble_mask;
  uint8x16_t v0f;
  uint8x16_t bit_mask;
};

void AdvanceStringWebKit(const char *&mstart, const char *end) {
  const char *start = mstart;
  uint8x16_t low_nibble_mask = {0, 0, 0, 0, 0,    0,   0x26, 0,
                                0, 0, 0, 0, 0x3c, 0xd, 0,    0};
  uint8x16_t v0f = vmovq_n_u8(0xf);
  uint8x16_t bit_mask = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  static constexpr auto stride = 16;
  for (; start + (stride - 1) < end; start += stride) {
    uint8x16_t data = vld1q_u8(reinterpret_cast<const uint8_t *>(start));
    uint8x16_t lowpart = vqtbl1q_u8(low_nibble_mask, vandq_u8(data, v0f));
    uint8x16_t matchesones = vceqq_u8(lowpart, data);
    if (vmaxvq_u32(vreinterpretq_u32_u8(matchesones)) != 0) {
      uint8x16_t matches = vornq_u8(bit_mask, matchesones);
      int m = vminvq_u8(matches);
      start += m;
      mstart = start;
      return;
    }
  }
  for (; start < end; start++) {
    if (*start == '<' || *start == '&' || *start == '\r' || *start == '\0') {
      return;
    }
  }
  mstart = start;
}

void AdvanceStringChromium(const char *&mstart, const char *end) {
  const char *start = mstart;
  uint8x16_t low_nibble_mask = {0, 0, 0, 0, 0,    0,   0x26, 0,
                                0, 0, 0, 0, 0x3c, 0xd, 0,    0};
  uint8x16_t v0f = vmovq_n_u8(0xf);
  static constexpr auto stride = 16;
  for (; start + (stride - 1) < end; start += stride) {
    uint8x16_t data = vld1q_u8(reinterpret_cast<const uint8_t *>(start));
    uint8x16_t lowpart = vqtbl1q_u8(low_nibble_mask, vandq_u8(data, v0f));
    uint8x16_t matchesones = vceqq_u8(lowpart, data);
    const uint8x8_t res = vshrn_n_u16(matchesones, 4);
    const uint64_t matches = vget_lane_u64(vreinterpret_u64_u8(res), 0);
    if (matches != 0) {
      start += __builtin_ctzll(matches) >> 2;
      mstart = start;
      return;
    }
  }
  for (; start < end; start++) {
    if (*start == '<' || *start == '&' || *start == '\r' || *start == '\0') {
      return;
    }
  }
  mstart = start;
}
#endif