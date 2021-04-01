#pragma GCC optimize ("O3")
// Xosera Test Jig
enum
{
  BUS_SEL_N     = 10,
  BUS_RD_RNW    = 11,
  BUS_BYTESEL   = 12,

  BUS_REG_NUM0  = A0,
  BUS_REG_NUM1  = A1,
  BUS_REG_NUM2  = A2,
  BUS_REG_NUM3  = A3,

  BUS_DATA0     = 9,
  BUS_DATA1     = 8,
  BUS_DATA2     = 7,
  BUS_DATA3     = 6,
  BUS_DATA4     = 5,
  BUS_DATA5     = 4,
  BUS_DATA6     = 3,
  BUS_DATA7     = 2,

  CS_SELECTED   = LOW,
  CS_DESELECTED = HIGH,

  RNW_WRITE     = LOW,    // CPU WRITE, Xosera READ
  RNW_READ      = HIGH    // CPU READ,  Xosera WRITE
};

enum
{
  R_XVID_RD_ADDR,        // reg 0 0000: address to read from VRAM (write-only)
  R_XVID_WR_ADDR,        // reg 1 0001: address to write from VRAM (write-only)
  R_XVID_DATA,           // reg 2 0010: read/write word from/to VRAM RD/WR
  R_XVID_DATA_2,         // reg 3 0011: read/write word from/to VRAM RD/WR (for 32-bit)
  R_XVID_VID_MODE,       // reg 4 0100: TODO video display mode (write-only)
  R_XVID_BLIT_CTRL,      // reg 5 0101: TODO blitter mode/control/status (read/write)
  R_XVID_RD_INC,         // reg 6 0110: TODO read addr increment value (write-only)
  R_XVID_WR_INC,         // reg 7 0111: TODO write addr increment value (write-only)
  R_XVID_RD_MOD,         // reg 8 1000: TODO read modulo width (write-only)
  R_XVID_WR_MOD,         // reg A 1001: TODO write modulo width (write-only)
  R_XVID_WIDTH,          // reg 9 1010: TODO width for 2D blit (write-only)
  R_XVID_COUNT,          // reg B 1011: TODO blitter "repeat" count (write-only)
  R_XVID_AUX_RD_ADDR,    // reg C 1100: TODO aux read address (font audio etc.?) (write-only)
  R_XVID_AUX_WR_ADDR,    // reg D 1101: TODO aux write address (font audio etc.?) (write-only)
  R_XVID_AUX_DATA,       // reg E 1110: TODO aux memory/register data read/write value
  R_XVID_AUX_CTRL        // reg F 1111: TODO audio and other control? (read/write)
};

inline void xvid_set_bus_read()
{
  pinMode(BUS_DATA0, INPUT);
  pinMode(BUS_DATA1, INPUT);
  pinMode(BUS_DATA2, INPUT);
  pinMode(BUS_DATA3, INPUT);
  pinMode(BUS_DATA4, INPUT);
  pinMode(BUS_DATA5, INPUT);
  pinMode(BUS_DATA6, INPUT);
  pinMode(BUS_DATA7, INPUT);
  digitalWrite(BUS_RD_RNW, RNW_READ);
}

inline void xvid_set_bus_write()
{
  digitalWrite(BUS_RD_RNW, RNW_WRITE);
  pinMode(BUS_DATA0, OUTPUT);
  pinMode(BUS_DATA1, OUTPUT);
  pinMode(BUS_DATA2, OUTPUT);
  pinMode(BUS_DATA3, OUTPUT);
  pinMode(BUS_DATA4, OUTPUT);
  pinMode(BUS_DATA5, OUTPUT);
  pinMode(BUS_DATA6, OUTPUT);
  pinMode(BUS_DATA7, OUTPUT);
}

inline void xvid_reg_num(uint8_t r)
{
  digitalWrite(BUS_REG_NUM0, r & 0x1);
  digitalWrite(BUS_REG_NUM1, r & 0x2);
  digitalWrite(BUS_REG_NUM2, r & 0x4);
  digitalWrite(BUS_REG_NUM3, r & 0x8);
}

inline void xvid_bytesel(uint8_t b)
{
  digitalWrite(BUS_BYTESEL, b);
}

inline void xvid_data_write(uint8_t data)
{
  digitalWrite(BUS_DATA0, data & 0x01);
  digitalWrite(BUS_DATA1, data & 0x02);
  digitalWrite(BUS_DATA2, data & 0x04);
  digitalWrite(BUS_DATA3, data & 0x08);
  digitalWrite(BUS_DATA4, data & 0x10);
  digitalWrite(BUS_DATA5, data & 0x20);
  digitalWrite(BUS_DATA6, data & 0x40);
  digitalWrite(BUS_DATA7, data & 0x80);
}

inline uint8_t xvid_data_read()
{
  uint8_t data = 0;
  if (digitalRead(BUS_DATA0)) data |= 0x01;
  if (digitalRead(BUS_DATA1)) data |= 0x02;
  if (digitalRead(BUS_DATA2)) data |= 0x04;
  if (digitalRead(BUS_DATA3)) data |= 0x08;
  if (digitalRead(BUS_DATA4)) data |= 0x10;
  if (digitalRead(BUS_DATA5)) data |= 0x20;
  if (digitalRead(BUS_DATA6)) data |= 0x40;
  if (digitalRead(BUS_DATA7)) data |= 0x80;
  return data;
}

inline void xvid_set_reg(uint8_t r, uint16_t value)
{
  xvid_reg_num(r);
  xvid_bytesel(0);
  xvid_data_write(value >> 8);
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
  xvid_bytesel(1);
  xvid_data_write(value & 0xff);
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
}

inline void xvid_set_regb(uint8_t r, uint8_t value)
{
  xvid_reg_num(r);
  xvid_bytesel(1);
  xvid_data_write(value & 0xff);
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
}

inline uint16_t xvid_get_reg(uint8_t r)
{
  xvid_reg_num(r);
  xvid_bytesel(0);
  xvid_set_bus_read();
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  uint8_t msb = xvid_data_read();
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
  xvid_bytesel(1);
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  uint8_t lsb = xvid_data_read();
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
  xvid_set_bus_write();
  return (msb << 8) | lsb;
}

inline uint8_t xvid_get_regb(uint8_t r, uint16_t value)
{
  xvid_reg_num(r);
  xvid_bytesel(1);
  xvid_set_bus_read();
  digitalWrite(BUS_SEL_N, CS_SELECTED);
  value = xvid_data_read();
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
  xvid_set_bus_write();
  return value;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Xosera Test Jig");
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(BUS_SEL_N, OUTPUT);
  digitalWrite(BUS_SEL_N, CS_DESELECTED);
  pinMode(BUS_RD_RNW, OUTPUT);
  digitalWrite(BUS_RD_RNW, RNW_WRITE);
  pinMode(BUS_BYTESEL, OUTPUT);
  digitalWrite(BUS_BYTESEL, LOW);
  pinMode(BUS_REG_NUM0, OUTPUT);
  digitalWrite(BUS_REG_NUM0, LOW);
  pinMode(BUS_REG_NUM1, OUTPUT);
  digitalWrite(BUS_REG_NUM1, LOW);
  pinMode(BUS_REG_NUM2, OUTPUT);
  digitalWrite(BUS_REG_NUM2, LOW);
  pinMode(BUS_REG_NUM3, OUTPUT);
  digitalWrite(BUS_REG_NUM3, LOW);

  pinMode(BUS_DATA0, OUTPUT);
  pinMode(BUS_DATA1, OUTPUT);
  pinMode(BUS_DATA2, OUTPUT);
  pinMode(BUS_DATA3, OUTPUT);
  pinMode(BUS_DATA4, OUTPUT);
  pinMode(BUS_DATA5, OUTPUT);
  pinMode(BUS_DATA6, OUTPUT);
  pinMode(BUS_DATA7, OUTPUT);

  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Starting...");
  delay(4000);

  randomSeed(0x1234);
  xvid_set_reg(R_XVID_WR_ADDR, 0);
}

uint16_t addr = 0x0000;
uint16_t data = 0x0100;
uint16_t rdata = 0x0000;
uint16_t count = 0;

uint32_t seed = 0x0001;
uint32_t feed = 0x80000BBE;

void loop()
{
  if (seed & 1)
  {
    seed = (seed >> 1) ^ feed;
  }
  else
  {
    seed = (seed >> 1);
  }

  data = (uint16_t)random(0, 0xffff);
  xvid_set_reg(R_XVID_WR_ADDR, addr);
  xvid_set_reg(R_XVID_DATA, data);
  xvid_set_reg(R_XVID_RD_ADDR, addr);
  rdata = xvid_get_reg(R_XVID_DATA);
  if (rdata != data)
  {
    Serial.print(addr, HEX);
    Serial.print(": WR=");
    Serial.print(data, HEX);
    Serial.print(" vs RD=");
    Serial.print(rdata, HEX);
    Serial.print("    \n");
  }
  if (++addr >= (106*30))
  {
    addr = 0;
  }
}
