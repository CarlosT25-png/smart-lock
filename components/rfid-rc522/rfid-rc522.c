#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "rfid-rc522.h"

//  Custom code

#define PIN_NUM_MISO 4
#define PIN_NUM_MOSI 2
#define PIN_NUM_CLK 16
#define PIN_NUM_CS 15

// add here your allowed cards
const uint8_t allowed_cards[][4] = {
    {0x17, 0x60, 0x19, 0x07}, {0x7A, 0x3D, 0x2A, 0xB5}};

const uint8_t NUM_ALLOWED_CARDS = sizeof(allowed_cards) / sizeof(allowed_cards[0]);

static const char *TAG = "RC522";

esp_err_t rfid_init(spi_device_handle_t *spi)
{
  esp_err_t ret;
  spi_bus_config_t buscfg = {
      .miso_io_num = PIN_NUM_MISO,
      .mosi_io_num = PIN_NUM_MOSI,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1};
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 1000000,
      .mode = 0,
      .spics_io_num = PIN_NUM_CS,
      .queue_size = 7,
  };

  ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  assert(ret == ESP_OK);

  ret = spi_bus_add_device(SPI2_HOST, &devcfg, spi);
  assert(ret == ESP_OK);

  PCD_Init(*spi);
  ESP_LOGI(TAG, "initialized");

  return ESP_OK;
}

bool rfid_read_access(spi_device_handle_t spi, void (*grant_access)(), void (*deny_acces)())
{
  if (PICC_IsNewCardPresent(spi))
  {
    if (PICC_Select(spi, &uid, 0) == STATUS_OK)
    {

      ESP_LOGI(TAG, "Card Scanned! UID: %02X %02X %02X %02X\n",
               uid.uidByte[0], uid.uidByte[1], uid.uidByte[2], uid.uidByte[3]);

      // check if the the card is in the allowed cards
      bool found_allowed_user = false;
      for (int i = 0; i < NUM_ALLOWED_CARDS; i++)
      {
        if (memcmp(uid.uidByte, allowed_cards[i], 4) == 0)
        {
          found_allowed_user = true;
          break;
        }
      }

      if (found_allowed_user)
      {
        ESP_LOGI(TAG, "AUTHORIZED USER DETECTED.\n");
        grant_access();
        // // put the card back to sleep so it doesn't spam the reader
        // PICC_HaltA(spi);
        // PCD_StopCrypto1(spi);
      }
      else
      {
        ESP_LOGI(TAG, "UNKNOWN USER.\n");
        deny_acces();
      }

      PICC_HaltA(spi);
      PCD_StopCrypto1(spi);
      return found_allowed_user;
    }
  }
  return false;
}

// library code

Uid uid;
enum StatusCode state;

void PCD_Version(spi_device_handle_t spi)
{
  uint8_t ver = PCD_ReadRegister(spi, VersionReg);
  printf("Version:%x\r\n", ver);
  if (ver == 0x92)
  {
    printf("MFRC522 Version 2 detected.\n");
  }
  else if (ver == 0x91)
  {
    printf("MFRC522 Version 1 detected.\n");
  }
  else
  {
    printf("Is connected device MFRC522 ? If yes, check the wiring again.\n");
    for (int i = 5; i >= 0; i--)
    {
      printf("Restarting in %d seconds...\n", i);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting Now.\n");
    fflush(stdout);
    esp_restart();
  }
}

void PCD_WriteRegister(spi_device_handle_t spi, uint8_t Register, uint8_t value)
{
  esp_err_t ret;
  static spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.flags = SPI_TRANS_USE_TXDATA;
  t.length = 16;
  t.tx_data[0] = Register;
  t.tx_data[1] = value;
  ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);
}

void PCD_WriteRegisterMany(spi_device_handle_t spi, uint8_t Register, uint8_t count, uint8_t *values)
{
  esp_err_t ret;
  uint8_t total[count + 1];
  total[0] = Register;
  for (int i = 1; i <= count; ++i)
  {
    total[i] = values[i - 1];
  }
  static spi_transaction_t t1;
  memset(&t1, 0, sizeof(t1));
  t1.length = 8 * (count + 1);
  t1.tx_buffer = total;
  ret = spi_device_transmit(spi, &t1);
  assert(ret == ESP_OK);
}

// --- CRITICAL FIX 1: Proper 16-bit SPI Read (No manual CS toggling) ---
uint8_t PCD_ReadRegister(spi_device_handle_t spi, uint8_t Register)
{
  uint8_t address = Register | 0x80;
  uint8_t tx_data[2] = {address, 0x00};
  uint8_t rx_data[2] = {0, 0};

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 16;
  t.tx_buffer = tx_data;
  t.rx_buffer = rx_data;

  esp_err_t ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);

  return rx_data[1];
}

// --- CRITICAL FIX 2: Fixed ReadMany memory corruption ---
void PCD_ReadRegisterMany(spi_device_handle_t spi, uint8_t Register, uint8_t count, uint8_t *values, uint8_t rxAlign)
{
  if (count == 0)
    return;

  uint8_t address = Register | 0x80;
  uint8_t tx_data[count + 1];
  uint8_t rx_data[count + 1];

  memset(tx_data, 0x00, count + 1);
  tx_data[0] = address;

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8 * (count + 1);
  t.tx_buffer = tx_data;
  t.rx_buffer = rx_data;

  esp_err_t ret = spi_device_transmit(spi, &t);
  assert(ret == ESP_OK);

  for (int i = 0; i < count; i++)
  {
    values[i] = rx_data[i + 1];
  }
}

void PCD_ClearRegisterBitMask(spi_device_handle_t spi, uint8_t reg, uint8_t mask)
{
  uint8_t tmp;
  tmp = PCD_ReadRegister(spi, reg);
  PCD_WriteRegister(spi, reg, tmp & (~mask));
}

void PCD_SetRegisterBitMask(spi_device_handle_t spi, uint8_t reg, uint8_t mask)
{
  uint8_t tmp;
  tmp = PCD_ReadRegister(spi, reg);
  PCD_WriteRegister(spi, reg, tmp | mask);
}

uint8_t PICC_HaltA(spi_device_handle_t spi)
{
  uint8_t result;
  uint8_t buffer[4];
  buffer[0] = PICC_CMD_HLTA;
  buffer[1] = 0;
  result = PCD_CalculateCRC(spi, buffer, 2, &buffer[2]);
  if (result != STATUS_OK)
  {
    return result;
  }
  result = PCD_TransceiveData(spi, buffer, sizeof(buffer), NULL, 0, NULL, 0, false);
  if (result == STATUS_TIMEOUT)
  {
    return STATUS_OK;
  }
  if (result == STATUS_OK)
  {
    return STATUS_ERROR;
  }
  return result;
}

void PCD_StopCrypto1(spi_device_handle_t spi)
{
  PCD_ClearRegisterBitMask(spi, Status2Reg, 0x08);
}

void PCD_Init(spi_device_handle_t spi)
{
  gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(PIN_NUM_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(50));

  PCD_WriteRegister(spi, CommandReg, 0x0F);
  vTaskDelay(pdMS_TO_TICKS(50));

  PCD_WriteRegister(spi, TModeReg, 0x80);
  PCD_WriteRegister(spi, TPrescalerReg, 0xA9);
  PCD_WriteRegister(spi, TReloadRegH, 0x03);
  PCD_WriteRegister(spi, TReloadRegL, 0xE8);
  PCD_WriteRegister(spi, TxASKReg, 0x40);
  PCD_WriteRegister(spi, ModeReg, 0x3D);

  PCD_AntennaOn(spi);

  printf("Initialization successful.\n");
}

void PCD_AntennaOn(spi_device_handle_t spi)
{
  uint8_t value = PCD_ReadRegister(spi, TxControlReg);
  if ((value & 0x03) != 0x03)
  {
    PCD_WriteRegister(spi, TxControlReg, value | 0x03);
  }
  PCD_WriteRegister(spi, RFCfgReg, (0x07 << 4)); // Set to Max Gain for Elegoo tags
  printf("Antenna turned on.\n");
}

bool PICC_IsNewCardPresent(spi_device_handle_t spi)
{
  static uint8_t bufferATQA[4] = {0, 0, 0, 0};
  uint8_t bufferSize = sizeof(bufferATQA);
  PCD_WriteRegister(spi, TxModeReg, 0x00);
  PCD_WriteRegister(spi, RxModeReg, 0x00);
  PCD_WriteRegister(spi, ModWidthReg, 0x26);

  uint8_t result = PICC_RequestA(spi, bufferATQA, &bufferSize);
  return (result == STATUS_OK || result == STATUS_COLLISION);
}

void PICC_GetTypeName(PICC_Type piccType)
{
  switch (piccType)
  {
  case PICC_TYPE_ISO_14443_4:
    printf("PICC compliant with ISO/IEC 14443-4\n");
    break;
  case PICC_TYPE_ISO_18092:
    printf("PICC compliant with ISO/IEC 18092 (NFC)\n");
    break;
  case PICC_TYPE_MIFARE_MINI:
    printf("MIFARE Mini, 320 bytes\n");
    break;
  case PICC_TYPE_MIFARE_1K:
    printf("MIFARE 1KB\n");
    break;
  case PICC_TYPE_MIFARE_4K:
    printf("MIFARE 4KB\n");
    break;
  case PICC_TYPE_MIFARE_UL:
    printf("MIFARE Ultralight or Ultralight C\n");
    break;
  case PICC_TYPE_MIFARE_PLUS:
    printf("MIFARE Plus\n");
    break;
  case PICC_TYPE_MIFARE_DESFIRE:
    printf("MIFARE DESFire\n");
    break;
  case PICC_TYPE_TNP3XXX:
    printf("MIFARE TNP3XXX\n");
    break;
  case PICC_TYPE_NOT_COMPLETE:
    printf("SAK indicates UID is not complete.\n");
    break;
  case PICC_TYPE_UNKNOWN:
  default:
    printf("Unknown type\n");
  }
}

PICC_Type PICC_GetType(uint8_t sak)
{
  sak &= 0x7F;
  switch (sak)
  {
  case 0x04:
    return PICC_TYPE_NOT_COMPLETE;
  case 0x09:
    return PICC_TYPE_MIFARE_MINI;
  case 0x08:
    return PICC_TYPE_MIFARE_1K;
  case 0x18:
    return PICC_TYPE_MIFARE_4K;
  case 0x00:
    return PICC_TYPE_MIFARE_UL;
  case 0x10:
  case 0x11:
    return PICC_TYPE_MIFARE_PLUS;
  case 0x01:
    return PICC_TYPE_TNP3XXX;
  case 0x20:
    return PICC_TYPE_ISO_14443_4;
  case 0x40:
    return PICC_TYPE_ISO_18092;
  default:
    return PICC_TYPE_UNKNOWN;
  }
}

uint8_t PICC_RequestA(spi_device_handle_t spi, uint8_t *bufferATQA, uint8_t *bufferSize)
{
  return PICC_REQA_or_WUPA(spi, PICC_CMD_REQA, bufferATQA, bufferSize);
}

// --- CRITICAL FIX 3: 7-bit Framing for Wakeup ---
uint8_t PICC_REQA_or_WUPA(spi_device_handle_t spi, uint8_t command, uint8_t *bufferATQA, uint8_t *bufferSize)
{
  uint8_t validBits;
  uint8_t status;
  if (bufferATQA == NULL || *bufferSize < 2)
  {
    return STATUS_NO_ROOM;
  }
  PCD_ClearRegisterBitMask(spi, CollReg, 0x80);
  validBits = 7;

  PCD_WriteRegister(spi, BitFramingReg, 0x07); // MUST force 7 bits

  status = PCD_TransceiveData(spi, &command, 1, bufferATQA, bufferSize, &validBits, 0, false);
  if (status != STATUS_OK)
  {
    return status;
  }
  if (*bufferSize != 2 || validBits != 0)
  {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

uint8_t PCD_TransceiveData(spi_device_handle_t spi, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen, uint8_t *validBits, uint8_t rxAlign, bool checkCRC)
{
  uint8_t waitIRq = 0x30, returnval;
  returnval = PCD_CommunicateWithPICC(spi, PCD_Transceive, waitIRq, sendData, sendLen, backData, backLen, validBits, rxAlign, checkCRC);
  return returnval;
}

// --- CRITICAL FIX 4: ESP32 Microsecond Timer ---
uint8_t PCD_CommunicateWithPICC(spi_device_handle_t spi, uint8_t command, uint8_t waitIRq, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen, uint8_t *validBits, uint8_t rxAlign, bool checkCRC)
{
  uint8_t txLastBits = validBits ? *validBits : 0;
  uint8_t bitFraming = (rxAlign << 4) + txLastBits;

  PCD_WriteRegister(spi, CommandReg, PCD_Idle);
  PCD_WriteRegister(spi, ComIrqReg, 0x7F);
  PCD_WriteRegister(spi, FIFOLevelReg, 0x80);
  int sendData_l = 0;

  for (sendData_l = 0; sendData_l < sendLen; sendData_l++)
    PCD_WriteRegister(spi, FIFODataReg, sendData[sendData_l]);

  PCD_WriteRegister(spi, BitFramingReg, bitFraming);
  PCD_WriteRegister(spi, CommandReg, command);
  if (command == PCD_Transceive)
  {
    PCD_SetRegisterBitMask(spi, BitFramingReg, 0x80);
  }

  // ESP32 Timer (Replaces Arduino 'for' loop)
  int64_t start_time = esp_timer_get_time();
  uint32_t timeout_us = 40000; // 40ms timeout
  uint8_t n = 0;

  while ((esp_timer_get_time() - start_time) < timeout_us)
  {
    n = PCD_ReadRegister(spi, ComIrqReg);
    if (n & waitIRq)
    {
      break;
    }
    if (n & 0x01)
    {
      return STATUS_TIMEOUT;
    }
  }

  if ((esp_timer_get_time() - start_time) >= timeout_us)
  {
    return STATUS_TIMEOUT;
  }

  uint8_t errorRegValue = PCD_ReadRegister(spi, ErrorReg);
  if (errorRegValue & 0x13)
  {
    return STATUS_ERROR;
  }

  uint8_t _validBits = 0;
  uint8_t h;

  if (backData && backLen)
  {
    h = PCD_ReadRegister(spi, FIFOLevelReg);
    if (h > *backLen)
    {
      return STATUS_NO_ROOM;
    }
    *backLen = h;

    int k;
    for (k = 0; k < h; k++)
    {
      *(backData + k) = PCD_ReadRegister(spi, FIFODataReg);
    }

    _validBits = PCD_ReadRegister(spi, ControlReg) & 0x07;
    if (validBits)
    {
      *validBits = _validBits;
    }
  }

  if (errorRegValue & 0x08)
  {
    return STATUS_COLLISION;
  }
  return STATUS_OK;
}

uint8_t PICC_ReadCardSerial(spi_device_handle_t spi)
{
  uint8_t result = PICC_Select(spi, &uid, 0);
  return (result == STATUS_OK);
}

uint8_t PICC_Select(spi_device_handle_t spi, Uid *uid, uint8_t validBits)
{
  uint8_t uidComplete;
  uint8_t selectDone;
  uint8_t useCascadeTag;
  uint8_t cascadeLevel = 1;
  uint8_t result;
  uint8_t count;
  uint8_t checkBit;
  uint8_t index;
  uint8_t uidIndex;
  int8_t currentLevelKnownBits;
  uint8_t buffer[9];
  uint8_t bufferUsed;
  uint8_t rxAlign;
  uint8_t txLastBits;
  uint8_t *responseBuffer = NULL;
  uint8_t responseLength;

  if (validBits > 80)
  {
    return STATUS_INVALID;
  }
  PCD_ClearRegisterBitMask(spi, CollReg, 0x80);

  uidComplete = false;
  while (!uidComplete)
  {
    switch (cascadeLevel)
    {
    case 1:
      buffer[0] = PICC_CMD_SEL_CL1;
      uidIndex = 0;
      useCascadeTag = validBits && uid->size > 4;
      break;
    case 2:
      buffer[0] = PICC_CMD_SEL_CL2;
      uidIndex = 3;
      useCascadeTag = validBits && uid->size > 7;
      break;
    case 3:
      buffer[0] = PICC_CMD_SEL_CL3;
      uidIndex = 6;
      useCascadeTag = false;
      break;
    default:
      return STATUS_INTERNAL_ERROR;
      break;
    }

    currentLevelKnownBits = validBits - (8 * uidIndex);
    if (currentLevelKnownBits < 0)
    {
      currentLevelKnownBits = 0;
    }
    index = 2;
    if (useCascadeTag)
    {
      buffer[index++] = PICC_CMD_CT;
    }
    uint8_t bytesToCopy = currentLevelKnownBits / 8 + (currentLevelKnownBits % 8 ? 1 : 0);

    if (bytesToCopy)
    {
      uint8_t maxBytes = useCascadeTag ? 3 : 4;
      if (bytesToCopy > maxBytes)
      {
        bytesToCopy = maxBytes;
      }
      for (count = 0; count < bytesToCopy; count++)
      {
        buffer[index] = uid->uidByte[uidIndex + count];
      }
    }
    if (useCascadeTag)
    {
      currentLevelKnownBits += 8;
    }

    selectDone = false;
    while (!selectDone)
    {
      if (currentLevelKnownBits >= 32)
      {
        buffer[1] = 0x70;
        buffer[6] = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
        result = PCD_CalculateCRC(spi, buffer, 7, &buffer[7]);
        if (result != STATUS_OK)
        {
          return result;
        }
        txLastBits = 0;
        bufferUsed = 9;
        responseBuffer = &buffer[6];
        responseLength = 3;
      }
      else
      {
        txLastBits = currentLevelKnownBits % 8;
        count = currentLevelKnownBits / 8;
        index = 2 + count;
        buffer[1] = (index << 4) + txLastBits;
        bufferUsed = index + (txLastBits ? 1 : 0);
        responseBuffer = &buffer[index];
        responseLength = sizeof(buffer) - index;
      }

      rxAlign = txLastBits;
      PCD_WriteRegister(spi, BitFramingReg, (rxAlign << 4) + txLastBits);

      result = PCD_TransceiveData(spi, buffer, bufferUsed, responseBuffer, &responseLength, &txLastBits, rxAlign, 0);

      if (result == STATUS_COLLISION)
      {
        uint8_t valueOfCollReg = PCD_ReadRegister(spi, CollReg);
        if (valueOfCollReg & 0x20)
        {
          return STATUS_COLLISION;
        }
        uint8_t collisionPos = valueOfCollReg & 0x1F;
        if (collisionPos == 0)
        {
          collisionPos = 32;
        }
        if (collisionPos <= currentLevelKnownBits)
        {
          return STATUS_INTERNAL_ERROR;
        }
        currentLevelKnownBits = collisionPos;
        count = currentLevelKnownBits % 8;
        checkBit = (currentLevelKnownBits - 1) % 8;
        index = 1 + (currentLevelKnownBits / 8) + (count ? 1 : 0);
        buffer[index] |= (1 << checkBit);
      }
      else if (result != STATUS_OK)
      {
        return result;
      }
      else
      {
        if (currentLevelKnownBits >= 32)
        {
          selectDone = true;
        }
        else
        {
          currentLevelKnownBits = 32;
        }
      }
    }

    index = (buffer[2] == PICC_CMD_CT) ? 3 : 2;
    bytesToCopy = (buffer[2] == PICC_CMD_CT) ? 3 : 4;

    for (count = 0; count < bytesToCopy; count++)
    {
      uid->uidByte[uidIndex + count] = buffer[index++];
    }

    if (responseLength != 3 || txLastBits != 0)
    {
      return STATUS_ERROR;
    }
    result = PCD_CalculateCRC(spi, responseBuffer, 1, &buffer[2]);
    if (result != STATUS_OK)
    {
      return result;
    }
    if ((buffer[2] != responseBuffer[1]) || (buffer[3] != responseBuffer[2]))
    {
      return STATUS_CRC_WRONG;
    }
    if (responseBuffer[0] & 0x04)
    {
      cascadeLevel++;
    }
    else
    {
      uidComplete = true;
      uid->sak = responseBuffer[0];
    }
  }

  uid->size = 3 * cascadeLevel + 1;
  return STATUS_OK;
}

bool PCD_CalculateCRC(spi_device_handle_t spi, uint8_t *data, uint8_t length, uint8_t *result)
{
  PCD_WriteRegister(spi, CommandReg, PCD_Idle);
  PCD_WriteRegister(spi, DivIrqReg, 0x04);
  PCD_WriteRegister(spi, FIFOLevelReg, 0x80);
  PCD_WriteRegisterMany(spi, FIFODataReg, length, data);
  PCD_WriteRegister(spi, CommandReg, PCD_CalcCRC);

  int64_t start_time = esp_timer_get_time();
  while ((esp_timer_get_time() - start_time) < 89000) // 89ms timeout
  {
    uint8_t n = PCD_ReadRegister(spi, DivIrqReg);
    if (n & 0x04)
    {
      PCD_WriteRegister(spi, CommandReg, PCD_Idle);
      result[0] = PCD_ReadRegister(spi, CRCResultRegL);
      result[1] = PCD_ReadRegister(spi, CRCResultRegH);
      return STATUS_OK;
    }
  }
  return STATUS_TIMEOUT;
}

void PICC_DumpToSerial(spi_device_handle_t spi, Uid *uid)
{
  MIFARE_Key key;
  PICC_DumpDetailsToSerial(uid);
  PICC_Type piccType = PICC_GetType(uid->sak);

  switch (piccType)
  {
  case PICC_TYPE_MIFARE_MINI:
  case PICC_TYPE_MIFARE_1K:
  case PICC_TYPE_MIFARE_4K:
    for (uint8_t i = 0; i < 6; i++)
    {
      key.keyByte[i] = 0xFF;
    }
    PICC_DumpMifareClassicToSerial(spi, uid, piccType, &key);
    break;
  case PICC_TYPE_MIFARE_UL:
    PICC_DumpMifareUltralightToSerial(spi);
    break;
  case PICC_TYPE_ISO_14443_4:
  case PICC_TYPE_MIFARE_DESFIRE:
  case PICC_TYPE_ISO_18092:
  case PICC_TYPE_MIFARE_PLUS:
  case PICC_TYPE_TNP3XXX:
    printf("Dumping memory contents not implemented for that PICC type.\n");
    break;
  case PICC_TYPE_UNKNOWN:
  case PICC_TYPE_NOT_COMPLETE:
  default:
    break;
  }
}

void PICC_DumpMifareClassicToSerial(spi_device_handle_t spi, Uid *uid, PICC_Type piccType, MIFARE_Key *key)
{
  uint8_t no_of_sectors = 0;
  switch (piccType)
  {
  case PICC_TYPE_MIFARE_MINI:
    no_of_sectors = 5;
    break;
  case PICC_TYPE_MIFARE_1K:
    no_of_sectors = 16;
    break;
  case PICC_TYPE_MIFARE_4K:
    no_of_sectors = 40;
    break;
  default:
    break;
  }

  if (no_of_sectors)
  {
    printf("Sector Block   0  1  2  3   4  5  6  7   8  9 10 11  12 13 14 15  AccessBits\r\n");
    for (int8_t i = no_of_sectors - 1; i >= 0; i--)
    {
      PICC_DumpMifareClassicSectorToSerial(spi, uid, key, i);
    }
  }
  PICC_HaltA(spi);
  PCD_StopCrypto1(spi);
}

void PICC_DumpMifareClassicSectorToSerial(spi_device_handle_t spi, Uid *uid, MIFARE_Key *key, uint8_t sector)
{
  uint8_t status;
  uint8_t firstBlock;
  uint8_t no_of_blocks;
  bool isSectorTrailer;

  uint8_t c1, c2, c3;
  uint8_t c1_, c2_, c3_;
  bool invertedError;
  uint8_t g[4];
  uint8_t group;
  bool firstInGroup;

  if (sector < 32)
  {
    no_of_blocks = 4;
    firstBlock = sector * no_of_blocks;
  }
  else if (sector < 40)
  {
    no_of_blocks = 16;
    firstBlock = 128 + (sector - 32) * no_of_blocks;
  }
  else
  {
    return;
  }

  uint8_t byteCount;
  uint8_t buffer[18];
  uint8_t blockAddr;
  isSectorTrailer = true;
  invertedError = false;
  for (int8_t blockOffset = no_of_blocks - 1; blockOffset >= 0; blockOffset--)
  {
    blockAddr = firstBlock + blockOffset;
    if (isSectorTrailer)
    {
      if (sector < 10)
        printf("   ");
      else
      {
        printf("  ");
        printf("%d", sector);
        printf("   ");
      }
    }
    else
    {
      printf("       ");
    }

    if (blockAddr < 10)
      printf("   ");
    else
    {
      if (blockAddr < 100)
        printf("  ");
      else
        printf(" ");
    }
    printf("%d", blockAddr);
    printf("  ");

    if (isSectorTrailer)
    {
      status = PCD_Authenticate(spi, PICC_CMD_MF_AUTH_KEY_A, firstBlock, key, uid);
      if (status != STATUS_OK)
      {
        printf("PCD_Authenticate() failed: ");
        GetStatusCodeName(status);
        return;
      }
    }

    byteCount = sizeof(buffer);
    status = MIFARE_Read(spi, blockAddr, buffer, &byteCount);
    if (status != STATUS_OK)
    {
      printf("MIFARE_Read() failed: ");
      GetStatusCodeName(status);
      printf("\n");
      continue;
    }

    for (uint8_t index = 0; index < 16; index++)
    {
      if (buffer[index] < 0x10)
        printf(" 0");
      else
        printf(" ");
      printf("%x", buffer[index]);
      if ((index % 4) == 3)
      {
        printf(" ");
      }
    }

    if (isSectorTrailer)
    {
      c1 = buffer[7] >> 4;
      c2 = buffer[8] & 0xF;
      c3 = buffer[8] >> 4;
      c1_ = buffer[6] & 0xF;
      c2_ = buffer[6] >> 4;
      c3_ = buffer[7] & 0xF;
      invertedError = (c1 != (~c1_ & 0xF)) || (c2 != (~c2_ & 0xF)) || (c3 != (~c3_ & 0xF));
      g[0] = ((c1 & 1) << 2) | ((c2 & 1) << 1) | ((c3 & 1) << 0);
      g[1] = ((c1 & 2) << 1) | ((c2 & 2) << 0) | ((c3 & 2) >> 1);
      g[2] = ((c1 & 4) << 0) | ((c2 & 4) >> 1) | ((c3 & 4) >> 2);
      g[3] = ((c1 & 8) >> 1) | ((c2 & 8) >> 2) | ((c3 & 8) >> 3);
      isSectorTrailer = false;
    }

    if (no_of_blocks == 4)
    {
      group = blockOffset;
      firstInGroup = true;
    }
    else
    {
      group = blockOffset / 5;
      firstInGroup = (group == 3) || (group != (blockOffset + 1) / 5);
    }

    if (firstInGroup)
    {
      printf(" [ ");
      printf("%d", (g[group] >> 2) & 1);
      printf(" ");
      printf("%d", (g[group] >> 1) & 1);
      printf(" ");
      printf("%d", (g[group] >> 0) & 1);
      printf(" ] ");
      if (invertedError)
      {
        printf(" Inverted access bits did not match! ");
      }
    }

    if (group != 3 && (g[group] == 1 || g[group] == 6))
    {
      uint32_t value = ((uint32_t)(buffer[3]) << 24) | ((uint32_t)(buffer[2]) << 16) | ((uint32_t)(buffer[1]) << 8) | (uint32_t)(buffer[0]);
      printf(" Value=0x");
      printf("%x", (unsigned int)value);
      printf(" Adr=0x");
      printf("%x", (unsigned int)buffer[12]);
    }
    printf("\n");
  }
}

uint8_t PCD_Authenticate(spi_device_handle_t spi, uint8_t command, uint8_t blockAddr, MIFARE_Key *key, Uid *uid)
{
  uint8_t waitIRq = 0x10;
  uint8_t status;
  uint8_t sendData[12];
  sendData[0] = command;
  sendData[1] = blockAddr;
  for (uint8_t i = 0; i < MF_KEY_SIZE; i++)
  {
    sendData[2 + i] = key->keyByte[i];
  }
  for (uint8_t i = 0; i < 4; i++)
  {
    sendData[8 + i] = uid->uidByte[i + uid->size - 4];
  }
  status = PCD_CommunicateWithPICC(spi, PCD_MFAuthent, waitIRq, &sendData[0], sizeof(sendData), NULL, 0, NULL, 0, false);
  return status;
}

void PICC_DumpDetailsToSerial(Uid *uid)
{
  printf("Card UID:");
  for (uint8_t i = 0; i < uid->size; i++)
  {
    if (uid->uidByte[i] < 0x10)
      printf(" 0");
    else
      printf(" ");
    printf("%x", uid->uidByte[i]);
  }
  printf("\r\n");

  printf("Card SAK: ");
  if (uid->sak < 0x10)
    printf(" 0");
  printf("%x\n", uid->sak);

  PICC_Type piccType = PICC_GetType(uid->sak);
  printf("PICC type: ");
  PICC_GetTypeName(piccType);
  printf("\n");
}

bool MIFARE_Read(spi_device_handle_t spi, uint8_t blockAddr, uint8_t *buffer, uint8_t *bufferSize)
{
  uint8_t result;
  if (buffer == NULL || *bufferSize < 18)
  {
    return STATUS_NO_ROOM;
  }
  buffer[0] = PICC_CMD_MF_READ;
  buffer[1] = blockAddr;
  result = PCD_CalculateCRC(spi, buffer, 2, &buffer[2]);
  if (result != STATUS_OK)
  {
    return result;
  }
  return PCD_TransceiveData(spi, buffer, 4, buffer, bufferSize, NULL, 0, true);
}

uint8_t MIFARE_Write(spi_device_handle_t spi, uint8_t blockAddr, uint8_t *buffer, uint8_t bufferSize)
{
  uint8_t result;
  if (buffer == NULL || bufferSize < 16)
  {
    return STATUS_INVALID;
  }
  uint8_t cmdBuffer[2];
  cmdBuffer[0] = PICC_CMD_MF_WRITE;
  cmdBuffer[1] = blockAddr;
  result = PCD_MIFARE_Transceive(spi, cmdBuffer, 2, false);
  if (result != STATUS_OK)
  {
    return result;
  }
  result = PCD_MIFARE_Transceive(spi, buffer, bufferSize, false);
  if (result != STATUS_OK)
  {
    return result;
  }
  return STATUS_OK;
}

uint8_t PCD_MIFARE_Transceive(spi_device_handle_t spi, uint8_t *sendData, uint8_t sendLen, bool acceptTimeout)
{
  uint8_t result;
  uint8_t cmdBuffer[18];
  if (sendData == NULL || sendLen > 16)
  {
    return STATUS_INVALID;
  }
  memcpy(cmdBuffer, sendData, sendLen);
  result = PCD_CalculateCRC(spi, cmdBuffer, sendLen, &cmdBuffer[sendLen]);
  if (result != STATUS_OK)
  {
    return result;
  }
  sendLen += 2;
  uint8_t waitIRq = 0x30;
  uint8_t cmdBufferSize = sizeof(cmdBuffer);
  uint8_t validBits = 0;
  result = PCD_CommunicateWithPICC(spi, PCD_Transceive, waitIRq, cmdBuffer, sendLen, cmdBuffer, &cmdBufferSize, &validBits, 0, false);
  if (acceptTimeout && result == STATUS_TIMEOUT)
  {
    return STATUS_OK;
  }
  if (result != STATUS_OK)
  {
    return result;
  }
  if (cmdBufferSize != 1 || validBits != 4)
  {
    return STATUS_ERROR;
  }
  if (cmdBuffer[0] != MF_ACK)
  {
    return STATUS_MIFARE_NACK;
  }
  return STATUS_OK;
}

void PICC_DumpMifareUltralightToSerial(spi_device_handle_t spi)
{
  uint8_t status;
  uint8_t byteCount;
  uint8_t buffer[18];
  uint8_t i;

  printf("Page  0  1  2  3\r\n");
  for (uint8_t page = 0; page < 16; page += 4)
  {
    byteCount = sizeof(buffer);
    status = MIFARE_Read(spi, page, buffer, &byteCount);
    if (status != STATUS_OK)
    {
      printf("MIFARE_Read() failed: \n");
      break;
    }
    for (uint8_t offset = 0; offset < 4; offset++)
    {
      i = page + offset;
      if (i < 10)
        printf("  ");
      else
        printf(" ");
      printf("%d", i);
      printf("  ");
      for (uint8_t index = 0; index < 4; index++)
      {
        i = 4 * offset + index;
        if (buffer[i] < 0x10)
          printf(" 0");
        else
          printf(" ");
        printf("%x", buffer[i]);
      }
      printf("\r\n");
    }
  }
}

void GetStatusCodeName(uint8_t code)
{
  switch (code)
  {
  case STATUS_OK:
    printf("Success.\n");
    break;
  case STATUS_ERROR:
    printf("Error in communication.\n");
    break;
  case STATUS_COLLISION:
    printf("Collission detected.\n");
    break;
  case STATUS_TIMEOUT:
    printf("Timeout in communication.\n");
    break;
  case STATUS_NO_ROOM:
    printf("A buffer is not big enough.\n");
    break;
  case STATUS_INTERNAL_ERROR:
    printf("Internal error in the code. Should not happen.\n");
    break;
  case STATUS_INVALID:
    printf("Invalid argument.\n");
    break;
  case STATUS_CRC_WRONG:
    printf("The CRC_A does not match.\n");
    break;
  case STATUS_MIFARE_NACK:
    printf("A MIFARE PICC responded with NAK.\n");
    break;
  default:
    printf("Unknown error\n");
  }
}