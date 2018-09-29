/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2018 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
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

#include "hardwarebaremetal.h"
#include "networkh3emac.h"
#include "ledblinkbaremetal.h"

#include "console.h"
#include "display.h"

#include "artnetnode.h"
#include "artnetdiscovery.h"
#include "artnetparams.h"

#include "ipprog.h"

#include "timecode.h"
#include "timesync.h"

// DMX Out, RDM Controller
#include "dmxparams.h"
#include "dmxsend.h"
// Pixel Controller
#include "ws28xxstripeparams.h"
#include "ws28xxstripedmx.h"

#include "software_version.h"

extern "C" {

void notmain(void) {
	HardwareBaremetal hw;
	NetworkH3emac nw;
	LedBlinkBaremetal lb;
	uint8_t nHwTextLength;
	ArtNetParams artnetparams;
	IpProg ipprog;

	if (artnetparams.Load()) {
		artnetparams.Dump();
	}

	const TOutputType tOutputType = artnetparams.GetOutputType();

	Display display(0,8);
	const bool oled_connected = display.isDetected();

	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);

	console_puts("Ethernet Art-Net 3 Node ");
	console_set_fg_color(tOutputType == OUTPUT_TYPE_DMX ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("DMX Output");
	console_set_fg_color(CONSOLE_WHITE);
	console_puts(" / ");
	console_set_fg_color((artnetparams.IsRdm() && (tOutputType == OUTPUT_TYPE_DMX)) ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("RDM");
	console_set_fg_color(CONSOLE_WHITE);
	console_puts(" / ");
	console_set_fg_color(tOutputType == OUTPUT_TYPE_SPI ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("Pixel controller {4 Universes}");
	console_set_fg_color(CONSOLE_WHITE);
	console_putc('\n');

	hw.SetLed(HARDWARE_LED_ON);

	console_set_top_row(3);

	console_status(CONSOLE_YELLOW, "Network init ...");
	DISPLAY_CONNECTED(oled_connected, display.TextStatus("Network init ..."));

	nw.Init();
	nw.Print();

	ArtNetNode node;
	DMXSend dmx;
	SPISend spi;

	TimeCode timecode;
	TimeSync timesync;
	ArtNetRdmController discovery;

	console_status(CONSOLE_YELLOW, "Setting Node parameters ...");
	DISPLAY_CONNECTED(oled_connected, display.TextStatus("Setting Node parameters ..."));

	artnetparams.Set(&node);

	node.SetIpProgHandler(&ipprog);

	if (artnetparams.IsUseTimeCode()) {
		timecode.Start();
		node.SetTimeCodeHandler(&timecode);
	}

	if (artnetparams.IsUseTimeSync()) {
		timesync.Start();
		node.SetTimeSyncHandler(&timesync);
	}

	node.SetUniverseSwitch(0, ARTNET_OUTPUT_PORT, artnetparams.GetUniverse());

	if (tOutputType == OUTPUT_TYPE_SPI) {
		WS28XXStripeParams ws28xxparms;

		if (ws28xxparms.Load()) {
			ws28xxparms.Dump();
			ws28xxparms.Set(&spi);
		}

		node.SetOutput(&spi);
		node.SetDirectUpdate(true);

		const uint16_t nLedCount = spi.GetLEDCount();
		const uint8_t nUniverse = artnetparams.GetUniverse();

		if (spi.GetLEDType() == SK6812W) {
			if (nLedCount > 128) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(1, ARTNET_OUTPUT_PORT, nUniverse + 1);
			}
			if (nLedCount > 256) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(2, ARTNET_OUTPUT_PORT, nUniverse + 2);
			}
			if (nLedCount > 384) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(3, ARTNET_OUTPUT_PORT, nUniverse + 3);
			}
		} else {
			if (nLedCount > 170) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(1, ARTNET_OUTPUT_PORT, nUniverse + 1);
			}
			if (nLedCount > 340) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(2, ARTNET_OUTPUT_PORT, nUniverse + 2);
			}
			if (nLedCount > 510) {
				node.SetDirectUpdate(true);
				node.SetUniverseSwitch(3, ARTNET_OUTPUT_PORT, nUniverse + 3);
			}
		}
	} else {
		DMXParams dmxparams;

		if (dmxparams.Load()) {
			dmxparams.Dump();
			dmxparams.Set(&dmx);
		}

		node.SetOutput(&dmx);
		node.SetDirectUpdate(false);

		if(artnetparams.IsRdm()) {
			if (artnetparams.IsRdmDiscovery()) {
				console_status(CONSOLE_YELLOW, "Running RDM Discovery ...");
				DISPLAY_CONNECTED(oled_connected, display.TextStatus("Running RDM Discovery ..."));
				discovery.Full();
			}
			node.SetRdmHandler(&discovery);
		}
	}

	node.Print();

	if (tOutputType == OUTPUT_TYPE_SPI) {
		spi.Print();
	} else {
		dmx.Print();
	}

	if (oled_connected) {
		display.Write(1, "Eth Art-Net 3 ");

		if (tOutputType == OUTPUT_TYPE_SPI) {
			display.PutString("Pixel");
		} else {
			if (artnetparams.IsRdm()) {
				display.PutString("RDM");
			} else {
				display.PutString("DMX");
			}
		}

		uint8_t nAddress;
		node.GetUniverseSwitch((uint8_t) 0, nAddress);

		(void) display.Printf(2, "%s", hw.GetBoardName(nHwTextLength));
		(void) display.Printf(3, "IP: " IPSTR "", IP2STR(Network::Get()->GetIp()));
		if (nw.IsDhcpKnown()) {
			if (nw.IsDhcpUsed()) {
				display.PutString(" D");
			} else {
				display.PutString(" S");
			}
		}
		(void) display.Printf(4, "N: " IPSTR "", IP2STR(Network::Get()->GetNetmask()));
		(void) display.Printf(5, "SN: %s", node.GetShortName());
		(void) display.Printf(6, "N: %d SubN: %d U: %d", node.GetNetSwitch(),node.GetSubnetSwitch(), nAddress);
		(void) display.Printf(7, "Active ports: %d", node.GetActiveOutputPorts());
	}

	console_status(CONSOLE_YELLOW, "Starting the Node ...");
	DISPLAY_CONNECTED(oled_connected, display.TextStatus("Starting the Node ..."));

	node.Start();

	console_status(CONSOLE_GREEN, "Node started");
	DISPLAY_CONNECTED(oled_connected, display.TextStatus("Node started"));

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();
		(void) node.HandlePacket();
		lb.Run();
	}
}

}