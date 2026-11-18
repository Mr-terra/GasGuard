# Test case matrix

Each requirement is tested in five categories (`write-tests` skill):

- **Positive**: the action happens when its condition is met.
- **Negative**: nothing happens when the condition is not met, or a bad
  argument is refused and changes nothing.
- **False positive**: a near miss does not trigger. This guards against
  nuisance alarms (hazard H2).
- **False negative**: a real case is not missed. This guards against a
  missed alarm (hazard H1).
- **Limits**: exact boundary values on both sides.

Each cell names one or two representative tests, with the `test_` prefix
dropped. `docs/traceability.md` lists every test of a requirement.

"n/a" means the category does not apply to that requirement, and the cell
says why. "Board" means the case can only be checked on the hardware; the
result is in `docs/test-report.md`. Updated 2026-11-18.

## Measurement

| Requirement | Positive | Negative | False positive | False negative | Limits |
|---|---|---|---|---|---|
| REQ-FUN-001 sampling every 100 ms | `firmware_each_sample_reads_both_gas_channels` | `hal_adc_read_rejects_channel_out_of_range` | n/a: sampling has no trigger | `firmware_each_sample_reads_both_gas_channels` (no channel skipped) | `scheduler_task_is_due_at_its_period` (99 ms not due, 100 ms due) |
| REQ-FUN-002 median of 3, then 8-sample average | `filter_averages_the_window`, `filter_median_ignores_the_order_of_the_three_samples` | `filter_rejected_sample_leaves_the_average_unchanged` | `filter_removes_a_single_spike`, `filter_largest_spike_below_the_rails_is_removed`, `firmware_spike_that_ends_a_stuck_fault_raises_no_alarm`, `firmware_spike_right_after_a_rail_reading_raises_no_alarm`, `firmware_spike_right_after_a_read_error_raises_no_alarm` | `filter_step_reaches_its_full_value_on_the_ninth_sample`, `filter_removes_a_single_dip_too`, `firmware_gas_during_range_recovery_is_ignored_then_seen_at_recovery` | `filter_two_samples_in_a_row_are_not_removed`, `filter_full_scale_window_does_not_overflow`, `filter_partial_window_averages_the_samples_received` |
| REQ-FUN-003 linear calibration | `calib_is_linear_above_zero` | `calib_out_of_range_raw_leaves_the_output_unchanged` | `calib_below_zero_reads_zero_not_negative` | `calib_large_gain_saturates_and_never_wraps_negative` | `calib_one_lsb_each_side_of_the_zero`, `calib_full_scale_raw_is_accepted` |
| REQ-FUN-004 temperature compensation | `calib_temperature_correction_applied_when_valid` | `calib_temperature_ignored_when_sensor_invalid` | `calib_zero_coefficient_never_moves_the_gas_reading` | `firmware_faulty_temperature_never_holds_back_an_alarm` | `firmware_temperature_valid_range_ends_exactly_at_217_and_769`, `calib_temperature_correction_stops_exactly_at_the_adc_ends` |
| REQ-FUN-005 maximum of valid channels | `channel_select_takes_the_maximum_of_valid_channels` | `channel_select_rejected_call_leaves_the_outputs_unchanged` | `channel_select_ignores_faulty_channel` | `channel_select_gas_on_the_second_channel_is_seen_when_the_first_is_faulty` | `channel_select_equal_readings_and_one_channel`, `channel_select_largest_value_is_kept_whole` |
| REQ-FUN-006 simulation build | `firmware_mq5_channel_reads_the_mapped_adc_input`; `make sim-test` runs the whole firmware suite in the simulation build | n/a: build option | n/a: build option | n/a: build option | n/a: build option |

## Alarms

| Requirement | Positive | Negative | False positive | False negative | Limits |
|---|---|---|---|---|---|
| REQ-ALM-001 ALARM_1 after 3 samples | `detector_alarm_1_after_three_samples_at_threshold` | `detector_gas_one_unit_below_al1_never_alarms` | `detector_single_spike_does_not_alarm`, `detector_two_samples_then_a_dip_restart_the_count` | `detector_two_samples_then_a_dip_restart_the_count` (alarm on the third), `firmware_faulty_temperature_never_holds_back_an_alarm` | `detector_alarm_1_after_three_samples_at_threshold` (2 samples, 3 samples), `detector_largest_concentration_alarms` |
| REQ-ALM-002 ALARM_2 after 3 samples | `detector_gas_above_al2_from_normal_reaches_alarm_2_in_three_samples`, `detector_alarm_2_after_three_samples_at_threshold` | `detector_from_normal_two_samples_at_al2_then_clean_air_stay_normal` | `detector_two_samples_at_al2_do_not_give_alarm_2`, `detector_from_normal_two_samples_at_al2_then_one_at_al1_give_alarm_1` | `detector_gas_above_al2_from_normal_reaches_alarm_2_in_three_samples` (relay off and latched on the direct path) | `detector_from_normal_one_unit_below_al2_gives_alarm_1_only`, 2 and 3 samples |
| REQ-ALM-003 ALARM_1 clears after 10 samples | `detector_alarm_1_clears_after_ten_samples_below_hysteresis` | `detector_alarm_1_does_not_clear_while_gas_is_still_above_al1` | `detector_hysteresis_band_does_not_chatter` | `detector_clear_count_restarts_when_gas_returns` | `detector_clear_level_itself_does_not_count_as_below` (80 holds, 79 counts), 9 and 10 samples |
| REQ-ALM-004 ALARM_2 latched | `detector_alarm_2_reset_accepted_when_gas_gone` | `detector_reset_request_outside_alarm_2_changes_nothing` | `detector_alarm_2_reset_refused_while_gas_present` | `detector_gas_back_after_an_alarm_2_reset_alarms_again`, `detector_alarm_2_is_latched_when_gas_drops` | `detector_alarm_2_reset_at_the_clear_level_is_refused_one_below_accepted` |
| REQ-ALM-005 threshold validation | `detector_accepts_the_widest_valid_thresholds` | `config_validate_negative_thresholds_are_rejected` | `config_validate_accepts_the_widest_valid_settings` (no valid setting refused) | `detector_rejected_thresholds_keep_the_ones_in_use` | `config_validate_limits_al2_to_600`, `config_validate_al2_one_step_above_al1_is_accepted`, `config_validate_hysteresis_one_below_al1_is_accepted` |
| REQ-ALM-006 inhibited in WARMUP | `detector_starts_in_warmup_with_alarms_inhibited` | `detector_end_warmup_rejects_null_and_acts_once` | `sc13_gas_during_warmup_raises_no_alarm` | `detector_gas_present_at_the_end_of_warmup_alarms_three_samples_later` | `detector_gas_present_at_the_end_of_warmup_alarms_three_samples_later` (2 samples, 3 samples) |

## Safety

| Requirement | Positive | Negative | False positive | False negative | Limits |
|---|---|---|---|---|---|
| REQ-SAF-001 relay normally energised | `detector_relay_is_energised_during_warmup`, `detector_relay_is_energised_again_after_alarm_1_clears` | `detector_relay_released_on_alarm` | `detector_relay_is_energised_again_after_alarm_1_clears` | `sc09_both_sensors_unplugged_is_fault_with_relay_off`, `firmware_gas_ending_a_stuck_fault_keeps_the_relay_off` | Board: D5 at reset (test report, 2026-11-17) |
| REQ-SAF-002 open or short circuit, or failed reads | `diag_open_circuit_faulty_after_five_samples`, `diag_read_failures_faulty_after_five_samples`, `firmware_repeated_read_errors_on_one_channel_are_a_logged_channel_fault`, `firmware_read_error_fault_recovers_after_sixty_seconds_of_good_reads`, `firmware_read_errors_on_both_channels_latch_the_fault_at_once` | `diag_rejected_range_input_changes_nothing`, `diag_read_failed_rejects_null` | `diag_valid_sample_resets_the_count`, `diag_valid_samples_in_a_healthy_channel_never_fault`, `diag_four_read_failures_then_a_valid_sample_are_no_fault`, `firmware_four_read_errors_are_no_channel_fault` | `diag_open_and_short_samples_count_together`, `diag_range_recovery_restarts_when_the_fault_comes_back`, `diag_read_failures_and_rail_readings_count_together`, `diag_read_failure_during_recovery_restarts_it` | `diag_open_circuit_limit_is_raw_five`, `diag_short_circuit_limit_is_raw_1018`, 599 and 600 recovery samples, 4 and 5 failed reads |
| REQ-SAF-003 stuck sensor | `diag_stuck_value_faulty_after_ten_minutes` | `diag_stuck_check_rejects_bad_arguments` | `diag_moving_value_is_not_stuck`, `diag_stuck_check_is_inhibited_during_warmup` | `diag_one_lsb_of_noise_does_not_hide_a_stuck_sensor`, `diag_stuck_sensor_through_the_warmup_is_caught_ten_minutes_after_it` | `diag_stuck_sensor_recovers_on_two_lsb_not_on_one`, `diag_stuck_window_survives_the_clock_wrap` |
| REQ-SAF-004 bandgap every 60 s | `firmware_bandgap_out_of_tolerance_raises_fault` | `firmware_bandgap_within_tolerance_logs_nothing` | `firmware_bandgap_at_the_tolerance_edges_raises_no_fault` | `firmware_failed_bandgap_read_is_logged_as_zero_and_raises_fault`, `diag_bandgap_at_the_adc_ends_is_a_fault` | `diag_bandgap_tolerance_ends_at_22_lsb`, `firmware_bandgap_is_measured_after_60_s_not_before` |
| REQ-SAF-005 watchdog 500 ms | `watchdog_is_kicked_only_when_every_task_has_run` | `watchdog_is_not_kicked_when_no_task_has_run` | `scheduler_late_task_runs_once_not_in_a_burst` | `tasks_done_in_the_last_cycle_do_not_allow_a_kick_in_this_one` | `scheduler_accepts_one_to_eight_tasks`, `scheduler_task_stays_on_time_across_the_clock_wrap`; board: timeout with the bench image (`#h`) |
| REQ-SAF-006 reset cause | `firmware_watchdog_and_brown_out_resets_are_logged_with_their_cause` | `firmware_boot_logs_the_reset_cause` (power-on) | n/a: every cause is logged | `firmware_every_reset_cause_is_logged_even_the_unknown_one` | Every value of `hal_reset_cause_t` |
| REQ-SAF-007 FAULT | `firmware_both_channels_short_raise_fault_and_drop_the_relay` | `sc08_one_sensor_unplugged_is_channel_fault_only` | `detector_stuck_only_fault_ends_by_itself` | `detector_fault_during_warmup_is_not_hidden`, `detector_real_fault_during_a_stuck_fault_latches` | `firmware_stuck_channel_with_the_other_open_latches_the_fault` |
| REQ-SAF-008 overrun | `cycle_over_100_ms_is_an_overrun`, `three_consecutive_overruns_raise_fault` | `cycle_of_100_ms_is_not_an_overrun` | `a_normal_cycle_resets_the_overrun_count`, `two_overruns_in_a_row_are_not_yet_a_fault` | `overruns_past_three_keep_the_fault_and_never_wrap_the_count` | 100 and 101 ms, `overrun_is_measured_across_the_clock_wrap`; board: `#o` and `#f` |
| REQ-SAF-009 stack watermark | `firmware_stack_below_64_bytes_latches_fault` | `firmware_plenty_of_stack_never_faults` | `firmware_stack_below_64_bytes_latches_fault` (64 bytes: no fault) | `firmware_low_stack_during_warmup_is_a_fault`, `firmware_low_stack_during_maintenance_is_a_fault` | 64 and 63 bytes; board: value with the bench image |
| REQ-SAF-010 no dynamic memory, no recursion | `make lint`: cppcheck MISRA 17.2 (recursion), 21.3 (malloc and free), and a grep for `malloc`, `calloc`, `realloc` and `free` calls (fixed 2026-11-17, see below) | n/a: static rule | n/a: static rule | n/a: static rule | n/a: static rule |
| REQ-SAF-011 reset loop | `firmware_fourth_short_start_enters_fault_and_logs_it` | `firmware_sixty_seconds_after_warmup_clear_the_start_count` | `firmware_start_count_is_cleared_sixty_seconds_after_warmup_not_before` | `firmware_later_short_starts_stay_in_fault_and_log_each_start`, `firmware_corrupted_start_count_is_read_as_zero` | `firmware_start_count_stays_at_its_maximum_in_a_long_loop` |

## Operator interface

| Requirement | Positive | Negative | False positive | False negative | Limits |
|---|---|---|---|---|---|
| REQ-HMI-001 LCD lines | `lcd_rgb_write_line_zero_sets_cursor_and_sends_characters`, `hmi_format_lel_puts_the_tenth_after_the_point` | `lcd_rgb_write_line_rejects_bad_arguments` | `lcd_rgb_write_line_pads_the_row_with_spaces` (no stale text) | `lcd_rgb_write_line_reports_a_missing_display`, `firmware_missing_display_is_logged` | `lcd_rgb_write_line_of_exactly_16_characters_fills_the_row`, `..._of_17_characters_is_refused_and_sends_nothing` |
| REQ-HMI-002 backlight colours | `lcd_rgb_set_colour_writes_red_green_blue_registers` | `lcd_rgb_set_colour_reports_a_missing_backlight_chip` | Board: colour per state (the table is in `main.c`, outside the host build) | Board, as false positive | `lcd_rgb_set_colour_sends_the_extreme_levels_unchanged` |
| REQ-HMI-003 buzzer patterns | `hmi_alarm_1_pattern_is_1_hz`, `hmi_alarm_2_pattern_is_4_hz` | `hmi_quiet_states_keep_the_buzzer_off` | `hmi_quiet_states_keep_the_buzzer_off` | `hmi_fault_beeps_once_every_five_seconds_and_cannot_be_silenced` | `hmi_alarm_1_beep_lasts_exactly_half_a_second`, `..._125_ms`, `..._200_ms`, `hmi_patterns_keep_their_rhythm_across_the_clock_wrap` |
| REQ-HMI-004 short press silences | `firmware_short_press_silences_the_alarm_1_buzzer`, `hmi_new_alarm_2_episode_gets_its_own_60_s` | `hmi_press_after_the_60_s_of_alarm_2_has_no_effect`, `hmi_ack_outside_an_alarm_does_nothing` | `hmi_press_between_one_and_three_seconds_is_neither` | `firmware_short_presses_cannot_keep_alarm_2_quiet_past_60_s`, `hmi_pressing_every_ten_seconds_cannot_keep_alarm_2_quiet`, `hmi_alarm_1_silence_does_not_use_up_the_alarm_2_budget` | `hmi_short_press_ends_at_999_ms`, `hmi_alarm_2_silence_expires_after_60_seconds` (59.999 s, 60 s), `hmi_last_press_before_the_end_of_the_60_s_adds_nothing`, `hmi_alarm_2_silence_budget_across_the_clock_wrap`; a new 60 s after a passage through FAULT: `firmware_alarm_2_back_from_a_stuck_fault_gets_a_new_silence` |
| REQ-HMI-005 long press resets ALARM_2 | `firmware_long_press_in_alarm_2_resets_the_latch_when_gas_is_gone` | `firmware_long_press_in_alarm_1_neither_resets_nor_starts_the_test` | `sc07_long_press_refused_while_gas_present` | `hmi_press_held_for_three_seconds_is_a_long_press_fired_once` | 2999 and 3000 ms (same test) |
| REQ-HMI-006 functional test | `firmware_long_press_in_normal_runs_the_functional_test` | `firmware_long_press_in_alarm_1_neither_resets_nor_starts_the_test` | `firmware_long_press_in_fault_never_energises_the_relay` | `hmi_functional_test_releases_the_relay_for_exactly_one_step` | `hmi_functional_test_runs_until_its_last_millisecond` |
| REQ-HMI-007 debounce 30 ms | `button_press_is_accepted_after_exactly_30_ms` | `button_starts_released` | `button_press_shorter_than_30_ms_is_ignored`, `button_bounce_restarts_the_30_ms_window` | `button_short_release_glitch_does_not_end_a_held_press` | 29 and 30 ms, `button_window_is_correct_across_millisecond_wraparound` |
| REQ-HMI-008 touch enters MAINTENANCE, refused at or above AL1 | `firmware_touch_for_three_seconds_enters_maintenance` | `detector_touch_is_ignored_in_alarm_2_and_fault`, `detector_touch_with_gas_at_al1_is_ignored`, `detector_touch_refused_for_gas_is_not_kept_for_later`, `firmware_touch_refused_for_gas_and_still_held_never_enters_maintenance` | `firmware_touch_shorter_than_three_seconds_is_ignored` | `firmware_touch_during_an_alarm_does_not_hide_it`, `detector_touch_on_the_third_gas_sample_gives_the_alarm` | `firmware_touch_of_3_s_is_ignored_and_3_2_s_enters_maintenance` (to one 100 ms loop step), `detector_touch_with_gas_one_unit_below_al1_enters_maintenance` (AL1 refused, AL1 - 0.1 %LEL accepted), `detector_touch_on_a_dip_below_al1_while_gas_builds_enters_maintenance` (accepted residual risk, F36) |
| REQ-HMI-009 MAINTENANCE output | `firmware_gas_is_shown_during_maintenance_but_not_output` | `detector_maintenance_keeps_relay_and_ignores_alarms` | `detector_maintenance_keeps_relay_and_ignores_alarms` | `detector_gas_after_maintenance_alarms_three_samples_after_its_end` | n/a: no numeric limit (the 10 minutes are REQ-HMI-010) |
| REQ-HMI-010 MAINTENANCE ends after 10 min | `firmware_maintenance_ends_after_ten_minutes_not_before` | `detector_maintenance_holds_until_the_timer_expires` | `detector_maintenance_holds_until_the_timer_expires` | `firmware_touch_held_during_maintenance_does_not_extend_it` | 598 s and 602 s (same test) |

## Configuration and log

| Requirement | Positive | Negative | False positive | False negative | Limits |
|---|---|---|---|---|---|
| REQ-CFG-001 two copies with CRC | `config_store_writes_two_identical_copies` | `config_null_arguments_are_rejected` | `crc16_same_data_gives_the_same_crc` | `crc16_every_single_bit_error_is_detected`, `config_flipped_bit_in_a_crc_byte_is_caught`, `crc16_erased_eeprom_does_not_check_good` | `crc16_zero_length_ignores_the_data`, `crc16_check_vector_123456789` |
| REQ-CFG-002 first valid copy, defaults | `config_load_uses_copy_b_and_repairs_copy_a` | `config_failed_repair_keeps_the_valid_copy_in_use` | `config_copy_a_wins_when_both_copies_are_valid_but_differ` | `config_both_copies_damaged_give_the_defaults_not_a_half_copy` | `config_flipped_bit_in_each_value_byte_is_caught` |
| REQ-CFG-003 validation before use and storage | `config_validate_accepts_the_defaults` | `config_store_refuses_an_invalid_configuration` | `config_extreme_valid_values_survive_a_store_and_load` | `config_copy_with_a_valid_crc_and_warmup_out_of_range_is_not_used` | `config_extreme_valid_values_survive_a_store_and_load` |
| REQ-CFG-004 warm-up 120 s (30–600) | `config_defaults_match_the_requirements` | `config_validate_warmup_is_between_30_and_600_s` (29, 601) | n/a: a setting, no trigger | `firmware_start_count_clear_follows_the_stored_warmup` | 29, 30, 600, 601 s (same test) |
| REQ-LOG-001 events in a ring of 111 | `firmware_maintenance_end_logs_the_peak_of_each_channel` | `eventlog_refused_entry_writes_nothing` | `firmware_no_peak_is_logged_outside_maintenance` | `eventlog_failed_eeprom_write_is_reported_not_hidden`, `firmware_touch_right_after_maintenance_keeps_its_peaks` | `eventlog_head_wraps_to_zero_on_the_111th_append`, `eventlog_first_and_last_codes_are_accepted` |
| REQ-LOG-002 entry fields and CRC | `eventlog_append_stores_timestamp_code_and_argument` | `eventlog_unwritten_slot_is_reported_invalid` | `eventlog_zero_uptime_and_argument_are_a_valid_entry` | `eventlog_corrupted_crc_bytes_are_detected` | `eventlog_largest_uptime_and_argument_round_trip`, `eventlog_head_at_its_last_valid_value_is_kept` |
| REQ-LOG-003 written on events only | `eventlog_writes_only_when_an_event_is_appended` | `eventlog_refused_entry_writes_nothing` | `firmware_no_peak_is_logged_outside_maintenance` | n/a: covered by REQ-LOG-001 | `eventlog_append_at_the_ring_end_still_writes_twice` |
| REQ-LOG-004 `log` over UART | `cli_log_prints_each_written_slot_with_its_code_name` | `cli_unknown_or_too_long_command_is_refused` | `cli_command_that_starts_like_log_is_refused` | `cli_full_log_dumps_all_111_entries_then_the_end_line`, `cli_dump_resumes_when_the_transmit_buffer_is_full` | `cli_line_of_the_maximum_length_is_refused_and_does_not_overflow`, `cli_log_with_a_carriage_return_is_accepted` |
| REQ-LOG-005 `status` over UART | `cli_status_prints_the_last_snapshot` | `cli_status_with_extra_text_is_refused` | `cli_status_prints_a_negative_concentration_as_zero` | `cli_status_shows_the_latest_snapshot_not_an_older_one` | `cli_status_prints_the_largest_values_in_full` |

## Quality gates

REQ-NFR-001 to REQ-NFR-005 are verified by `make` targets, not by test
cases, so the five categories do not apply:

| Requirement | Verified by |
|---|---|
| REQ-NFR-001 host build without AVR headers | `make test` (host build) and the R20 grep in `make lint` |
| REQ-NFR-002 coding standard | `make lint` (cppcheck MISRA, clang-tidy, lizard), and every test that checks a refused argument |
| REQ-NFR-003 warning-free build | `make test` and `make avr` with `-Werror` |
| REQ-NFR-004 line coverage ≥ 90 % | `make coverage` gate on `app/` and `services/` |
| REQ-NFR-005 memory budget | `make size` |

## Open findings from the review of 2026-11-17

1. **REQ-ALM-002, one sample late (fixed).** From NORMAL, gas at or above AL2
   entered ALARM_1 on the third sample and ALARM_2 on the fourth. The relay
   already dropped in ALARM_1, so only the ALARM_2 latch and the 4 Hz beep
   were 100 ms late. Fixed on the author's decision: in NORMAL the AL2 count is
   checked first.
2. **REQ-HMI-004, repeated silence of ALARM_2 (fixed).** Each short press
   restarted the 60 s silence, so pressing once a minute kept ALARM_2 quiet.
   Decision of the author: 60 s in total per ALARM_2, counted from the first
   press, until the alarm is reset or the state changes (a passage through
   FAULT gives a new 60 s). The requirement text and FMEA F14 and F38 are
   updated, with the 49.7-day clock wrap recorded as an accepted limit.
3. **REQ-SAF-010, extra grep in `make lint` (fixed).** In the Makefile, the
   word boundary `\b` before `(malloc|calloc|realloc|free)` had become a
   backspace character, so the grep could never match. cppcheck (MISRA 21.3)
   still caught dynamic memory. The pattern is restored and was checked on a
   planted file: it finds `malloc(` and `free(`, not `freeze(`, and nothing in
   the real code.
4. **REQ-HMI-002, colour table.** The table of colours per state is in
   `main.c`, which the host tests do not build. It is checked on the
   board only.
5. **Large single spike (FMEA F16, fixed).** The 8-sample average was taken
   on the raw ADC value, so one raw sample at 1017 lifted it by about 11 %LEL
   for 8 samples, above the default AL1: one glitch gave ALARM_1. Decision of
   the author: a median of the last 3 raw samples comes first (REQ-FUN-002).
   A single wild sample is removed; a real leak is followed 100 ms later. The
   review then found that the filter restarted when a channel left out came
   back, so a spike that ended a stuck FAULT still passed: decision of the
   author (option X), the filter of a channel left out keeps its history.
   A second review found that a reading at the rails also restarted the
   filter, so a bouncing contact (rail, then 1017) latched ALARM_2: the
   filter is now never restarted after boot.
6. **Touch and alarm in the same sample (REQ-HMI-008, fixed).** If the 3 s
   touch ended on the same sample as the third sample above AL1, MAINTENANCE
   won and the relay stayed energised. Decision of the author: REQ-HMI-008
   now refuses MAINTENANCE while the gas is at or above AL1, and the refused
   request is dropped. Residual: a touch on a sample just below AL1 while gas
   builds is still accepted (F36).
7. **Repeated ADC read errors on one channel (REQ-SAF-002, fixed).** A read
   error skipped the diagnostic, so with the other channel valid one channel
   could fail every read and stay out silently. Decision of the author:
   REQ-SAF-002 now counts a failed read as a bad sample, together with the
   rail readings (FMEA F50).
8. **Peak record wiped by a touch (REQ-LOG-001, fixed).** `act_touch`
   cleared the MAINTENANCE peaks when the request was made, so a touch in the
   2 samples after a MAINTENANCE ended logged the peaks of the finished gas
   test as 0 (found by the review of 2026-11-17). The peaks now restart when
   MAINTENANCE starts, and the log reads a copy fixed at the end.
