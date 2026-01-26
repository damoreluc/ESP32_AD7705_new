#include <Arduino.h>
#include "./AD7705/AD7705.h"

// Impostazioni SPI: 2MHz, MSB First, SPI Mode 3 (CPOL=1, CPHA=1 tipico per AD7705)
static SPISettings ad7705Settings(1000000, MSBFIRST, SPI_MODE3);
// static SPISettings ad7705Settings(500000, MSBFIRST, SPI_MODE3);

AD7705_ESP32 *AD7705_ESP32::_instance = nullptr;

AD7705_ESP32::AD7705_ESP32(SPIClass &spiBus, uint8_t csPin, uint8_t drdyPin, uint8_t rstPin)
    : _spi(&spiBus), _cs(csPin), _drdy(drdyPin), _rst(rstPin)
{
    _instance = this;
}

void AD7705_ESP32::hardReset()
{
    digitalWrite(_rst, LOW);
    delay(2); // Il datasheet richiede min 100ns, 2ms sono ultra-sicuri
    digitalWrite(_rst, HIGH);
    delay(10); // Tempo di assestamento post-reset
}

bool AD7705_ESP32::begin(UpdateRate rate)
{
    _currentRate = rate;
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    pinMode(_rst, OUTPUT);    // Configura il pin di reset
    digitalWrite(_rst, HIGH); // Stato normale: Alto
    pinMode(_drdy, INPUT_PULLUP);

    _spi->begin();

    // Esegui il reset hardware invece di quello software
    hardReset();

    // Verifica hardware
    if (!checkConnection())
    {
        Serial.println("ERRORE: AD7705 non risponde dopo Hard Reset!");
        return false;
    }

    // Configurazione clock: CLKDIV=1 per 4.9152MHz + Rate scelto
    // 0x08 è il bit CLK (bit 3), CLKDIV è il bit 4 (0x10)
    // Per 4.9152MHz con CLKDIV=1: 0x08 | 0x04 | rate = 0x0C | rate
    writeRegister(0x20, 0x0C | _currentRate);

    _dataSem = xSemaphoreCreateBinary();
    attachInterrupt(digitalPinToInterrupt(_drdy), isr, FALLING);

    return true;
}

// Poiché l'AD7705 non ha un registro "WHO_AM_I", il modo migliore per testarlo è scrivere un valore
// in un registro (es. il Clock Register) e rileggerlo per vedere se i dati coincidono.
bool AD7705_ESP32::checkConnection()
{
    uint8_t testVal = 0x0D;       // Configurazione test per il Clock Register
    writeRegister(0x20, testVal); // Scrittura

    // Lettura di verifica: registro 0x20 in lettura è 0x28 (Comms: RS2,RS1,RS0 = 100)
    _spi->beginTransaction(ad7705Settings);
    digitalWrite(_cs, LOW);
    _spi->transfer(0x28);
    uint8_t readVal = _spi->transfer(0x00);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();

    return (readVal == testVal);
}

void AD7705_ESP32::setConfig(Channel ch, Mode mode, Gain gain)
{
    _chMode[ch] = mode;
    _chGain[ch] = gain;

    // Forza l'attivazione del clock prima del setup
    writeRegister(0x20, 0x08 | _currentRate); // Registro 0x20: Clock attivo

    // Comunicazione: seleziona setup register per il canale (0x10 | ch)
    // Setup Register: Self-cal=0, Gain, B/U, FSync=1 (Reset filtro)
    uint8_t buf = 0;
    uint8_t FSync = 0x01;
    // uint8_t setupByte = (gain << 3) | (mode << 2) | 0x02; // FSync = 1
    uint8_t setupByte = (gain << 3) | (mode << 2) | (buf << 1) | FSync; // FSync = 1
    writeRegister(0x10 | ch, setupByte);
    delay(2); // Attendi un paio di ms per il reset del filtro

    // Rilascia FSync per far partire le conversioni
    writeRegister(0x10 | ch, setupByte & 0xFE);
    delay(2);

    // STEP 3: Avvia la Self-Calibration (MD1=0, MD0=1)
    // Questa operazione mette automaticamente FSYNC a 0 internamente se non lo è,
    // ma è buona norma averlo già rilasciato.
    uint8_t calSetup = 0x40 | (gain << 3) | (mode << 2); // 0x40 imposta MD1=0, MD0=1
    writeRegister(0x10 | ch, calSetup);

    // STEP 4: Attendi la fine della calibrazione
    // Bisogna aspettare che il pin DRDY vada LOW
    delay(10);
    uint32_t timeout = millis();
    while (digitalRead(_drdy) == HIGH)
    {
        if (millis() - timeout > 3000)
        {
            Serial.println("Timeout Calibrazione!");
            break;
        }
    }
    // Al termine della calibrazione, i bit MD1 e MD0 tornano automaticamente a 0
}

void AD7705_ESP32::getCalibrationData(Channel ch)
{
    uint32_t offset = 0;
    uint32_t gain = 0;

    _spi->beginTransaction(ad7705Settings);

    digitalWrite(_cs, LOW);
    for(int i=0; i<5; i++) _spi->transfer(0xFF); 
    digitalWrite(_cs, HIGH);

    // 1. Leggi il Registro di OFFSET (Zero-Scale)
    // Registro 011 -> Per CH1 è 0x30, per CH2 è 0x31 (RW=1)
    digitalWrite(_cs, LOW);
    _spi->transfer(0x30 | ch);
    delayMicroseconds(10);

// Leggiamo i 3 byte e componiamo il 24 bit
    offset = (uint32_t)_spi->transfer(0x00) << 16;
    offset |= (uint32_t)_spi->transfer(0x00) << 8;
    offset |= (uint32_t)_spi->transfer(0x00);
    digitalWrite(_cs, HIGH);

    // 2. Leggi il Registro di GAIN (Full-Scale)
    // Registro 100 -> Per CH1 è 0x34, per CH2 è 0x35 (RW=1)
    digitalWrite(_cs, LOW);
    _spi->transfer(0x34 | ch);
    delayMicroseconds(10);
    
    gain = (uint32_t)_spi->transfer(0x00) << 16;
    gain |= (uint32_t)_spi->transfer(0x00) << 8;
    gain |= (uint32_t)_spi->transfer(0x00);

    digitalWrite(_cs, HIGH);
    _spi->endTransaction();

    // Stampa i risultati per il debug
    Serial.printf("--- Calibrazione Canale %d ---\n", ch + 1);
    Serial.printf("Offset (Zero-Scale): 0x%06X\n", offset);
    Serial.printf("Gain   (Full-Scale): 0x%06X\n", gain);
    Serial.println("-----------------------------");
}

// La calibrazione può fallire se il riferimento di tensione è assente
// o se i pin analogici sono fuori range. Aggiungo un timeout di 2 secondi.
bool AD7705_ESP32::calibrate(Channel ch)
{
    // 1. Disabilita temporaneamente l'interrupt per evitare interferenze
    detachInterrupt(digitalPinToInterrupt(_drdy));
    // 2. Svuota il semaforo
    xSemaphoreTake(_dataSem, 0);

    // 3. Avvia Self-Calibration
    uint8_t setupByte = (_chGain[ch] << 3) | (_chMode[ch] << 2) | 0x40;
    writeRegister(0x10 | ch, setupByte);

    // 4. Attesa del pin DRDY (la calibrazione richiede circa 9 cicli di output)
    unsigned long start = millis();
    bool success = false;

    while (millis() - start < 2000)
    { // Timeout 2 secondi
        if (digitalRead(_drdy) == LOW)
        {
            success = true;
            break;
        }
        delay(1);
    }

    if (!success)
    {
        Serial.printf("ERRORE: Timeout calibrazione Canale %d\n", ch + 1);
        return false;
    }

    // 5. Pulizia finale del semaforo per evitare interrupt spuri post-calibrazione
    delay(10);
    xSemaphoreTake(_dataSem, 0);

    // 6. Riattiva l'interrupt
    attachInterrupt(digitalPinToInterrupt(_drdy), isr, FALLING);

    return true;
}

void AD7705_ESP32::selectChannel(Channel ch)
{
    if (_activeCh != ch)
    {
        _activeCh = ch;
        // Cambio canale con FSync per pulire il filtro
        uint8_t setupByte = (_chGain[ch] << 3) | (_chMode[ch] << 2) | 0x02;
        writeRegister(0x10 | ch, setupByte);        // FSync = 1
        writeRegister(0x10 | ch, setupByte & 0xFD); // FSync = 0
    }
}

void IRAM_ATTR AD7705_ESP32::isr()
{
    if (_instance)
    {
        // notifiche alla getRawData
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        _instance->_spi->beginTransaction(ad7705Settings);
        digitalWrite(_instance->_cs, LOW);

        _instance->_spi->transfer(0x38 | _instance->_activeCh);
        uint16_t msb = _instance->_spi->transfer(0x00);
        uint16_t lsb = _instance->_spi->transfer(0x00);

        _instance->_lastRawData = (int16_t)((msb << 8) | lsb);
        digitalWrite(_instance->_cs, HIGH);
        _instance->_spi->endTransaction();

        // Notifica che il dato è disponibile
        xSemaphoreGiveFromISR(_instance->_dataSem, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

float AD7705_ESP32::getVoltage()
{
    // float gainVal = pow(2, _chGain[_activeCh]);
    float gainVal = (float)(1 << _chGain[_activeCh]);

    if (_chMode[_activeCh] == UNIPOLAR)
    {
        // Trattiamo raw come unsigned 16 bit (0 to 65535)
        uint16_t uRaw = (uint16_t)_lastRawData;
        return (uRaw * _vRef) / (65535.0f * gainVal);
    }
    else
    {
        // In modalità bipolare, l'ADC usa Offset Binary (0x8000 = 0V)
        // Convertiamo in Signed Complemento a 2:
        int32_t sRaw = (int32_t)_lastRawData - 32768;
        return (sRaw * _vRef) / (32768.0f * gainVal);
    }
}

int16_t AD7705_ESP32::getRawData()
{
    // Prende il semaforo (timeout 0 = non bloccante)
    if (xSemaphoreTake(_dataSem, 0) == pdTRUE)
    {
        return _lastRawData;
    }
    return _lastRawData; // Ritorna l'ultimo dato comunque, ma il flag è consumato
}

bool AD7705_ESP32::available()
{
    void *temp;
    // peek controlla se il semaforo c'è senza rimuoverlo
    return (xQueuePeek(_dataSem, &temp, 0) == pdTRUE);
}

void AD7705_ESP32::writeRegister(uint8_t reg, uint8_t value)
{
    _spi->beginTransaction(ad7705Settings);
    digitalWrite(_cs, LOW);
    _spi->transfer(reg);
    _spi->transfer(value);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
    delayMicroseconds(10); // Piccolo tempo di recupero per l'ADC
}

uint8_t AD7705_ESP32::readRegister(uint8_t regAddrRead)
{
    uint8_t commByte = 0x28 | regAddrRead; // RS2,RS1,RS0 = 100 per lettura registro
    _spi->beginTransaction(ad7705Settings);
    digitalWrite(_cs, LOW);
    _spi->transfer(regAddrRead);
    uint8_t val = _spi->transfer(0x00);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
    return val;
}

void AD7705_ESP32::reset()
{
    _spi->beginTransaction(ad7705Settings);
    digitalWrite(_cs, LOW);
    // Invia 32 bit a 1 per resettare l'interfaccia seriale dell'ADC
    for (int i = 0; i < 5; i++)
        _spi->transfer(0xFF);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}