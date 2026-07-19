// RUN: %clang_cc1 -triple ezh-none-elf -fsyntax-only -verify %s
//
// Range checks for the constant (ImmArg) operands of the EZH builtins. An
// out-of-range literal would otherwise silently truncate to the instruction's
// immediate field width (e.g. int_trigger(0x1000005) raises event 5).

void t_int_trigger(void) {
  __builtin_ezh_int_trigger(1);          // ok
  __builtin_ezh_int_trigger(0xFFFFFF);   // ok (field max)
  __builtin_ezh_int_trigger(0);          // expected-error {{argument value 0 is outside the valid range [1, 16777215]}}
  __builtin_ezh_int_trigger(0x1000000);  // expected-error {{argument value 16777216 is outside the valid range [1, 16777215]}}
}

void t_synch_all_to_beat(void) {
  __builtin_ezh_synch_all_to_beat(1);    // ok
  __builtin_ezh_synch_all_to_beat(2);    // expected-error {{argument value 2 is outside the valid range [0, 1]}}
}

void t_heart_rythm_imm(void) {
  __builtin_ezh_heart_rythm_imm(65535);  // ok
  __builtin_ezh_heart_rythm_imm(0x12345); // expected-error {{argument value 74565 is outside the valid range [0, 65535]}}
}

void t_modify_gpo_byte(void) {
  __builtin_ezh_modify_gpo_byte(0xFF, 0, 0); // ok
  __builtin_ezh_modify_gpo_byte(0x1FF, 0, 256); // expected-error {{argument value 511 is outside the valid range [0, 255]}} expected-error {{argument value 256 is outside the valid range [0, 255]}}
}

void t_acc_vectored_hold(void *table) {
  (void)__builtin_ezh_acc_vectored_hold(table, 0xFF); // ok
  (void)__builtin_ezh_acc_vectored_hold(table, 0x105); // expected-error {{argument value 261 is outside the valid range [0, 255]}}
}

void t_bit_positions(void) {
  __builtin_ezh_gpd_drive_low(31);  // ok
  __builtin_ezh_gpd_drive_low(32);  // expected-error {{argument value 32 is outside the valid range [0, 31]}}
  __builtin_ezh_gpd_release(40);    // expected-error {{argument value 40 is outside the valid range [0, 31]}}
  __builtin_ezh_cfm_bset(31);       // ok
  __builtin_ezh_cfm_bset(32);       // expected-error {{argument value 32 is outside the valid range [0, 31]}}
  __builtin_ezh_cfm_bclr(99);       // expected-error {{argument value 99 is outside the valid range [0, 31]}}
}
