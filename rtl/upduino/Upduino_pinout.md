
### UPduino v3.0 pins for Xosera

          [Xosera]       PCF  Pin#  _____  Pin#  PCF       [Xosera]
                             ------| USB |------
                       <GND> |  1   \___/   48 | spi_ssn   (16)
                       <VIO> |  2           47 | spi_sck   (15)
     [BUS_RESET_N]     <RST> |  3           46 | spi_mosi  (17)
                      <DONE> |  4           45 | spi_miso  (14)
        [BUS_CS_N]   led_red |  5           44 | gpio_20   <N/A w/OSC>
      [BUS_RD_NWR] led_green |  6     U     43 | gpio_10   <INTERRUPT>
     [BUS_BYTESEL]  led_blue |  7     P     42 | <12 MHz>
                       <+5V> |  8     D     41 | <GND>
                     <+3.3V> |  9     U     40 | gpio_12   [VGA_HS]
                       <GND> | 10     I     39 | gpio_21   [VGA_VS]
    [BUS_REG_NUM0]   gpio_23 | 11     N     38 | gpio_13   [VGA_R3]
    [BUS_REG_NUM1]   gpio_25 | 12     O     37 | gpio_19   [VGA_G3]
    [BUS_REG_NUM2]   gpio_26 | 13           36 | gpio_18   [VGA_B3]
    [BUS_REG_NUM3]   gpio_27 | 14     V     35 | gpio_11   [VGA_R2]
         [AUDIO_L]   gpio_32 | 15     3     34 | gpio_9    [VGA_G2]
         [AUDIO_R]   gpio_35 | 16     .     33 | gpio_6    [VGA_B2]
       [BUS_DATA0]   gpio_31 | 17     0     32 | gpio_44   [VGA_R1]
       [BUS_DATA1]   gpio_37 | 18           31 | gpio_4    [VGA_G1]
       [BUS_DATA2]   gpio_34 | 19           30 | gpio_3    [VGA_B1]
       [BUS_DATA3]   gpio_43 | 20           29 | gpio_48   [VGA_R0]
       [BUS_DATA4]   gpio_36 | 21           28 | gpio_45   [VGA_G0]
       [BUS_DATA5]   gpio_42 | 22           27 | gpio_47   [VGA_B0]
       [BUS_DATA6]   gpio_38 | 23           26 | gpio_46   [DV_DE]
       [BUS_DATA7]   gpio_28 | 24           25 | gpio_2    [DV_CLK]
                             -------------------
