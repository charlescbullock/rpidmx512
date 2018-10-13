/**
 * @file storenetwork.cpp
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

#include <stdint.h>
#include <assert.h>

#include "storenetwork.h"
#include "spiflashstore.h"

#include "networkparams.h"

#include "debug.h"

StoreNetwork::StoreNetwork(void) {
	DEBUG_ENTRY

	assert(SpiFlashStore::Get() != 0);

	DEBUG_EXIT
}

StoreNetwork::~StoreNetwork(void) {
	DEBUG_ENTRY

	DEBUG_EXIT
}

void StoreNetwork::Update(const struct TNetworkParams *pNetworkParams) {
	DEBUG_ENTRY

	SpiFlashStore::Get()->Update(STORE_NETWORK, (void *)pNetworkParams, sizeof(struct TNetworkParams));

	DEBUG_EXIT
}

void StoreNetwork::Copy(struct TNetworkParams* pNetworkParams) {
	DEBUG_ENTRY

	SpiFlashStore::Get()->Copy(STORE_NETWORK, (void *)pNetworkParams, sizeof(struct TNetworkParams));

	DEBUG_EXIT
}

void StoreNetwork::UpdateIp(uint32_t nIp) {
	DEBUG_ENTRY

	struct TNetworkParams p;

	const uint32_t nOffset = (uint8_t *)&p.nLocalIp - (uint8_t *)&p;

	DEBUG_PRINTF("nOffset=%d", nOffset);

	SpiFlashStore::Get()->Update(STORE_NETWORK, nOffset, (void *)&nIp, sizeof(uint32_t), NetworkParams::GetMaskIpAddress());

	DEBUG_EXIT
}

void StoreNetwork::UpdateNetMask(uint32_t nNetMask) {
	DEBUG_ENTRY

	struct TNetworkParams p;

	const uint32_t nOffset = (uint8_t *)&p.nNetmask - (uint8_t *)&p;

	DEBUG_PRINTF("nOffset=%d", nOffset);

	SpiFlashStore::Get()->Update(STORE_NETWORK, nOffset, (void *)&nNetMask, sizeof(uint32_t), NetworkParams::GetMaskNetMask());

	DEBUG_EXIT
}