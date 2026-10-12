/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt_audio_defs.h"
#include "esp_bt_audio_classic.h"
#include "esp_bt_audio_le.h"
#include "esp_bt_audio_vol.h"
#include "esp_bt_audio_media.h"
#include "esp_bt_audio_playback.h"
#include "esp_gmf_oal_sys.h"
#include "esp_bt_audio_tel.h"
#include "esp_bt_audio_pb.h"

#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "dev_audio_codec.h"
#include "cmd_reg.h"
#include "bt_debug.h"
#include "bt_power.h"

static int guarded_bt_command(void *context, int argc, char **argv)
{
    if (!bt_power_command_begin()) {
        printf("Bluetooth is OFF/busy/error; use bt_power status or bt_power off\n");
        return 1;
    }
    int ret = (*(esp_console_cmd_func_t *)context)(argc, argv);
    bt_power_command_end();
    return ret;
}
static esp_err_t register_bt_command(const esp_console_cmd_t *command)
{
    static esp_console_cmd_func_t handlers[32];
    static size_t count;
    if (count == sizeof(handlers) / sizeof(*handlers) || !command->func) return ESP_ERR_NO_MEM;
    handlers[count] = command->func;
    esp_console_cmd_t guarded = *command;
    guarded.func = NULL;
    guarded.func_w_context = guarded_bt_command;
    guarded.context = &handlers[count++];
    return esp_console_cmd_register(&guarded);
}

static int power_config(int argc, char **argv)
{
    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "status"))) {
        bt_power_status_t s;
        bt_power_get_status(&s);
        printf("Bluetooth: %s | controller=%s | step=%s | error=%s\n",
               bt_power_state_name(s.state), s.controller_enabled ? "enabled" : "disabled",
               bt_power_step_name(s.step), esp_err_to_name(s.error));
        return 0;
    }
    if (argc == 2 && (!strcmp(argv[1], "on") || !strcmp(argv[1], "off"))) {
        esp_err_t err = bt_power_request(!strcmp(argv[1], "on"));
        printf("Bluetooth request: %s (queued, check status for completion)\n", esp_err_to_name(err));
        return err == ESP_OK ? 0 : 1;
    }
    printf("Usage: bt_power [status|on|off]; ERROR: retry off before on\n");
    return 1;
}

static uint8_t target_device_bda[6] = {0};
#if CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE)
static char target_device_name[32] = {0};
static const char *TAG = "CMD_REG";
#endif  /* CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE) */

static int cmd_playback_play(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_playback_play();
    if (ret == ESP_OK) {
        printf("Media play command sent\n");
    } else {
        printf("Failed to send media play command: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_playback_pause(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_playback_pause();
    if (ret == ESP_OK) {
        printf("Media pause command sent\n");
    } else {
        printf("Failed to send media pause command: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_playback_stop(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_playback_stop();
    if (ret == ESP_OK) {
        printf("Media stop command sent\n");
    } else {
        printf("Failed to send media stop command: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_playback_next(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_playback_next();
    if (ret == ESP_OK) {
        printf("Media next command sent\n");
    } else {
        printf("Failed to send media next command: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_playback_prev(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_playback_prev();
    if (ret == ESP_OK) {
        printf("Media previous command sent\n");
    } else {
        printf("Failed to send media previous command: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_playback_metadata(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: metadata <mask>\n");
        printf("  mask bits: 0x1 title, 0x2 artist, 0x4 album, 0x8 track_num,\n");
        printf("             0x10 num_tracks, 0x20 genre, 0x40 time, 0x80 cover\n");
        return 1;
    }

    char *endptr = NULL;
    uint32_t mask = (uint32_t)strtoul(argv[1], &endptr, 0);
    if (argv[1][0] == '\0' || (endptr && *endptr != '\0')) {
        printf("Invalid mask: %s\n", argv[1]);
        return 1;
    }

    esp_err_t ret = esp_bt_audio_playback_request_metadata(mask);
    if (ret == ESP_OK) {
        printf("Media metadata request sent\n");
    } else {
        printf("Failed to send media metadata request: %s\n", esp_err_to_name(ret));
    }
    return 0;
}

static int cmd_volume_set(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: vol_set <volume>\n");
        printf("  volume: 0-100\n");
        return 1;
    }

    int volume = atoi(argv[1]);
    if (volume < 0 || volume > 100) {
        printf("Volume must be between 0 and 100\n");
        return 1;
    }
#if CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    esp_err_t ret = esp_bt_audio_vol_set_absolute((uint32_t)volume);
    if (ret == ESP_OK) {
        printf("Volume set to %d\n", volume);
    } else {
        printf("Failed to set volume: %s\n", esp_err_to_name(ret));
    }
#else   /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    dev_audio_codec_handles_t *codec_handle = NULL;
    esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&codec_handle);
    esp_codec_dev_set_out_vol(codec_handle->codec_dev, volume);

    esp_err_t ret = esp_bt_audio_vol_notify((uint32_t)volume);
    if (ret == ESP_OK) {
        printf("Volume set to %d\n", volume);
    } else {
        printf("Failed to set volume: %s\n", esp_err_to_name(ret));
    }
#endif  /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    return 0;
}

static int cmd_volume_up(int argc, char **argv)
{
#if CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    esp_err_t ret = esp_bt_audio_vol_set_relative(true);
    if (ret == ESP_OK) {
        printf("Volume up\n");
    } else {
        printf("Failed to increase volume: %s\n", esp_err_to_name(ret));
    }
#else   /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    int current_volume = 0;
    dev_audio_codec_handles_t *codec_handle = NULL;
    esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&codec_handle);
    esp_codec_dev_get_out_vol(codec_handle->codec_dev, &current_volume);
    current_volume = (current_volume >= 90) ? 100 : current_volume + 10;
    esp_codec_dev_set_out_vol(codec_handle->codec_dev, current_volume);

    esp_err_t ret = esp_bt_audio_vol_notify(current_volume);
    if (ret == ESP_OK) {
        printf("Volume up to %d\n", (int)current_volume);
    } else {
        printf("Failed to increase volume: %s\n", esp_err_to_name(ret));
    }
#endif  /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    return 0;
}

static int cmd_volume_down(int argc, char **argv)
{
#if CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    esp_err_t ret = esp_bt_audio_vol_set_relative(false);
    if (ret == ESP_OK) {
        printf("Volume down\n");
    } else {
        printf("Failed to decrease volume: %s\n", esp_err_to_name(ret));
    }
#else   /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    int current_volume = 0;
    dev_audio_codec_handles_t *codec_handle = NULL;
    esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&codec_handle);
    esp_codec_dev_get_out_vol(codec_handle->codec_dev, &current_volume);
    current_volume = (current_volume <= 10) ? 0 : current_volume - 10;
    esp_codec_dev_set_out_vol(codec_handle->codec_dev, current_volume);

    esp_err_t ret = esp_bt_audio_vol_notify(current_volume);
    if (ret == ESP_OK) {
        printf("Volume down to %d\n", (int)current_volume);
    } else {
        printf("Failed to decrease volume: %s\n", esp_err_to_name(ret));
    }
#endif  /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    return 0;
}

#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
static int cmd_connect_device(int argc, char **argv)
{
#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
    if (argc != 2) {
        printf("Usage: connect <mac_address>\n");
        printf("Example: connect 01:02:03:04:05:06\n");
        return 1;
    }

    uint8_t bda[6] = {0};
    if (sscanf(argv[1], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &bda[0], &bda[1], &bda[2], &bda[3], &bda[4], &bda[5]) != 6) {
        printf("Invalid MAC address format. Use: XX:XX:XX:XX:XX:XX\n");
        return 1;
    }
#if CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    esp_err_t ret = esp_bt_audio_classic_connect(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SRC, bda);
#else   /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    esp_err_t ret = esp_bt_audio_classic_connect(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SNK, bda);
#endif  /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    if (ret == ESP_OK) {
        printf("Connecting to device %s...\n", argv[1]);
        memcpy(target_device_bda, bda, sizeof(target_device_bda));
    } else {
        printf("Failed to connect: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    printf("Classic connect is disabled in LE example mode\n");
    return 1;
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */
}

static int cmd_disconnect_device(int argc, char **argv)
{
#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
    bool is_bda_empty = true;
    for (int i = 0; i < sizeof(target_device_bda); ++i) {
        if (target_device_bda[i] != 0) {
            is_bda_empty = false;
            break;
        }
    }
    if (is_bda_empty) {
        printf("No device is currently connected.\n");
        return 1;
    }
#if CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    esp_err_t ret = esp_bt_audio_classic_disconnect(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SRC, target_device_bda);
#else   /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    esp_err_t ret = esp_bt_audio_classic_disconnect(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SNK, target_device_bda);
#endif  /* CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
    if (ret == ESP_OK) {
        printf("Disconnecting device...\n");
        memset(target_device_bda, 0, sizeof(target_device_bda));
    } else {
        printf("Failed to disconnect: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    printf("Classic disconnect is disabled in LE example mode\n");
    return 1;
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */
}

static int cmd_hf_connect(int argc, char **argv)
{
#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
    if (argc != 2) {
        printf("Usage: hf_connect <mac_address>\n");
        printf("Example: hf_connect 01:02:03:04:05:06\n");
        return 1;
    }

    uint8_t bda[6];
    if (sscanf(argv[1], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &bda[0], &bda[1], &bda[2], &bda[3], &bda[4], &bda[5]) != 6) {
        printf("Invalid MAC address format. Use: XX:XX:XX:XX:XX:XX\n");
        return 1;
    }

    esp_err_t ret = esp_bt_audio_classic_connect(ESP_BT_AUDIO_CLASSIC_ROLE_HFP_HF, bda);
    if (ret == ESP_OK) {
        printf("Connecting HFP HF to device %s...\n", argv[1]);
        memcpy(target_device_bda, bda, sizeof(target_device_bda));
    } else {
        printf("Failed to connect HFP HF: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    printf("HFP is disabled in LE example mode\n");
    return 1;
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */
}

static int cmd_hf_disconnect(int argc, char **argv)
{
#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
    bool is_bda_empty = true;
    for (int i = 0; i < sizeof(target_device_bda); ++i) {
        if (target_device_bda[i] != 0) {
            is_bda_empty = false;
            break;
        }
    }
    if (is_bda_empty) {
        printf("No device is currently connected.\n");
        return 1;
    }

    esp_err_t ret = esp_bt_audio_classic_disconnect(ESP_BT_AUDIO_CLASSIC_ROLE_HFP_HF, target_device_bda);
    if (ret == ESP_OK) {
        printf("Disconnecting HFP HF device...\n");
        memset(target_device_bda, 0, sizeof(target_device_bda));
    } else {
        printf("Failed to disconnect HFP HF: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    printf("HFP is disabled in LE example mode\n");
    return 1;
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */
}
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */

static int cmd_call_answer(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_call_answer(0);
    if (ret == ESP_OK) {
        printf("Call answer command sent\n");
    } else {
        printf("Failed to send call answer command: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_call_reject(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_call_reject(0);
    if (ret == ESP_OK) {
        printf("Call reject/terminate command sent\n");
    } else {
        printf("Failed to send call reject/terminate command: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_call_dial(int argc, char **argv)
{
    if (argc > 2) {
        printf("Usage: call_dial [number]\n");
        printf("  number: Optional. If omitted, redial last number\n");
        return 1;
    }
    const char *number = (argc == 2) ? argv[1] : NULL;
    esp_err_t ret = esp_bt_audio_call_dial(number);
    if (ret == ESP_OK) {
        printf("Call dial command sent\n");
    } else {
        printf("Failed to send call dial command: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_pb_fetch(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: pb_fetch <target> [start_idx] [count]\n");
        printf("  target (1-5 only):\n");
        printf("    1  Main phonebook\n");
        printf("    2  Incoming call history\n");
        printf("    3  Outgoing call history\n");
        printf("    4  Missed call history\n");
        printf("    5  Combined call history\n");
        printf("  start_idx: Start index (default 0)\n");
        printf("  count:     Number of entries, 0 = fetch all (default 0)\n");
        return 1;
    }

    char *endptr = NULL;
    unsigned long tv = strtoul(argv[1], &endptr, 0);
    if (argv[1][0] == '\0' || (endptr && *endptr != '\0') || tv < 1 || tv > 5) {
        printf("Invalid target: %s (use 1-5, see pb_fetch with no args)\n", argv[1]);
        return 1;
    }
    uint8_t target = (uint8_t)tv;

    uint16_t start_idx = 0;
    uint16_t count = 0;
    if (argc >= 3) {
        char *endptr = NULL;
        unsigned long v = strtoul(argv[2], &endptr, 0);
        if (argv[2][0] == '\0' || (endptr && *endptr != '\0') || v > 0xFFFF) {
            printf("Invalid start_idx: %s\n", argv[2]);
            return 1;
        }
        start_idx = (uint16_t)v;
    }
    if (argc >= 4) {
        char *endptr = NULL;
        unsigned long v = strtoul(argv[3], &endptr, 0);
        if (argv[3][0] == '\0' || (endptr && *endptr != '\0') || v > 0xFFFF) {
            printf("Invalid count: %s\n", argv[3]);
            return 1;
        }
        count = (uint16_t)v;
    }

    esp_err_t ret = esp_bt_audio_pb_fetch(target, start_idx, count);
    if (ret == ESP_OK) {
        printf("Phonebook/call history fetch request sent (target=%u, start=%u, count=%u)\n",
               (unsigned)target, (unsigned)start_idx, (unsigned)count);
    } else {
        printf("Failed to send pb_fetch: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
static int cmd_le_scan_start(int argc, char **argv)
{
    uint32_t timeout_ms = CONFIG_GMF_EXAMPLE_LE_SCAN_TIMEOUT_MS;

    if (argc >= 2) {
        timeout_ms = (uint32_t)strtoul(argv[1], NULL, 0);
    }
    esp_err_t ret = esp_bt_audio_le_scan_start(timeout_ms);
    if (ret == ESP_OK) {
        printf("LE scan started (%u ms)\n", (unsigned)timeout_ms);
    } else {
        printf("Failed to start LE scan: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_le_scan_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t ret = esp_bt_audio_le_scan_stop();
    if (ret == ESP_OK) {
        printf("LE scan stopped\n");
    } else {
        printf("Failed to stop LE scan: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_le_connect(int argc, char **argv)
{
    uint8_t bda[6] = {0};
    uint8_t addr_type = 0;
    uint32_t timeout_ms = CONFIG_GMF_EXAMPLE_LE_SCAN_TIMEOUT_MS;

    if (argc < 3) {
        printf("Usage: le_connect <addr_type> <mac_address> [timeout_ms]\n");
        printf("Example: le_connect 0 01:02:03:04:05:06 10000\n");
        return 1;
    }
    addr_type = (uint8_t)strtoul(argv[1], NULL, 0);
    if (sscanf(argv[2], "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               &bda[0], &bda[1], &bda[2], &bda[3], &bda[4], &bda[5]) != 6) {
        printf("Invalid MAC address format. Use: XX:XX:XX:XX:XX:XX\n");
        return 1;
    }
    if (argc >= 4) {
        timeout_ms = (uint32_t)strtoul(argv[3], NULL, 0);
    }
    esp_err_t ret = esp_bt_audio_le_connect(addr_type, bda, timeout_ms);
    if (ret == ESP_OK) {
        printf("LE connect started (%s)\n", argv[2]);
    } else {
        printf("Failed to start LE connect: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_le_disconnect(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t ret = esp_bt_audio_le_disconnect();
    if (ret == ESP_OK) {
        printf("LE disconnect requested\n");
    } else {
        printf("Failed to disconnect LE link: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */

#if CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE)
static int cmd_start_discovery(int argc, char **argv)
{
    memset(target_device_name, 0, sizeof(target_device_name));

    if (argc == 2) {
        strncpy(target_device_name, argv[1], sizeof(target_device_name) - 1);
        target_device_name[sizeof(target_device_name) - 1] = '\0';
        printf("Discovery started - looking for device: %s\n", target_device_name);
    } else if (argc == 1) {
        printf("Discovery started - scanning for all devices\n");
    } else {
        printf("Usage: start_discovery [device_name]\n");
        printf("  device_name: Optional. If provided, will auto-connect to this device\n");
        return 1;
    }

    esp_err_t ret = esp_bt_audio_classic_discovery_start();
    if (ret == ESP_OK) {
        if (strlen(target_device_name) > 0) {
            printf("Will auto-connect to '%s' when found\n", target_device_name);
        }
    } else {
        printf("Failed to start discovery: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_stop_discovery(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_classic_discovery_stop();
    if (ret == ESP_OK) {
        printf("Discovery stopped\n");
    } else {
        printf("Failed to stop discovery: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_start_media(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_media_start(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SRC, NULL);
    if (ret == ESP_OK) {
        printf("Media started\n");
    } else {
        printf("Failed to start media: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}

static int cmd_stop_media(int argc, char **argv)
{
    esp_err_t ret = esp_bt_audio_media_stop(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SRC);
    if (ret == ESP_OK) {
        printf("Media stopped\n");
    } else {
        printf("Failed to stop media: %s\n", esp_err_to_name(ret));
    }
    return ret == ESP_OK ? 0 : 1;
}
#endif  /* CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE) */

void cli_register_bt(void)
{
    const esp_console_cmd_t play_cmd = {
        .command = "play",
        .help = "Send media play command",
        .hint = NULL,
        .func = &cmd_playback_play,
    };
    ESP_ERROR_CHECK(register_bt_command(&play_cmd));

    const esp_console_cmd_t pause_cmd = {
        .command = "pause",
        .help = "Send media pause command",
        .hint = NULL,
        .func = &cmd_playback_pause,
    };
    ESP_ERROR_CHECK(register_bt_command(&pause_cmd));

    const esp_console_cmd_t stop_cmd = {
        .command = "stop",
        .help = "Send media stop command",
        .hint = NULL,
        .func = &cmd_playback_stop,
    };
    ESP_ERROR_CHECK(register_bt_command(&stop_cmd));

    const esp_console_cmd_t next_cmd = {
        .command = "next",
        .help = "Send media next track command",
        .hint = NULL,
        .func = &cmd_playback_next,
    };
    ESP_ERROR_CHECK(register_bt_command(&next_cmd));

    const esp_console_cmd_t prev_cmd = {
        .command = "prev",
        .help = "Send media previous track command",
        .hint = NULL,
        .func = &cmd_playback_prev,
    };
    ESP_ERROR_CHECK(register_bt_command(&prev_cmd));

    const esp_console_cmd_t metadata_cmd = {
        .command = "metadata",
        .help = "Request media metadata with mask",
        .hint = "<mask>",
        .func = &cmd_playback_metadata,
    };
    ESP_ERROR_CHECK(register_bt_command(&metadata_cmd));

    const esp_console_cmd_t vol_set_cmd = {
        .command = "vol_set",
        .help = "Set volume level (0-100)",
        .hint = "<volume>",
        .func = &cmd_volume_set,
    };
    ESP_ERROR_CHECK(register_bt_command(&vol_set_cmd));

    const esp_console_cmd_t vol_up_cmd = {
        .command = "vol_up",
        .help = "Increase volume by 10",
        .hint = NULL,
        .func = &cmd_volume_up,
    };
    ESP_ERROR_CHECK(register_bt_command(&vol_up_cmd));

    const esp_console_cmd_t vol_down_cmd = {
        .command = "vol_down",
        .help = "Decrease volume by 10",
        .hint = NULL,
        .func = &cmd_volume_down,
    };
    ESP_ERROR_CHECK(register_bt_command(&vol_down_cmd));

#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC
    {
        const esp_console_cmd_t connect_cmd = {
            .command = "connect",
            .help = "Connect to a Bluetooth device",
            .hint = "<mac_address>",
            .func = &cmd_connect_device,
        };
        ESP_ERROR_CHECK(register_bt_command(&connect_cmd));
    }

    {
        const esp_console_cmd_t disconnect_cmd = {
            .command = "disconnect",
            .help = "Disconnect from current Bluetooth device",
            .hint = NULL,
            .func = &cmd_disconnect_device,
        };
        ESP_ERROR_CHECK(register_bt_command(&disconnect_cmd));
    }

    {
        const esp_console_cmd_t hf_connect_cmd = {
            .command = "hf_connect",
            .help = "Connect HFP HF to a Bluetooth device",
            .hint = "<mac_address>",
            .func = &cmd_hf_connect,
        };
        ESP_ERROR_CHECK(register_bt_command(&hf_connect_cmd));
    }

    {
        const esp_console_cmd_t hf_disconnect_cmd = {
            .command = "hf_disconnect",
            .help = "Disconnect HFP HF from current Bluetooth device",
            .hint = NULL,
            .func = &cmd_hf_disconnect,
        };
        ESP_ERROR_CHECK(register_bt_command(&hf_disconnect_cmd));
    }
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_AUDIO_TECH_CLASSIC */

    const esp_console_cmd_t call_answer_cmd = {
        .command = "call_answer",
        .help = "Answer incoming call",
        .hint = NULL,
        .func = &cmd_call_answer,
    };
    ESP_ERROR_CHECK(register_bt_command(&call_answer_cmd));

    const esp_console_cmd_t call_reject_cmd = {
        .command = "call_reject",
        .help = "Reject / terminate call",
        .hint = NULL,
        .func = &cmd_call_reject,
    };
    ESP_ERROR_CHECK(register_bt_command(&call_reject_cmd));

    const esp_console_cmd_t call_dial_cmd = {
        .command = "call_dial",
        .help = "Dial number; omit for redial",
        .hint = "[number]",
        .func = &cmd_call_dial,
    };
    ESP_ERROR_CHECK(register_bt_command(&call_dial_cmd));

    const esp_console_cmd_t pb_fetch_cmd = {
        .command = "pb_fetch",
        .help = "Fetch phonebook or call history, target 1-5 = main_pb/incoming/outgoing/missed/combined",
        .hint = "<target> [start_idx] [count]",
        .func = &cmd_pb_fetch,
    };
    ESP_ERROR_CHECK(register_bt_command(&pb_fetch_cmd));

#if CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE)
    const esp_console_cmd_t start_discovery_cmd = {
        .command = "start_discovery",
        .help = "Start Bluetooth device discovery with optional auto-connect",
        .hint = "[device_name]",
        .func = &cmd_start_discovery,
    };
    ESP_ERROR_CHECK(register_bt_command(&start_discovery_cmd));

    const esp_console_cmd_t stop_discovery_cmd = {
        .command = "stop_discovery",
        .help = "Stop Bluetooth device discovery",
        .hint = NULL,
        .func = &cmd_stop_discovery,
    };
    ESP_ERROR_CHECK(register_bt_command(&stop_discovery_cmd));

    const esp_console_cmd_t start_media_cmd = {
        .command = "start_media",
        .help = "Start media playback",
        .hint = NULL,
        .func = &cmd_start_media,
    };
    ESP_ERROR_CHECK(register_bt_command(&start_media_cmd));

    const esp_console_cmd_t stop_media_cmd = {
        .command = "stop_media",
        .help = "Stop media playback",
        .hint = NULL,
        .func = &cmd_stop_media,
    };
    ESP_ERROR_CHECK(register_bt_command(&stop_media_cmd));
#endif  /* CONFIG_BT_CLASSIC_ENABLED && defined(CONFIG_GMF_EXAMPLE_A2DP_SOURCE) */

#if CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE
    {
        const esp_console_cmd_t le_scan_start_cmd = {
            .command = "le_scan_start",
            .help = "Start LE scan",
            .hint = "[timeout_ms]",
            .func = &cmd_le_scan_start,
        };
        ESP_ERROR_CHECK(register_bt_command(&le_scan_start_cmd));
    }

    {
        const esp_console_cmd_t le_scan_stop_cmd = {
            .command = "le_scan_stop",
            .help = "Stop LE scan",
            .hint = NULL,
            .func = &cmd_le_scan_stop,
        };
        ESP_ERROR_CHECK(register_bt_command(&le_scan_stop_cmd));
    }

    {
        const esp_console_cmd_t le_connect_cmd = {
            .command = "le_connect",
            .help = "Connect LE peer",
            .hint = "<addr_type> <mac_address> [timeout_ms]",
            .func = &cmd_le_connect,
        };
        ESP_ERROR_CHECK(register_bt_command(&le_connect_cmd));
    }

    {
        const esp_console_cmd_t le_disconnect_cmd = {
            .command = "le_disconnect",
            .help = "Disconnect LE ACL link",
            .hint = NULL,
            .func = &cmd_le_disconnect,
        };
        ESP_ERROR_CHECK(register_bt_command(&le_disconnect_cmd));
    }
#endif  /* CONFIG_GMF_EXAMPLE_AUDIO_TECH_LE */
}

static int restart(int argc, char **argv)
{
    printf("Restarting\n");
    esp_restart();
}

static int free_mem(int argc, char **argv)
{
    printf("\nFree heap size: internal %u, psram %u\nmin  heap size: internal %u, psram %u\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

    return 0;
}

/** 'tasks' command prints the list of tasks and related information */
static int tasks_info(int argc, char **argv)
{
    return esp_gmf_oal_sys_get_real_time_stats(1000, false);
}

static bool parse_log_level(const char *level_str, esp_log_level_t *level)
{
    if (strcmp(level_str, "none") == 0 || strcmp(level_str, "0") == 0) {
        *level = ESP_LOG_NONE;
    } else if (strcmp(level_str, "error") == 0 || strcmp(level_str, "err") == 0 || strcmp(level_str, "1") == 0) {
        *level = ESP_LOG_ERROR;
    } else if (strcmp(level_str, "warn") == 0 || strcmp(level_str, "warning") == 0 || strcmp(level_str, "2") == 0) {
        *level = ESP_LOG_WARN;
    } else if (strcmp(level_str, "info") == 0 || strcmp(level_str, "3") == 0) {
        *level = ESP_LOG_INFO;
    } else if (strcmp(level_str, "debug") == 0 || strcmp(level_str, "4") == 0) {
        *level = ESP_LOG_DEBUG;
    } else if (strcmp(level_str, "verbose") == 0 || strcmp(level_str, "5") == 0) {
        *level = ESP_LOG_VERBOSE;
    } else {
        return false;
    }
    return true;
}

static int log_level(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: log_level <tag|*> <none|error|warn|info|debug|verbose|0-5>\n");
        printf("Example: log_level CLK_SYNC_EL debug\n");
        printf("Example: log_level * warn\n");
        return 1;
    }

    esp_log_level_t level = ESP_LOG_NONE;
    if (!parse_log_level(argv[2], &level)) {
        printf("Invalid log level: %s\n", argv[2]);
        return 1;
    }

    if (level > CONFIG_LOG_MAXIMUM_LEVEL) {
        printf("Requested level %d exceeds compiled maximum %d. Rebuild with a higher maximum (screen profile supports DEBUG).\n",
               (int)level, CONFIG_LOG_MAXIMUM_LEVEL);
        return 1;
    }

    esp_log_level_set(argv[1], level);
    printf("Set log level: tag=%s level=%s\n", argv[1], argv[2]);
    return 0;
}

static int debug_config(int argc, char **argv)
{
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "status") == 0)) {
        for (size_t i = 0; i < bt_debug_module_count(); ++i) {
            const bt_debug_module_t *module = bt_debug_module_at(i);
            printf("%-12s requested=%s effective=%s available=%s\n", module->key,
                   bt_debug_enabled(i) ? "ON" : "OFF", bt_debug_overridden(i) ? "raw override" :
                   bt_debug_effective(i) ? "DEBUG" : "baseline",
                   bt_debug_supported(i) ? "yes" : "not compiled");
        }
        bt_debug_status_t state;
        bt_debug_get_status(&state);
        printf("SD requested=%s state=%s bytes=%" PRIu64 " dropped=%" PRIu32 " truncated=%" PRIu32 " errno=%d\n",
               state.sd_requested ? "ON" : "OFF", bt_debug_sd_state_name(state.sd_state), state.bytes_written,
               state.dropped, state.truncated, state.sd_errno);
        printf("File: %s\nNVS: %s%s\n", state.path, esp_err_to_name(state.settings_error), state.settings_pending ? " (saving)" : "");
        return 0;
    }
    if (argc == 3 && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) {
        bool enabled = strcmp(argv[2], "on") == 0;
        if (!strcmp(argv[1], "sd")) { bt_debug_sd_set(enabled); return 0; }
        if (!strcmp(argv[1], "all") && !enabled) { bt_debug_all_off(); return 0; }
        int index = bt_debug_module_find(argv[1]);
        esp_err_t err = index < 0 ? ESP_ERR_INVALID_ARG : bt_debug_set((size_t)index, enabled);
        if (err != ESP_OK) printf("Debug configuration rejected: %s\n", esp_err_to_name(err));
        return err == ESP_OK ? 0 : 1;
    }
    printf("Usage: debug [status] | debug <module-key|sd> <on|off> | debug all off\n");
    return 1;
}

void cli_register_sys()
{
    static const esp_console_cmd_t cmds[] = {
        {
            .command = "bt_power",
            .help = "Bluetooth ON/OFF without resetting the display; Classic Sink adapter",
            .hint = "[status|on|off]",
            .func = power_config,
        },
        {
            .command = "debug",
            .help = "Configure module debug / async SD logging; shared with the screen",
            .hint = "[status|<module-key|sd|all> <on|off>]",
            .func = &debug_config,
        },
        {
            .command = "restart",
            .help = "Software reset of the chip",
            .hint = NULL,
            .func = &restart,
        },
        {
            .command = "free",
            .help = "Get the current size of free heap memory",
            .hint = NULL,
            .func = &free_mem,
        },
        {
            .command = "tasks",
            .help = "Get information about running tasks",
            .hint = NULL,
            .func = &tasks_info,
        },
        {
            .command = "log_level",
            .help = "Set runtime log level for a tag or *",
            .hint = "<tag|*> <level>",
            .func = &log_level,
        }};

    for (int i = 0; i < sizeof(cmds) / sizeof(esp_console_cmd_t); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}

void cli_init()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "BTAudio >";
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif  /* CONFIG_ESP_CONSOLE_UART */

    cli_register_sys();
    cli_register_bt();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void cli_bt_device_found(const char *name, const uint8_t *bda)
{
#if CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_A2DP_SOURCE
    if (strlen(target_device_name) > 0) {
        if (strstr(name, target_device_name) != NULL) {
            ESP_LOGI(TAG, "Found target device '%s', initiating auto-connect...", target_device_name);
            esp_bt_audio_classic_discovery_stop();
            esp_err_t ret = esp_bt_audio_classic_connect(ESP_BT_AUDIO_CLASSIC_ROLE_A2DP_SRC, (uint8_t *)bda);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Connecting to %s...\n", name);
                memset(target_device_name, 0, sizeof(target_device_name));
            } else {
                ESP_LOGE(TAG, "Failed to connect to %s: %s\n", name, esp_err_to_name(ret));
            }
        }
    }
#endif  /* CONFIG_BT_CLASSIC_ENABLED && CONFIG_GMF_EXAMPLE_A2DP_SOURCE */
}

void cli_bt_device_conn_st_chg(const uint8_t *bda, bool connected)
{
    if (connected) {
        memcpy(target_device_bda, bda, sizeof(target_device_bda));
    } else {
        memset(target_device_bda, 0, sizeof(target_device_bda));
    }
}
