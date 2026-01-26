#ifndef AD7705_H
#define AD7705_H

/*
Sincronizzazione tra isr e getRawData mediante semaforo.

Perché questa soluzione è superiore?
1. Thread Safety: Non c'è rischio che la CPU legga il dato mentre l'ISR lo sta scrivendo (race condition),
   perché l'accesso logico è mediato dal semaforo.

2. Zero spreco di CPU: Se volessi aspettare un dato, potresti fare
   xSemaphoreTake(_dataSem, pdMS_TO_TICKS(100)). Il task dell'ESP32 andrebbe in "sleep" liberando la CPU
   per altre attività (come il Wi-Fi) finché l'ADC non risponde.

3. Pulizia: rimosso _newDataAvailable (booleano), poiché il semaforo funge sia da protezione
   che da indicatore di stato.

*/

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class AD7705_ESP32
{
public:
    enum Channel
    {
        CH1 = 0x0,
        CH2 = 0x1
    };
    // modalità funzionamento del canale
    enum Mode
    {
        BIPOLAR = 0x0,        
        UNIPOLAR = 0x1
    };
    // guadagni di tensione nel buffer dell'ADC
    enum Gain
    {
        G1 = 0,
        G2 = 1,
        G4 = 2,
        G8 = 3,
        G16 = 4,
        G32 = 5,
        G64 = 6,
        G128 = 7
    };
    // Velocità di output regolate per clock a 4.9152 MHz (CLKDIV=1)
    // Nel file .h aggiorniamo le costanti per chiarezza
    enum UpdateRate
    {
        RATE_50HZ = 0x00,  // FS1=0, FS0=0
        RATE_60HZ = 0x01,  // FS1=0, FS0=1
        RATE_250HZ = 0x02, // FS1=1, FS0=0
        RATE_500HZ = 0x03  // FS1=1, FS0=1
    };

    AD7705_ESP32(SPIClass &spiBus, uint8_t csPin, uint8_t drdyPin, uint8_t rstPin);

    bool begin(UpdateRate rate = RATE_50HZ);
    void hardReset(); // Nuovo metodo per il reset fisico
    void setConfig(Channel ch, Mode mode, Gain gain);
    void setVRef(float vRefMv) { _vRef = vRefMv; }
    bool calibrate(Channel ch);
    void getCalibrationData(Channel ch);

    // Gestione dati
    void selectChannel(Channel ch);
    bool available();
    int16_t getRawData();
    float getVoltage();
    uint8_t readRegister(uint8_t regAddrRead);

private:
    SPIClass *_spi;
    uint8_t _cs, _drdy, _rst;
    float _vRef = 2500.0;

    Mode _chMode[2];
    Gain _chGain[2];
    Channel _activeCh = CH1;
    UpdateRate _currentRate;

    static AD7705_ESP32 *_instance;
    volatile int16_t _lastRawData;

    static void IRAM_ATTR isr();
    void writeRegister(uint8_t reg, uint8_t value);

    void reset();
    // Verifica se il chip risponde
    bool checkConnection();
    // All'interno della classe, nella sezione private:
    SemaphoreHandle_t _dataSem;
};

#endif