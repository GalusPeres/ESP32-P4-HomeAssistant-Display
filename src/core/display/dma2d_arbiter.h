#ifndef DMA2D_ARBITER_H
#define DMA2D_ARBITER_H

#include <stdint.h>

// On the ESP32-P4 the PPA (display rotation) and the hardware JPEG decoder
// (media cover art) share the same 2D-DMA channel pool and interrupt path. When
// both overlap in time, the IDF driver (release/v5.5) can lose a PPA
// transaction: it never starts, the engine semaphore is never given, and
// ppa_unregister_client keeps failing with err=259. That is the "PPA wedged"
// state, which only a reboot recovered from; ppa_core.c carries a known TODO
// for it ("need a way to force end"). Both field wedges correlated with cover
// decode activity, and the stable 8-inch device practically never rotates
// during a decode because of its high PPA threshold. This mutex makes the
// non-overlap explicit instead of leaving it to chance.
namespace dma2d_arbiter {

// true = the lock is held and unlock() is mandatory. false = timed out.
bool lock(uint32_t timeout_ms);
void unlock();

}  // namespace dma2d_arbiter

// RAII guard for paths with several return points.
class Dma2dArbiterGuard {
 public:
  explicit Dma2dArbiterGuard(uint32_t timeout_ms)
      : locked_(dma2d_arbiter::lock(timeout_ms)) {}
  ~Dma2dArbiterGuard() {
    if (locked_) {
      dma2d_arbiter::unlock();
    }
  }
  Dma2dArbiterGuard(const Dma2dArbiterGuard&) = delete;
  Dma2dArbiterGuard& operator=(const Dma2dArbiterGuard&) = delete;
  bool locked() const { return locked_; }
  // Transfers submitted in non-blocking mode still own their buffers after a
  // caller-side timeout. Detaching deliberately keeps the shared DMA2D lease
  // held so no JPEG/PPA client can reuse those resources before a safe reset.
  bool detach() {
    const bool was_locked = locked_;
    locked_ = false;
    return was_locked;
  }

 private:
  bool locked_;
};

#endif  // DMA2D_ARBITER_H
