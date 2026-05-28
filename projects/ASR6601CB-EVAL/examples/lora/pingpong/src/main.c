#include <stdio.h>
#include <string.h>
#include "delay.h"
#include "timer.h"
#include "radio.h"
#include "tremo_regs.h"
#include "tremo_uart.h"
#include "tremo_lpuart.h"
#include "tremo_gpio.h"
#include "tremo_rcc.h"
#include "tremo_pwr.h"
#include "tremo_delay.h"
#include "rtc-board.h"

#define RX_SIZE 256

extern int app_start(void);

//LPUART Configuration
//void LpuartInit(void);
//UART0 Configuration
void uart0_IRQHandler(void);

void uart_log_init(void) {
    // uart0
    gpio_set_iomux(GPIOB, GPIO_PIN_0, 1); //RX: GP16
    gpio_set_iomux(GPIOB, GPIO_PIN_1, 1); //TX GP17

    /* uart config struct init */
    uart_config_t uart_config;
    uart_config_init(&uart_config);

    uart_config.baudrate = UART_BAUDRATE_115200;
    uart_init(UART0, &uart_config);
    uart_cmd(UART0, ENABLE);
	
	uart_config_interrupt(UART0, UART_INTERRUPT_RX_DONE, ENABLE);

	/* NVIC config */
	NVIC_SetPriority(UART0_IRQn, 2);
	NVIC_EnableIRQ(UART0_IRQn);
}

void board_init() {
	rcc_enable_peripheral_clk(RCC_PERIPHERAL_AFEC, true);
    rcc_enable_oscillator(RCC_OSC_XO32K, true);

    rcc_enable_peripheral_clk(RCC_PERIPHERAL_UART0, true);
	//rcc_enable_peripheral_clk(RCC_PERIPHERAL_LPUART, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOA, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOB, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_GPIOD, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_PWR, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_RTC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_SAC, true);
    rcc_enable_peripheral_clk(RCC_PERIPHERAL_LORA, true);
    
    delay_ms(100);
    pwr_xo32k_lpm_cmd(true);
    
    uart_log_init();
	//LpuartInit();

    RtcInit();
}

volatile uint8_t rx_data[RX_SIZE];
volatile uint16_t rx_index = 0;
//uint8_t tx_data[11] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k' };

int main(void) {
    // Target board initialization
    board_init();

    app_start();
}

// void LpuartInit(void) {
	// lpuart_init_t lpuart_init_cofig;

	// gpio_set_iomux(GPIOC, GPIO_PIN_11, 2); // TX:GP11
	// gpio_set_iomux(GPIOD, GPIO_PIN_10, 2); // RX:GP10

	// lpuart_init_cofig.baudrate         = 9600;
	// lpuart_init_cofig.data_width       = LPUART_DATA_8BIT;
	// lpuart_init_cofig.parity           = LPUART_PARITY_NONE;
	// lpuart_init_cofig.stop_bits        = LPUART_STOP_1BIT;
	// lpuart_init_cofig.low_level_wakeup = true;
	// lpuart_init_cofig.start_wakeup     = false;
	// lpuart_init_cofig.rx_done_wakeup   = false;
	// lpuart_init(LPUART, &lpuart_init_cofig);

	// lpuart_config_interrupt(LPUART, LPUART_CR1_RX_NOT_EMPTY_INT, ENABLE);

	// lpuart_config_tx(LPUART, true);
	// lpuart_config_rx(LPUART, true);

	// /* NVIC config */
	// NVIC_SetPriority(LPUART_IRQn, 2);
	// NVIC_EnableIRQ(LPUART_IRQn);

	// for (int i = 0; i < 11; i++) {
		// lpuart_send_data(LPUART, tx_data[i]);
		// while (!lpuart_get_tx_done_status(LPUART)) ;
		// lpuart_clear_tx_done_status(LPUART);
	// }
// }

void uart0_IRQHandler(void) {
	// if (uart_get_interrupt_status(UART0, UART_INTERRUPT_RX_DONE) == SET) {
        //2. Read the byte from the hardware register
        // uint8_t d = uart_receive_data(UART0);

        // if (rx_index < RX_SIZE) {
            // rx_data[rx_index++] = d;
        // } 
        // else {
            //Buffer overflow precaution: flush and reset
			// rx_index = 0;
            // memset((void *)rx_data, 0, RX_SIZE);
        // }

        //3. Clear the RX interrupt flag so the hardware knows the byte was handled
        // uart_clear_interrupt(UART0, UART_INTERRUPT_RX_DONE);
    // }
	if (uart_get_interrupt_status(UART0, UART_INTERRUPT_RX_DONE)) {
        uint8_t byte = uart_receive_data(UART0);

        uart_send_data(UART0, byte);

        while (uart_get_flag_status(UART0, UART_FLAG_TX_FIFO_EMPTY) == RESET);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(void* file, uint32_t line) {
    (void)file;
    (void)line;

    while (1) { }
}
#endif
