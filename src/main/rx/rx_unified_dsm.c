#include "rx_unified_serial.h"

#ifdef RX_UNIFIED_SERIAL

#include <stdbool.h>
#include <stdlib.h>

#include "control.h"
#include "drv_fmc.h"
#include "drv_serial.h"
#include "drv_time.h"
#include "flash.h"
#include "profile.h"

#define DSM_SCALE_PERCENT 147 //this might stay somewhere or be replaced with wizard scaling
//#define RX_DSM2_1024_TEMP     //for legacy override to dsm2 in place of dsmx
//#define DSM_RSSI_FADES		//for internal dsm link quality rssi based on satellite fades instead of packets per second

#define SPEKTRUM_MAX_FADE_PER_SEC 40
#define SPEKTRUM_FADE_REPORTS_PER_SEC 2

extern uint8_t rx_buffer[RX_BUFF_SIZE];
extern uint8_t rx_data[RX_BUFF_SIZE];
extern uint8_t rx_frame_position;
extern uint8_t expected_frame_length;

extern frame_status_t frame_status;

extern uint16_t link_quality_raw;
extern uint8_t stat_frames_second;

extern uint16_t bind_safety;
extern int32_t channels[16];

extern uint8_t failsafe_sbus_failsafe;
extern uint8_t failsafe_siglost;
extern uint8_t failsafe_noframes;

extern profile_t profile;
extern int current_pid_axis;
extern int current_pid_term;

extern uint8_t telemetry_offset;
extern uint8_t telemetry_packet[14];
extern uint8_t ready_for_next_telemetry;

#define USART usart_port_defs[serial_rx_port]
#define SPECTRUM_BIND_PIN usart_port_defs[profile.serial.rx].rx_pin

void rx_serial_process_dsmx() {

  for (uint8_t counter = 0; counter < 16; counter++) {    //First up, get the rx_data out of the RX buffer and into somewhere safe
    rx_data[counter] = rx_buffer[counter % RX_BUFF_SIZE]; // This can probably go away, as long as the buffer is large enough
  }

#ifdef RX_DSM2_1024_TEMP
  float dsm2_scalefactor = (0.29354210f / DSM_SCALE_PERCENT);
  // 10 bit frames
  static uint8_t spek_chan_shift = 2;
  static uint8_t spek_chan_mask = 0x03;
  static uint8_t dsm_channel_count = 7;
#else //DSMX_2048
#define RX_DSMX_2048_UNIFIED
  float dsmx_scalefactor = (0.14662756f / DSM_SCALE_PERCENT);
  // 11 bit frames
  static uint8_t spek_chan_shift = 3;
  static uint8_t spek_chan_mask = 0x07;
  static uint8_t dsm_channel_count = 12;
#endif

  // Fade to rssi hack
#ifdef DSM_RSSI_FADES
  uint16_t fade_count = (rx_data[0] << 8) + rx_data[1];
  uint32_t timestamp = timer_micros() / 1000 / (1000 / SPEKTRUM_FADE_REPORTS_PER_SEC);
  static uint32_t last_fade_timestamp = 0; // Stores the timestamp of the last fade read.
  static uint16_t last_fade_count = 0;     // Stores the fade count at the last fade read.
  if (last_fade_timestamp == 0) {          //first frame received
    last_fade_count = fade_count;
    last_fade_timestamp = timestamp;
  } else if ((timestamp - last_fade_timestamp) >= 1) {
#ifdef RX_DSMX_2048_UNIFIED
    link_quality_raw = 2048 - ((fade_count - last_fade_count) * 2048 / (SPEKTRUM_MAX_FADE_PER_SEC / SPEKTRUM_FADE_REPORTS_PER_SEC));
#else
    link_quality_raw = 1024 - ((fade_count - last_fade_count) * 1024 / (SPEKTRUM_MAX_FADE_PER_SEC / SPEKTRUM_FADE_REPORTS_PER_SEC));
#endif
    last_fade_count = fade_count;
    last_fade_timestamp = timestamp;
  }
#endif

  for (int b = 3; b < expected_frame_length; b += 2) { //stick data in channels buckets
    const uint8_t spekChannel = 0x0F & (rx_data[b - 1] >> spek_chan_shift);
    if (spekChannel < dsm_channel_count && spekChannel < 12) {
      channels[spekChannel] = ((uint32_t)(rx_data[b - 1] & spek_chan_mask) << 8) + rx_data[b];
      frame_status = FRAME_RX_DONE; // if we can hold 2 here for an entire frame, then we will decode it
    } else {
      //a counter here will flag on 22ms mode which could be used for auto-apply of correct filter cut on rc smoothing
    }
  }

  if (frame_status == FRAME_RX_DONE) {
    bind_safety++;
    if (bind_safety < 120)
      flags.rx_mode = RXMODE_BIND; // this is rapid flash during bind safety
                                   // TAER channel order
#ifdef RX_DSMX_2048_UNIFIED
    state.rx.axis[0] = (channels[1] - 1024.0f) * dsmx_scalefactor;
    state.rx.axis[1] = (channels[2] - 1024.0f) * dsmx_scalefactor;
    state.rx.axis[2] = (channels[3] - 1024.0f) * dsmx_scalefactor;
    state.rx.axis[3] = ((channels[0] - 1024.0f) * dsmx_scalefactor * 0.5f) + 0.5f;

    if (state.rx.axis[3] > 1)
      state.rx.axis[3] = 1;
    if (state.rx.axis[3] < 0)
      state.rx.axis[3] = 0;
#endif

#ifdef RX_DSM2_1024_TEMP
    state.rx.axis[0] = (channels[1] - 512.0f) * dsm2_scalefactor;
    state.rx.axis[1] = (channels[2] - 512.0f) * dsm2_scalefactor;
    state.rx.axis[2] = (channels[3] - 512.0f) * dsm2_scalefactor;
    state.rx.axis[3] = ((channels[0] - 512.0f) * dsm2_scalefactor * 0.5f) + 0.5f;

    if (state.rx.axis[3] > 1)
      state.rx.axis[3] = 1;
    if (state.rx.axis[3] < 0)
      state.rx.axis[3] = 0;
#endif

    rx_apply_expo();

#ifdef RX_DSMX_2048_UNIFIED
    state.aux[AUX_CHANNEL_0] = (channels[4] > 1100) ? 1 : 0; //1100 cutoff intentionally selected to force aux channels low if
    state.aux[AUX_CHANNEL_1] = (channels[5] > 1100) ? 1 : 0; //being controlled by a transmitter using a 3 pos switch in center state
    state.aux[AUX_CHANNEL_2] = (channels[6] > 1100) ? 1 : 0;
    state.aux[AUX_CHANNEL_3] = (channels[7] > 1100) ? 1 : 0;
    state.aux[AUX_CHANNEL_4] = (channels[8] > 1100) ? 1 : 0;
    state.aux[AUX_CHANNEL_5] = (channels[9] > 1100) ? 1 : 0;
    state.aux[AUX_CHANNEL_6] = (channels[10] > 1100) ? 1 : 0;
    state.aux[AUX_CHANNEL_7] = (channels[11] > 1100) ? 1 : 0;
#endif

#ifdef RX_DSM2_1024_TEMP
    state.aux[AUX_CHANNEL_0] = (channels[4] > 550) ? 1 : 0; //550 cutoff intentionally selected to force aux channels low if
    state.aux[AUX_CHANNEL_1] = (channels[5] > 550) ? 1 : 0; //being controlled by a transmitter using a 3 pos switch in center state
    state.aux[AUX_CHANNEL_2] = (channels[6] > 550) ? 1 : 0;
#endif

    rx_lqi_update_fps(0);

    if (profile.channel.aux[AUX_RSSI] > AUX_CHANNEL_11) { //rssi set to internal link quality
#ifdef DSM_RSSI_FADES
#ifdef RX_DSMX_2048_UNIFIED
      state.rx_rssi = 0.000488281 * link_quality_raw;
#else
      state.rx_rssi = 0.000976563 * link_quality_raw;
#endif
#else
      state.rx_rssi = stat_frames_second / 91.0f;
#endif
      state.rx_rssi = state.rx_rssi * state.rx_rssi * state.rx_rssi * LQ_EXPO + state.rx_rssi * (1 - LQ_EXPO);
      state.rx_rssi *= 100.0f;
    } else { //rssi set to value decoded from aux channel input from receiver
#ifdef RX_DSMX_2048_UNIFIED
      state.rx_rssi = ((channels[(profile.channel.aux[AUX_RSSI] + 4)] - 1024.0f) * dsmx_scalefactor * 0.5f) + 0.5f;
#else
      state.rx_rssi = ((channels[(profile.channel.aux[AUX_RSSI] + 4)] - 512.0f) * dsm2_scalefactor * 0.5f) + 0.5f;
#endif
    }
    if (state.rx_rssi > 100.0f)
      state.rx_rssi = 100.0f;
    if (state.rx_rssi < 0.0f)
      state.rx_rssi = 0.0f;

    frame_status = FRAME_TX; //We're done with this frame now.

    if (bind_safety > 120) {        //requires 120 good frames to come in before rx_ready safety can be toggled to 1.  About a second of good data
      flags.rx_ready = 1;           // because aux channels initialize low and clear the binding while armed flag before aux updates high
      flags.rx_mode = !RXMODE_BIND; // restores normal led operation
      bind_safety = 121;            // reset counter so it doesnt wrap
    }
  }
}

// Send Spektrum bind pulses to a GPIO e.g. TX1
void rx_spektrum_bind() {
  if (profile.serial.rx == USART_PORT_INVALID) {
    return;
  }

  if (bind_storage.bind_enable == 0) {
    LL_GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_NO;
    gpio_pin_init(&GPIO_InitStructure, SPECTRUM_BIND_PIN);

    // RX line, set high
    gpio_pin_set(SPECTRUM_BIND_PIN);
    // Bind window is around 20-140ms after powerup
    delay(60000);

    for (uint8_t i = 0; i < 9; i++) { // 9 pulses for internal dsmx 11ms, 3 pulses for internal dsm2 22ms
      // RX line, drive low for 120us
      gpio_pin_reset(SPECTRUM_BIND_PIN);
      delay(120);

      // RX line, drive high for 120us
      gpio_pin_set(SPECTRUM_BIND_PIN);
      delay(120);
    }
  }
}
#endif