/**
 * @file main.c
 *
 */
/* Copyright (C) 2016, 2017 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>

#include "hardware.h"
#include "hardware_uuid.h"

#include "led.h"
#include "console.h"

#include "wifi.h"
#include "udp.h"
#include "inet.h"
#include "uuid.h"

#include "ap_params.h"
#include "network_params.h"
#include "fota_params.h"
#include "fota.h"

#include "e131.h"
#include "e131bridge.h"
#include "e131params.h"
#include "dmxsend.h"
#include "dmxparams.h"
#include "dmxmonitor.h"

#include "oled.h"

#include "software_version.h"

#define OLED_CONNECTED(b,f)	\
do {						\
	if(b) {					\
		f;					\
	}						\
} while (0);

extern "C" {

void notmain(void) {
	_output_type output_type = OUTPUT_TYPE_MONITOR;
	uint8_t mac_address[6] ALIGNED;
	struct ip_info ip_config;
	E131Params e131params;
	DMXParams dmxparams;
	uuid_t uuid;
	char uuid_str[UUID_STRING_LENGTH + 1] ALIGNED;
	oled_info_t oled_info;
	bool oled_connected;

	hardware_init();

	oled_info.slave_address = 0;
	oled_info.type = OLED_PANEL_128x64;

	oled_connected = oled_start(&oled_info);

	(void) e131params.Load();

	if (e131params.isHaveCustomCid()) {
		memcpy(uuid_str, e131params.GetCidString(), UUID_STRING_LENGTH);
		uuid_str[UUID_STRING_LENGTH] = '\0';
		uuid_parse((const char *)uuid_str, uuid);
	} else {
		hardware_uuid(uuid);
		uuid_unparse(uuid, uuid_str);
	}

	output_type = e131params.GetOutputType();

	if (output_type == OUTPUT_TYPE_MONITOR) {
		//
	} else {
		output_type = OUTPUT_TYPE_DMX;
		(void) dmxparams.Load();
	}

	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hardware_board_get_model(), __DATE__, __TIME__);
	printf("WiFi sACN E1.31 DMX Out / Real-time DMX Monitor");

	OLED_CONNECTED(oled_connected, oled_puts(&oled_info, "WiFi sACN E1.31"));

	console_set_top_row(3);

	(void) ap_params_init();
	const char *ap_password = ap_params_get_password();

	hardware_watchdog_init();

	console_status(CONSOLE_YELLOW, "Starting Wifi ...");
	OLED_CONNECTED(oled_connected, oled_status(&oled_info, "Starting Wifi ..."));

	wifi_init(ap_password);

	hardware_watchdog_stop();

	printf("ESP8266 information\n");
	printf(" SDK      : %s\n", system_get_sdk_version());
	printf(" Firmware : %s\n\n", wifi_get_firmware_version());

	if (network_params_init()) {
		console_status(CONSOLE_YELLOW, "Changing to Station mode ...");
		OLED_CONNECTED(oled_connected, oled_status(&oled_info, "Changing to Station mode ..."));

		if (network_params_is_use_dhcp()) {
			wifi_station(network_params_get_ssid(), network_params_get_password());
		} else {
			ip_config.ip.addr = network_params_get_ip_address();
			ip_config.netmask.addr = network_params_get_net_mask();
			ip_config.gw.addr = network_params_get_default_gateway();
			wifi_station_ip(network_params_get_ssid(), network_params_get_password(), &ip_config);
		}
	}

	const _wifi_mode opmode = wifi_get_opmode();

	if (opmode == WIFI_STA) {
		printf("WiFi mode : Station\n");
	} else {
		printf("WiFi mode : Access Point (authenticate mode : %s)\n", *ap_password == '\0' ? "Open" : "WPA_WPA2_PSK");
	}

	if (wifi_get_macaddr(mac_address)) {
		printf(" MAC address : "MACSTR "\n", MAC2STR(mac_address));
	} else {
		console_error("wifi_get_macaddr");
		OLED_CONNECTED(oled_connected, oled_status(&oled_info, "E: wifi_get_macaddr"));
	}

	printf(" Hostname    : %s\n", wifi_station_get_hostname());

	if (wifi_get_ip_info(&ip_config)) {
		printf(" IP-address  : " IPSTR "\n", IP2STR(ip_config.ip.addr));
		printf(" Netmask     : " IPSTR "\n", IP2STR(ip_config.netmask.addr));
		printf(" Gateway     : " IPSTR "\n", IP2STR(ip_config.gw.addr));
		if (opmode == WIFI_STA) {
			const _wifi_station_status status = wifi_station_get_connect_status();
			printf("      Status : %s\n", wifi_station_status(status));
			if (status != WIFI_STATION_GOT_IP){
				printf("SSID : %s\n", network_params_get_ssid());
				console_error("Not connected!");
				if (oled_connected) {
					oled_set_cursor(&oled_info, 2, 0);
					oled_puts(&oled_info, wifi_station_status(status));
					oled_set_cursor(&oled_info, 5, 0);
					oled_printf(&oled_info, "SSID : %s\n", network_params_get_ssid());
					oled_status(&oled_info, "<Not connected!>");
				}
				for(;;);
			}
		}
	} else {
		console_error("wifi_get_ip_info");
		OLED_CONNECTED(oled_connected, oled_status(&oled_info, "E: wifi_get_ip_info"));
	}

	if (fota_params_init()) {
		OLED_CONNECTED(oled_connected, oled_status(&oled_info, "FOTA mode"));
		console_newline();
		fota(fota_params_get_server());
		for(;;);
	}

	console_status(CONSOLE_YELLOW, "Starting UDP ...");
	OLED_CONNECTED(oled_connected, oled_status(&oled_info, "Starting UDP ..."));

	udp_begin(E131_DEFAULT_PORT);

	console_status(CONSOLE_YELLOW, "Join group ...");
	OLED_CONNECTED(oled_connected, oled_status(&oled_info, "Join group ..."));

	uint32_t group_ip;
	(void)inet_aton("239.255.0.0", &group_ip);
	const uint16_t universe = e131params.GetUniverse();
	group_ip = group_ip | ((uint32_t)(((uint32_t)universe & (uint32_t)0xFF) << 24)) | ((uint32_t)(((uint32_t)universe & (uint32_t)0xFF00) << 8));
	udp_joingroup(group_ip);

	E131Bridge bridge;
	DMXSend dmx;
	DMXMonitor monitor;

	bridge.setCid(uuid);
	bridge.setUniverse(universe);
	bridge.setMergeMode(e131params.GetMergeMode());

	if (output_type == OUTPUT_TYPE_MONITOR) {
		bridge.SetOutput(&monitor);
		console_set_top_row(20);
	} else {
		bridge.SetOutput(&dmx);

		dmx.SetBreakTime(dmxparams.GetBreakTime());
		dmx.SetMabTime(dmxparams.GetMabTime());

		const uint8_t refresh_rate = dmxparams.GetRefreshRate();
		uint32_t period = (uint32_t) 0;

		if (refresh_rate != (uint8_t) 0) {
			period = (uint32_t) (1E6 / refresh_rate);
		}
		dmx.SetPeriodTime(period);
	}

	printf("\nBridge configuration\n");
	const uint8_t *firmware_version = bridge.GetSoftwareVersion();
	printf(" Firmware     : %d.%d\n", firmware_version[0], firmware_version[1]);
	printf(" CID          : %s\n", uuid_str);
	printf(" Universe     : %d\n", bridge.getUniverse());
	printf(" Merge mode   : %s\n", bridge.getMergeMode() == E131_MERGE_HTP ? "HTP" : "LTP");
	printf(" Multicast ip : " IPSTR "\n", IP2STR(group_ip));
	printf(" Unicast ip   : " IPSTR "\n\n", IP2STR(ip_config.ip.addr));

	if (output_type == OUTPUT_TYPE_DMX) {
		printf("DMX Send parameters\n");
		printf(" Break time   : %d\n", (int) dmx.GetBreakTime());
		printf(" MAB time     : %d\n", (int) dmx.GetMabTime());
		printf(" Refresh rate : %d\n", (int) (1E6 / dmx.GetPeriodTime()));
	}

	if (oled_connected) {
		oled_set_cursor(&oled_info, 0, 18);

		switch (output_type) {
		case OUTPUT_TYPE_DMX:
			oled_puts(&oled_info, "DMX");
			break;
		case OUTPUT_TYPE_MONITOR:
			oled_puts(&oled_info, "Mon");
			break;
		default:
			break;
		}

		oled_set_cursor(&oled_info, 1, 0);
		if (opmode == WIFI_STA) {
			oled_printf(&oled_info, "S: %s", network_params_get_ssid());
		} else {
			oled_printf(&oled_info, "AP (%s)\n", *ap_password == '\0' ? "Open" : "WPA_WPA2_PSK");
		}

		oled_set_cursor(&oled_info, 2, 0);
		oled_puts(&oled_info, "CID: ");
		oled_puts(&oled_info, uuid_str);
		oled_set_cursor(&oled_info, 4, 0);
		oled_printf(&oled_info, "U: %d M: %s", bridge.getUniverse(), bridge.getMergeMode() == E131_MERGE_HTP ? "HTP" : "LTP");
		oled_set_cursor(&oled_info, 5, 0);
		oled_printf(&oled_info, "M: " IPSTR "", IP2STR(group_ip));
		oled_set_cursor(&oled_info, 6, 0);
		oled_printf(&oled_info, "U: " IPSTR "", IP2STR(ip_config.ip.addr));
	}

	hardware_watchdog_init();

	console_status(CONSOLE_GREEN, "Bridge is running");
	OLED_CONNECTED(oled_connected, oled_status(&oled_info, "Bridge is running"));

	for (;;) {
		hardware_watchdog_feed();
		(void) bridge.Run();
		led_blink();
	}
}

}
