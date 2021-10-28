#pragma once
#include "Arduino.h"
#include <Wire.h>

/*
   Мини - класс для работы с микросхемой INA219
   НЕ ЯВЛЯЕТСЯ библиотекой
   НЕ РЕКОМЕНДУЕТСЯ к использованию отдельно от проекта voltamperemeter
*/

#define INA219_ADDRESS (0x40)

#define INA219_REG_CFG (0x00)
#define INA219_CFG_RESET (0x8000)
#define INA219_REG_SHUNTVOLTAGE (0x01)
#define INA219_REG_BUSVOLTAGE (0x02)
#define INA219_REG_POWER (0x03)
#define INA219_REG_CURRENT (0x04)
#define INA219_REG_CALIBRATION (0x05)

#define INA219_CFG_BVOLTAGERANGE_16V 0x0000
#define INA219_CFG_BVOLTAGERANGE_32V 0x2000
#define INA219_CFG_GAIN_MASK (0x1800)
#define INA219_CFG_GAIN_1_40MV 0x0000
#define INA219_CFG_GAIN_2_80MV 0x0800
#define INA219_CFG_GAIN_4_160MV 0x1000
#define INA219_CFG_GAIN_8_320MV 0x1800

#define INA219_CFG_BAD_MASK (0x0780)
#define INA219_CFG_BAD_9BIT  0x0000
#define INA219_CFG_BAD_10BIT 0x0080
#define INA219_CFG_BAD_11BIT 0x0100
#define INA219_CFG_BAD_12BIT 0x0180
#define INA219_CFG_BAD_12BIT_2S_1060US 0x0480
#define INA219_CFG_BAD_12BIT_4S_2130US 0x0500
#define INA219_CFG_BAD_12BIT_8S_4260US 0x0580
#define INA219_CFG_BAD_12BIT_16S_8510US 0x0600
#define INA219_CFG_BAD_12BIT_32S_17MS 0x0680
#define INA219_CFG_BAD_12BIT_64S_34MS 0x0700
#define INA219_CFG_BAD_12BIT_128S_69MS 0x0780

#define INA219_CFG_SAD_MASK (0x0078)
#define INA219_CFG_SAD_9BIT_1S_84US 0x0000
#define INA219_CFG_SAD_10BIT_1S_148US 0x0008
#define INA219_CFG_SAD_11BIT_1S_276US 0x0010
#define INA219_CFG_SAD_12BIT_1S_532US 0x0018
#define INA219_CFG_SAD_12BIT_2S_1060US 0x0048
#define INA219_CFG_SAD_12BIT_4S_2130US 0x0050
#define INA219_CFG_SAD_12BIT_8S_4260US 0x0058
#define INA219_CFG_SAD_12BIT_16S_8510US 0x0060
#define INA219_CFG_SAD_12BIT_32S_17MS 0x0068
#define INA219_CFG_SAD_12BIT_64S_34M 0x0070
#define INA219_CFG_SAD_12BIT_128S_69MS 0x0078

#define INA219_CFG_MODE_MASK (0x0007)
#define INA219_CFG_MODE_POWERDOWN 0x00
#define INA219_CFG_MODE_SVOLT_TRIGGERED 0x01
#define INA219_CFG_MODE_BVOLT_TRIGGERED 0x02
#define INA219_CFG_MODE_SANDBVOLT_TRIGGERED 0x03
#define INA219_CFG_MODE_ADCOFF 0x04
#define INA219_CFG_MODE_SVOLT_CONTINUOUS 0x05
#define INA219_CFG_MODE_BVOLT_CONTINUOUS 0x06
#define INA219_CFG_MODE_SANDBVOLT_CONTINUOUS 0x07


class INA219 {
  public:

    INA219(uint8_t addr = INA219_ADDRESS);
    bool begin();
    void setCalibration(uint16_t cal);
    uint16_t getVoltage();
    float getCurrent();

  private:
    uint16_t readRegister16(uint8_t addr);
    void writeRegister16(uint8_t addr, uint16_t data);

    uint8_t _iic_addr = 0;
    uint16_t _cal_current = 0;
    uint32_t _div_current = 0;
};

INA219::INA219(uint8_t addr) {
  _iic_addr = addr;
  _div_current = 0;
}

bool INA219::begin() {
  Wire.begin();
  Wire.beginTransmission(_iic_addr);
  if (Wire.endTransmission() != 0) return false;
  setCalibration(4096);
  return true;
}

uint16_t INA219::getVoltage() {
  uint16_t value = readRegister16(INA219_REG_BUSVOLTAGE);
  return (uint16_t)((value >> 3) << 2);
}

float INA219::getCurrent() {
  writeRegister16(INA219_REG_CALIBRATION, _cal_current);
  uint16_t value = readRegister16(INA219_REG_CURRENT);
  return  value / _div_current;
}

void INA219::setCalibration(uint16_t cal) {
  _cal_current = cal;
  _div_current = 10; // 100uA / bit (1000/100 = 10)

  writeRegister16(INA219_REG_CALIBRATION, _cal_current);

  uint16_t _config =
    INA219_CFG_BVOLTAGERANGE_32V |
    INA219_CFG_BAD_12BIT |
    INA219_CFG_SAD_12BIT_128S_69MS |
    INA219_CFG_MODE_SANDBVOLT_CONTINUOUS;

  if (_MAX_VSHUNT > 0.160f) {
    _config |= INA219_CFG_GAIN_8_320MV;
  } else if (_MAX_VSHUNT > 0.80f) {
    _config |= INA219_CFG_GAIN_4_160MV;
  } else if (_MAX_VSHUNT > 0.40f) {
    _config |= INA219_CFG_GAIN_2_80MV;
  } else {
    _config |= INA219_CFG_GAIN_1_40MV;
  }
  
  writeRegister16(INA219_REG_CFG, _config);
}

uint16_t INA219::readRegister16(uint8_t addr) {
  uint16_t result = 0;
  Wire.beginTransmission(_iic_addr);
  Wire.write(addr);
  Wire.endTransmission();
  Wire.requestFrom(_iic_addr, 2);
  result = Wire.read() << 8 | Wire.read();
  return result;
}

void INA219::writeRegister16(uint8_t addr, uint16_t data) {
  Wire.beginTransmission(_iic_addr);
  Wire.write(addr);
  Wire.write(highByte(data));
  Wire.write(lowByte(data));
  Wire.endTransmission();
}
