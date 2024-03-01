#include "global.h"

PIO escPio;

u32 escPioOffset = 0;

volatile u32 erpmEdges[4][32] = {0};
volatile u32 escRpm[4]        = {0};
u8 escDmaChannel[4]           = {0};
dma_channel_config escDmaConfig[4];
u8 escErpmFail     = 0;
u32 dmaChannelMask = 0;

// basically a memset(0) channel to clear the erpm edges
u8 escClearDmaChannel;
volatile u32 zero = 0;

#define iv 0xFFFFFFFF
const u32 escDecodeLut[32] = {
	iv, iv, iv, iv, iv, iv, iv, iv, iv, 9, 10, 11, iv, 13, 14, 15,
	iv, iv, 2, 3, iv, 5, 6, 7, iv, 0, 8, 1, iv, 4, 12, iv};

void initESCs() {
	escPio       = pio0;
	escPioOffset = pio_add_program(escPio, &bidir_dshot_x1_program);
	pio_claim_sm_mask(pio0, 0b1111);

	for (i32 i = 0; i < 4; i++) {
		pio_gpio_init(escPio, PIN_MOTORS + i);
		gpio_set_pulls(PIN_MOTORS + i, true, false);
		pio_sm_config c = bidir_dshot_x1_program_get_default_config(escPioOffset);
		sm_config_set_set_pins(&c, PIN_MOTORS + i, 1);
		sm_config_set_out_pins(&c, PIN_MOTORS + i, 1);
		sm_config_set_in_pins(&c, PIN_MOTORS + i);
		sm_config_set_jmp_pin(&c, PIN_MOTORS + i);
		sm_config_set_out_shift(&c, false, true, 32);
		sm_config_set_in_shift(&c, false, true, 32);
		pio_sm_init(escPio, i, escPioOffset, &c);
		pio_sm_set_consecutive_pindirs(escPio, i, PIN_MOTORS + i, 1, true);
		pio_sm_set_enabled(escPio, i, true);
		pio_sm_set_clkdiv_int_frac(escPio, i, bidir_dshot_x1_CLKDIV_300_INT, bidir_dshot_x1_CLKDIV_300_FRAC);
		escDmaChannel[i] = dma_claim_unused_channel(true);
		escDmaConfig[i]  = dma_channel_get_default_config(escDmaChannel[i]);
		channel_config_set_transfer_data_size(&escDmaConfig[i], DMA_SIZE_32);
		channel_config_set_read_increment(&escDmaConfig[i], false);
		channel_config_set_write_increment(&escDmaConfig[i], true);
		channel_config_set_dreq(&escDmaConfig[i], pio_get_dreq(escPio, i, false));
		dma_channel_set_config(escDmaChannel[i], &escDmaConfig[i], false);
		dma_channel_set_read_addr(escDmaChannel[i], &escPio->rxf[i], false);
		dma_channel_set_write_addr(escDmaChannel[i], &erpmEdges[i], false);
		dma_channel_set_trans_count(escDmaChannel[i], 32, true);
		dmaChannelMask |= 1 << escDmaChannel[i];
	}
	escClearDmaChannel                   = dma_claim_unused_channel(true);
	dma_channel_config escClearDmaConfig = dma_channel_get_default_config(escClearDmaChannel);
	channel_config_set_transfer_data_size(&escClearDmaConfig, DMA_SIZE_32);
	channel_config_set_read_increment(&escClearDmaConfig, false);
	channel_config_set_write_increment(&escClearDmaConfig, true);
	dma_channel_set_config(escClearDmaChannel, &escClearDmaConfig, false);
	dma_channel_set_read_addr(escClearDmaChannel, &zero, false);
}

u16 appendChecksum(u16 data) {
	int csum = data;
	csum ^= data >> 4;
	csum ^= data >> 8;
	csum = ~csum;
	csum &= 0xF;
	return (data << 4) | csum;
}

void sendRaw16Bit(const u16 raw[4]) {
	dma_channel_abort(escClearDmaChannel); // abort if not completed
	for (i32 i = 0; i < 4; i++) {
		pio_sm_exec(escPio, i, pio_encode_jmp(escPioOffset));
		while (!pio_sm_is_rx_fifo_empty(escPio, i))
			pio_sm_get(escPio, i);
		dma_channel_set_write_addr(escDmaChannel[i], &erpmEdges[i], false);
		dma_channel_set_trans_count(escDmaChannel[i], 32, true);
		pio_sm_drain_tx_fifo(escPio, i);
		pio_sm_put(escPio, i, ~(raw[i]));
		pio_sm_exec(escPio, i, pio_encode_jmp(escPioOffset + 6));
	}
	dma_start_channel_mask(dmaChannelMask);
}

void sendRaw11Bit(const u16 raw[4]) {
	static u16 t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		t[i] = constrain(raw[i], 0, 2047);
		t[i] = appendChecksum(t[i] << 1 | 1);
	}
	sendRaw16Bit(t);
}

void sendThrottles(const i16 throttles[4]) {
	static u16 t[4] = {0, 0, 0, 0};
	for (int i = 0; i < 4; i++) {
		t[i] = constrain(throttles[i], 0, 2000);
		if (t[i])
			t[i] += 47;
		t[i] = appendChecksum(t[i] << 1 | 0);
	}
	sendRaw16Bit(t);
}