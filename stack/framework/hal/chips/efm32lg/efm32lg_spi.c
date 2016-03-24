/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2015 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*! \file efm32hg_spi.c
 *
 * \author jeremie@wizzilab.com
 * \author daniel.vandenakker@uantwerpen.be
 * \author contact@christophe.vg
 */

#include <stdbool.h>
#include <stdint.h>

#include "debug.h"

#include "em_device.h"
#include "em_chip.h"
#include "em_usart.h"
#include "em_gpio.h"
#include "em_dma.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_int.h"

#include "hwgpio.h"
#include "hwspi.h"

#include "platform.h"

#define USARTS    3
#define LOCATIONS 6

typedef struct {
  uint32_t location;
  pin_id_t mosi;
  pin_id_t miso;
  pin_id_t clk;
} spi_pins_t;

#define UNDEFINED_LOCATION {                      \
 .location = 0,                                  \
 .mosi       = { .port = 0,         .pin =  0 },   \
 .miso       = { .port = 0,         .pin =  0 },   \
 .clk       = { .port = 0,         .pin =  0 }    \
}

static spi_pins_t location[USARTS][LOCATIONS] = {
  {
    // USART 0
    {
      .location = USART_ROUTE_LOCATION_LOC0,
      .mosi     = { .port = gpioPortE, .pin = 10 },
      .miso     = { .port = gpioPortE, .pin = 11 },
      .clk      = { .port = gpioPortE, .pin = 12 }
    },
    {
      .location = USART_ROUTE_LOCATION_LOC1,
      .mosi     = { .port = gpioPortE, .pin = 7 },
      .miso     = { .port = gpioPortE, .pin = 6 },
      .clk      = { .port = gpioPortE, .pin = 5 }
    },
    {
      .location = USART_ROUTE_LOCATION_LOC2,
      .mosi     = { .port = gpioPortC, .pin = 11 },
      .miso     = { .port = gpioPortC, .pin = 10 },
      .clk      = { .port = gpioPortC, .pin = 9 }
    },
    // no LOCATION 3
        UNDEFINED_LOCATION,
    {
      .location = USART_ROUTE_LOCATION_LOC4,
      .mosi     = { .port = gpioPortB, .pin = 7 },
      .miso     = { .port = gpioPortB, .pin = 8 },
      .clk      = { .port = gpioPortB, .pin = 13 }
    },
    {
      .location = USART_ROUTE_LOCATION_LOC5,
      .mosi     = { .port = gpioPortC, .pin = 0 },
      .miso     = { .port = gpioPortC, .pin = 1 },
      .clk      = { .port = gpioPortB, .pin = 13 }
    }
  },

  {
   // USART 1
   {
     .location = USART_ROUTE_LOCATION_LOC0,
     .mosi     = { .port = gpioPortC, .pin = 0 },
     .miso     = { .port = gpioPortC, .pin = 1 },
     .clk      = { .port = gpioPortB, .pin = 7 }
   },
   {
     .location = USART_ROUTE_LOCATION_LOC1,
     .mosi     = { .port = gpioPortD, .pin = 0 },
     .miso     = { .port = gpioPortD, .pin = 1 },
     .clk      = { .port = gpioPortD, .pin = 2 }
   },
   {
     .location = USART_ROUTE_LOCATION_LOC2,
     .mosi     = { .port = gpioPortD, .pin = 7 },
     .miso     = { .port = gpioPortD, .pin = 6 },
     .clk      = { .port = gpioPortF, .pin = 0 }
   },
    // no LOCATION 3
        UNDEFINED_LOCATION,
    // no LOCATION 4
        UNDEFINED_LOCATION,
    // no LOCATION 5
        UNDEFINED_LOCATION
    
 },
 {
   // USART 2
   {
     .location = USART_ROUTE_LOCATION_LOC0,
     .mosi     = { .port = gpioPortC, .pin = 2 },
     .miso     = { .port = gpioPortC, .pin = 3 },
     .clk      = { .port = gpioPortC, .pin = 4 }
   },
   {
     .location = USART_ROUTE_LOCATION_LOC1,
     .mosi     = { .port = gpioPortB, .pin = 3 },
     .clk      = { .port = gpioPortB, .pin = 5 }
   },
    // no LOCATION 2
        UNDEFINED_LOCATION,
    // no LOCATION 3
        UNDEFINED_LOCATION,
    // no LOCATION 4
        UNDEFINED_LOCATION,
    // no LOCATION 5
        UNDEFINED_LOCATION
 }
};

typedef struct spi_usart {
  USART_TypeDef*    channel;
  CMU_Clock_TypeDef clock;
} spi_usart_t;

// private storage for usarts, pointers to these records are passed around
static spi_usart_t usart[USARTS] = {
  {
    .channel = USART0,
    .clock   = cmuClock_USART0
  },
  {
    .channel = USART1,
    .clock   = cmuClock_USART1
  },
  {
    .channel = USART2,
    .clock   = cmuClock_USART2
  },
};

#define MAX_SPI_HANDLES 4              // TODO expose this in chip configuration
#define MAX_SPI_SLAVE_HANDLES 4        // TODO expose this in chip configuration

// private implementation of handle structs
struct spi_handle {
  spi_usart_t*        usart;
  spi_pins_t*         pins;
  uint32_t            baudrate;
  uint8_t             databits;
  bool                msbf;
  spi_slave_handle_t* slave[MAX_SPI_SLAVE_HANDLES];
  uint8_t             slaves;  // number of slaves for array mgmt
  uint8_t             users;   // for reference counting of active slaves
};

// private storage to spi handles, pointers to these are passed around
uint8_t      next_spi_handle = 0;
spi_handle_t handle[MAX_SPI_HANDLES];

struct spi_slave_handle {
  spi_handle_t* spi;
  pin_id_t      cs;
  bool          cs_is_active_low;
  bool          selected;
};

// private storage to spi slave handles, pointers to these are passed around
uint8_t            next_spi_slave_handle = 0;
spi_slave_handle_t slave_handle[MAX_SPI_SLAVE_HANDLES];

spi_handle_t* spi_init(uint8_t idx, uint32_t baudrate, uint8_t databits,
                       bool msbf, uint8_t pins)
{
  // limit pre-allocated handles
  assert(next_spi_handle < MAX_SPI_HANDLES);

  // assert what is supported by HW and emlib
  assert(databits == 8 || databits == 9);
  assert(idx < USARTS);
  assert(pins < LOCATIONS); // TODO: not more implemented yet

  // configure new handle
  handle[next_spi_handle] = (spi_handle_t){
    .usart    = &usart[idx],
    .pins     = &location[idx][pins],
    .baudrate = baudrate,
    .databits = databits,
    .msbf     = msbf,
    .slaves   = 0,
    .users    = 0
  };
 
  // pins can be reused, e.g. same configuration, different baudrate
  error_t err;
  err = hw_gpio_configure_pin(handle[next_spi_handle].pins->mosi, false, gpioModePushPull, 0);
  assert( err == SUCCESS || err == EALREADY );
  err = hw_gpio_configure_pin(handle[next_spi_handle].pins->miso, false, gpioModeInput,    0);
  assert( err == SUCCESS || err == EALREADY );
  err = hw_gpio_configure_pin(handle[next_spi_handle].pins->clk,  false, gpioModePushPull, 0);
  assert( err == SUCCESS || err == EALREADY );

  next_spi_handle++;
  return &handle[next_spi_handle-1];
}

static bool ensure_slaves_deselected(spi_handle_t* spi) {
  // make sure CS lines for all slaves of this bus are high for active low slaves and vice versa
  for(uint8_t s=0; s<spi->slaves; s++) {
    if(spi->slave[s]->cs_is_active_low)
      hw_gpio_set(spi->slave[s]->cs);
    else
      hw_gpio_clr(spi->slave[s]->cs);
  }
}

static bool spi_enable(spi_handle_t* spi) {
  // basic reference counting
  spi->users++;
  if(spi->users > 1) { return false; } // should already be enabled
  
  ensure_slaves_deselected(spi);

  // CMU_ClockEnable(cmuClock_GPIO,    true); // TODO future use: hw_gpio_enable
  CMU_ClockEnable(spi->usart->clock, true);

	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;

  usartInit.databits  = spi->databits == 9 ? usartDatabits9 : usartDatabits8;
  usartInit.baudrate  = spi->baudrate;
  usartInit.master    = true;
  usartInit.msbf      = spi->msbf;
  usartInit.clockMode = usartClockMode0;
  
  USART_InitSync(spi->usart->channel, &usartInit);
  USART_Enable  (spi->usart->channel, usartEnable);

  spi->usart->channel->ROUTE = USART_ROUTE_TXPEN
                             | USART_ROUTE_RXPEN
                             | USART_ROUTE_CLKPEN
                             | spi->pins->location;
  
  return true;
}

static bool spi_disable(spi_handle_t* spi) {
  // basic reference counting
  if(spi->users < 1) { return false; }  // should already be disabled
  spi->users--;
  if(spi->users > 0) { return false; }  // don't disable, still other users

  // reset route to make sure that TX pin will become low after disable
  spi->usart->channel->ROUTE = _USART_ROUTE_RESETVALUE;

  USART_Enable(spi->usart->channel, usartDisable);
  CMU_ClockEnable(spi->usart->clock, false);
  // CMU_ClockEnable(cmuClock_GPIO, false); // TODO future use: hw_gpio_disable

  ensure_slaves_deselected(spi); // turn off all CS lines, because bus is down

  return true;
}

spi_slave_handle_t* spi_init_slave(spi_handle_t* spi, pin_id_t cs_pin, bool cs_is_active_low) {
  // limit pre-allocated handles
  assert(next_spi_slave_handle < MAX_SPI_SLAVE_HANDLES);

  // configure CS pin as output. If the bus is already active and we have an active low slave
  // we pull CS high to prevent the slave from starting. If the bus is not already active (powered down)
  // we keep CS low (selected) to prevent current flowing to the slave.
  bool initial_level = 0;
  if(spi->users > 0 && cs_is_active_low)
    initial_level = 1;

  assert(hw_gpio_configure_pin(cs_pin, false, gpioModePushPull, initial_level) == SUCCESS);

  slave_handle[next_spi_slave_handle] = (spi_slave_handle_t){
    .spi      = spi,
    .cs       = cs_pin,
    .cs_is_active_low = cs_is_active_low,
    .selected = false
  };
  
  // add slave to spi for back-reference
  spi->slave[spi->slaves] = &slave_handle[next_spi_slave_handle];
  spi->slaves++;
  
  next_spi_slave_handle++;
  return &slave_handle[next_spi_slave_handle-1];
}

void spi_select(spi_slave_handle_t* slave) {
  if( slave->selected ) { return; }
  spi_enable(slave->spi);
  if(slave->cs_is_active_low)
    hw_gpio_clr(slave->cs);
  else
    hw_gpio_set(slave->cs);

  slave->selected = true;
}

void spi_deselect(spi_slave_handle_t* slave) {
  if( ! slave->selected ) { return; }
  if(slave->cs_is_active_low)
    hw_gpio_set(slave->cs);
  else
    hw_gpio_clr(slave->cs);

  spi_disable(slave->spi);
  slave->selected = false;
}

unsigned char spi_exchange_byte(spi_slave_handle_t* slave, unsigned char data) {
  return USART_SpiTransfer(slave->spi->usart->channel, data);
}

void spi_send_byte_with_control(spi_slave_handle_t* slave, uint16_t data) {
  USART_TxExt(slave->spi->usart->channel, data);
}

void spi_exchange_bytes(spi_slave_handle_t* slave,
                        uint8_t* TxData, uint8_t* RxData, unsigned int length)
{
  uint16_t i = 0;
  if( RxData != NULL && TxData != NULL ) {           // two way transmition
    while( i < length ) {
      RxData[i] = spi_exchange_byte(slave, TxData[i]);
      i++;
    }
  } else if( RxData == NULL && TxData != NULL ) {    // send only
    while( i < length ) {
      spi_exchange_byte(slave, TxData[i]);
      i++;
    }
  } else if( RxData != NULL && TxData == NULL ) {   // recieve only
    while( i < length ) {
      RxData[i] = spi_exchange_byte(slave, 0);
      i++;
    }
  }
}
