#ifndef DMA2D_ARBITER_H
#define DMA2D_ARBITER_H

#include <stdint.h>

// PPA (Display-Rotation) und HW-JPEG-Decoder (Media-Cover) teilen sich auf
// dem ESP32-P4 denselben 2D-DMA-Kanalpool samt Interrupt-Pfad. Ueberlappen
// sich beide zeitlich, kann der IDF-Treiber (release/v5.5) eine PPA-
// Transaktion verlieren: sie startet nie, die Engine-Semaphore wird nie
// gegeben, ppa_unregister_client verweigert dauerhaft mit err=259 — das ist
// der "PPA VERKLEMMT"-Zustand, aus dem nur ein Reboot half (bekanntes TODO
// in ppa_core.c: "need a way to force end"). Beide Feld-Wedges korrelierten
// mit Cover-Decode-Aktivitaet; das stabile 8-Zoll-Geraet rotiert dank hoher
// PPA-Schwelle praktisch nie waehrend eines Decodes. Dieser Mutex stellt die
// Nicht-Ueberlappung explizit sicher, statt sie dem Zufall zu ueberlassen.
namespace dma2d_arbiter {

// true = Lock gehalten, unlock() ist Pflicht. false = Timeout abgelaufen.
bool lock(uint32_t timeout_ms);
void unlock();

}  // namespace dma2d_arbiter

// RAII-Guard fuer Pfade mit mehreren Return-Stellen.
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
