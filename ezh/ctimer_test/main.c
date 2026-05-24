/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "devices/MIMXRT595S/MIMXRT595S_dsp.h"
#include "devices/MIMXRT595S/drivers/fsl_clock.h"
#include "devices/MIMXRT595S/drivers/fsl_ctimer.h"
#include "devices/MIMXRT595S/drivers/fsl_inputmux.h"
#include "ezh.h"
#include "ezh_test.h"

static volatile uint32_t g_interrupt_counter = 0;

static void ctimer_callback(uint32_t flags) {
  (void)flags;
  g_interrupt_counter++;
}

void CTIMER2_DriverIRQHandler(void);

// Implement vector2 to call CTIMER2 IRQ handler
void __attribute__((used)) vector2() { CTIMER2_DriverIRQHandler(); }

// Dummy implementation of SDK_DelayAtLeastUs to satisfy the linker.
// Now we also use it for the 5-second busy loop.
void SDK_DelayAtLeastUs(uint32_t delayTime_us, uint32_t coreClock_Hz) {
  (void)coreClock_Hz;
  volatile uint32_t count = delayTime_us * 5U;
  while (count > 0U) {
    count--;
  }
}

#include <string.h>

int main(void) {
  exc_signal = 0x11111111;
  ctimer_config_t config;
  ctimer_match_config_t matchConfig;

  // 1. Enable bit slice 2 to trigger on rising edge (and enable all slices)
  ezh_write_cfm(BS7(BS_0) | BS6(BS_0) | BS5(BS_0) | BS4(BS_0) | BS3(BS_0) |
                BS2(BS_RISE) |                 // Trigger on rising edge
                BS1(BS_0) | BS0(BS_0) | 0xFF); // Enable all interrupts
  // 2. Initialize INPUTMUX and attach CTIMER2 IRQ to SMARTDMA Input 2
  INPUTMUX_Init(INPUTMUX);
  INPUTMUX_AttachSignal(INPUTMUX, 2, kINPUTMUX_Ctimer2IrqToSmartDmaInput);

  // 3. Attach MAIN_CLK to CTIMER2
  CLOCK_AttachClk(kMAIN_CLK_to_CTIMER2);

  // 4. Initialize CTIMER2
  CTIMER_GetDefaultConfig(&config);

  // 96 MHz clock, 1 tick per second
  config.prescale = 95999999U;
  CTIMER_Init(CTIMER2, &config);

  // 5. Register CTIMER Callback
  ctimer_callback_t cb = ctimer_callback;
  CTIMER_RegisterCallBack(CTIMER2, &cb, kCTIMER_SingleCallback);

  // 6. Setup Match 0 to trigger interrupt and reset counter at 1 second
  matchConfig.enableCounterReset = true;
  matchConfig.enableCounterStop = false;
  matchConfig.matchValue = 1U; // Trigger when TC reaches 1 (1 second)
  matchConfig.outControl = kCTIMER_Output_NoAction;
  matchConfig.outPinInitState = false;
  matchConfig.enableInterrupt = true;
  CTIMER_SetupMatch(CTIMER2, kCTIMER_Match_0, &matchConfig);

  // 7. Start the timer
  CTIMER_StartTimer(CTIMER2);

  // 8. Busy loop with memset/memcmp to test register preservation
  // We will run for a fixed number of iterations.
  // At ~2000 cycles per iteration (due to AHB latency in memset/memcmp),
  // 250,000 iterations take ~5 seconds, allowing us to capture 5 interrupts.
  uint8_t buf1[64];
  uint8_t buf2[64];

  while (g_interrupt_counter < 5U) {
    memset(buf1, 0x5A, sizeof(buf1));
    memset(buf2, 0x5A, sizeof(buf2));
    if (memcmp(buf1, buf2, sizeof(buf1)) != 0) {
      // Register or memory corruption detected!
      exc_signal = 0xDEADBEEF;
      return 0xDEADBEEF;
    }
  }

  CTIMER_StopTimer(CTIMER2);

  // 9. Return the interrupt counter value
  uint32_t final_count = g_interrupt_counter;
  exc_signal = final_count;
  return final_count;
}
