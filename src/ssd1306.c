#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "ssd1306.h"

void ssd1306_spi_command(unsigned char command) {
  gpio_put(THUMBY_CS_PIN, 1);
  gpio_put(THUMBY_DC_PIN, 0); // switch to writing commands
  gpio_put(THUMBY_CS_PIN, 0);
  spi_write_blocking(spi0, &command, 1);
  gpio_put(THUMBY_CS_PIN, 1);
}


void display_init()
{
  gpio_init(THUMBY_CS_PIN);
  gpio_init(THUMBY_DC_PIN);
  gpio_init(THUMBY_RESET_PIN);
  gpio_set_dir(THUMBY_CS_PIN, GPIO_OUT);
  gpio_set_dir(THUMBY_DC_PIN, GPIO_OUT);
  gpio_set_dir(THUMBY_RESET_PIN, GPIO_OUT);
  gpio_put(THUMBY_CS_PIN, 1);

  spi_init(spi0, 10 * 1024 * 1024); // Semi-arbitrary baud rate choice; investigate further

  // Reset the screen
  gpio_put(THUMBY_RESET_PIN, 0);
  sleep_ms(10);
  gpio_put(THUMBY_RESET_PIN, 1);

  gpio_set_function(THUMBY_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(THUMBY_SDA_PIN, GPIO_FUNC_SPI);

  unsigned char commands[] = {
    SSD1306_DISPLAYOFF,
    SSD1306_SETDISPLAYCLOCKDIV, 0x80,
    SSD1306_SETDISPLAYOFFSET, 0x00,
    SSD1306_SETSTARTLINE | 0x00,
    SSD1306_DISPLAYALLON_RESUME,
    SSD1306_NORMALDISPLAY, // as opposed to inversed colors
    SSD1306_CHARGEPUMP, 0x14, //?
    SSD1306_MEMORYMODE, 0x00,
    SSD1306_SEGREMAP|1,
    SSD1306_COMSCANDEC,
    SSD1306_SETCONTRAST, 30, // seems a reasonable value, even though it might be a typo for 0x30
    SSD1306_SETPRECHARGE, 0xF1,
    SSD1306_SETVCOMDETECT, 0x20,
    SSD1306_SETMULTIPLEX, (40-1), // height-1
    SSD1306_SETCOMPINS, 0x12,
    0xAD, 0x30,
    SSD1306_DISPLAYON,
  };
  
  for (int i=0; i<sizeof(commands); i++) ssd1306_spi_command(commands[i]);
}

void display_write_buffer(unsigned char * buffer, int bufferLength) {
    unsigned char commands[] = {
    SSD1306_COLUMNADDR, 28, 99,
    SSD1306_PAGEADDR, 0x00, 0x05
  };

  for (int i=0; i<sizeof(commands); i++) ssd1306_spi_command(commands[i]);

  gpio_put(THUMBY_CS_PIN, 1);
  gpio_put(THUMBY_DC_PIN, 1); // switch to writing data
  gpio_put(THUMBY_CS_PIN, 0);
  spi_write_blocking(spi0, buffer, bufferLength);
  gpio_put(THUMBY_CS_PIN, 1);
}


// Set the screen brightness to a value between 0 (off) and 127 (max)
void display_set_brightness(unsigned char brightness){
  if(brightness>127){
    brightness = 127;
  }

  ssd1306_spi_command(0x81);
  ssd1306_spi_command(brightness);
}

