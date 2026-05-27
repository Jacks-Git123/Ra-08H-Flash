/*!
 * \file      main.c
 *
 * \brief     Ping-Pong implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "delay.h"
#include "timer.h"
#include "radio.h"
#include "tremo_system.h"
#include "tremo_lpuart.h"
#include "tremo_uart.h"

#define REGION_US915
//#define USE_MODEM_LORA

#if defined( REGION_AS923 )

#define RF_FREQUENCY                                923000000 // Hz

#elif defined( REGION_AU915 )

#define RF_FREQUENCY                                915000000 // Hz

#elif defined( REGION_CN470 )

#define RF_FREQUENCY                                470000000 // Hz

#elif defined( REGION_CN779 )

#define RF_FREQUENCY                                779000000 // Hz

#elif defined( REGION_EU433 )

#define RF_FREQUENCY                                433000000 // Hz

#elif defined( REGION_EU868 )

#define RF_FREQUENCY                                868000000 // Hz

#elif defined( REGION_KR920 )

#define RF_FREQUENCY                                920000000 // Hz

#elif defined( REGION_IN865 )

#define RF_FREQUENCY                                865000000 // Hz

#elif defined( REGION_US915 )

#define RF_FREQUENCY                                915000000 // Hz

#elif defined( REGION_US915_HYBRID )

#define RF_FREQUENCY                                915000000 // Hz

#else
    #error "Please define a frequency band in the compiler options."
#endif

#define TX_OUTPUT_POWER                             14        // dBm

#if defined( USE_MODEM_LORA )

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#elif defined( USE_MODEM_FSK )

#define FSK_FDEV                                    25000     // Hz
#define FSK_DATARATE                                50000     // bps
#define FSK_BANDWIDTH                               50000   // Hz >> DSB in sx126x
#define FSK_AFC_BANDWIDTH                           83333   // Hz
#define FSK_PREAMBLE_LENGTH                         5         // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON                   false

#else
    #error "Please define a modem in the compiler options."
#endif

typedef enum
{
    LOWPOWER,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT
}States_t;

#define RX_TIMEOUT_VALUE                            1800
#define BUFFER_SIZE                                 128 // Define the payload size here

extern volatile uint8_t rx_data[BUFFER_SIZE];
extern volatile uint16_t rx_index;

uint16_t BufferSize = BUFFER_SIZE;
uint8_t Buffer[BUFFER_SIZE];

#define UART_BUF_SIZE 256
uint8_t uartBuf[UART_BUF_SIZE];
volatile uint16_t uartLen = 0;
volatile uint8_t uartReady = 0;
uint8_t byte;

volatile uint8_t txBusy = 0;

volatile States_t State = LOWPOWER;

int8_t RssiValue = 0;
int8_t SnrValue = 0;

uint32_t ChipId[2] = {0};

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone( void );

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError( void );

void UartWrite(uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        // 1. Send data using standard UART function
        uart_send_data(UART0, data[i]);
        
        // 2. Wait until TX transmission is complete (TX empty flag turns true)
        while (uart_get_flag_status(UART0, UART_FLAG_TX_EMPTY) == RESET);
    }
}

/**
 * Main application entry point.
 */

int app_start(void) {
    (void)system_get_chip_id(ChipId);
    
    // Radio initialization
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    Radio.Init( &RadioEvents );

    Radio.SetChannel( RF_FREQUENCY );

#if defined( USE_MODEM_LORA )

    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

#elif defined( USE_MODEM_FSK )

    Radio.SetTxConfig( MODEM_FSK, TX_OUTPUT_POWER, FSK_FDEV, 0,
                                  FSK_DATARATE, 0,
                                  FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
                                  true, 0, 0, 0, 3000 );

    Radio.SetRxConfig( MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                                  0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                                  0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, true,
                                  0, 0,false, true );

#else
    #error "Please define a frequency band in the compiler options."
#endif

    Radio.Rx( RX_TIMEOUT_VALUE );

    while (1) {
		UartWrite((uint8_t*)"Hello\r\n",7);

		Radio.IrqProcess();

		if (rx_index > 0 && !txBusy) {
			int pktLen = -1;

			// Briefly disable interrupts to scan the buffer safely
			NVIC_DisableIRQ(UART0_IRQn); 
			for (int i = 0; i < rx_index; i++) {
				if (rx_data[i] == '\n') {
					pktLen = i + 1;
					break;
				}
			}

			if (pktLen > 0) {
				txBusy = 1;
				Radio.Send((uint8_t *)rx_data, pktLen);
				
				uint16_t remaining = rx_index - pktLen;
				if (remaining > 0) {
					memmove((void*)rx_data, (void*)&rx_data[pktLen], remaining);
				}
				rx_index = remaining;
			}
			NVIC_EnableIRQ(UART0_IRQn);
		}
	}
}

void OnTxDone(void) {
    Radio.Sleep( );
	uartLen = 0;
	txBusy = 0;
	State = LOWPOWER;
	
	// Define a 1-byte array containing the raw numeric value 1
    uint8_t ackVal = 1; 
    UartWrite(&ackVal, 1);

	Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    Radio.Sleep( );
    BufferSize = size;
    memcpy(Buffer, payload, BufferSize);
	
	// 1. Send the raw LoRa payload to the ESP32/STM32
	UartWrite(Buffer, BufferSize);
	
	// 2. Append the newline character immediately after
    uint8_t newline = '\n';
    UartWrite(&newline, 1);
	
    RssiValue = rssi;
    SnrValue = snr;
	
    State = LOWPOWER;
	Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnTxTimeout(void) {
    Radio.Sleep( );
    State = LOWPOWER; //TX_TIMEOUT
	Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxTimeout(void) {
    //printf("OnRxTimeout\r\n");
    Radio.Sleep( );
    State = LOWPOWER; //RX_TIMEOUT
	Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxError(void) {
    Radio.Sleep( );
    State = LOWPOWER; //RX_ERROR
	Radio.Rx(RX_TIMEOUT_VALUE);
}
