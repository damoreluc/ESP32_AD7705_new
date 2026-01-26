/*
Esempio completo di main.cpp che mette alla prova tutte le funzionalità della classe gemini_AD7705:
configurazione dei due canali con diversi guadagni,
calibrazione e lettura asincrona sfruttando il semaforo.

In questo esempio, l'ESP32 configurerà il Canale 1 per una lettura unipolare (es. un sensore di pressione)
e il Canale 2 per una lettura bipolare (es. uno shunt per corrente o un sensore a ponte).


Osservazioni relative alla flessibilità della classe:
1. Multi-Configurazione:
   il Canale 2 ha un guadagno di 8. Grazie alla formula integrata nella classe, getVoltage() restituirà il valore corretto in mV tenendo conto che il range dinamico è più stretto ($2500 / 8$).
2. Gestione del Cambio Canale: Quando chiami selectChannel(), la classe attiva il bit FSYNC.
   Questo resetta il filtro Sinc³. L'interrupt non scatterà finché il filtro non si sarà stabilizzato sul nuovo segnale del canale scelto, evitando di leggere dati "sporchi" del canale precedente.
3. Efficienza: Il loop() non contiene delay(). La funzione available() è estremamente veloce
   perché interroga solo lo stato del semaforo di FreeRTOS senza impegnare il bus SPI se non c'è nulla
   da leggere.
*/

#include <Arduino.h>
#include <SPI.h>
#include "./AD7705/AD7705.h"

// Definizione dei pin per ESP32
#define PIN_CS 5
#define PIN_DRDY 16
#define PIN_RST 17
#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCK 18

// Istanza della classe usando la VSPI di default
AD7705_ESP32 adc(SPI, PIN_CS, PIN_DRDY, PIN_RST);

AD7705_ESP32::Mode currentMode = AD7705_ESP32::UNIPOLAR;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Attendi l'apertura del monitor seriale

    Serial.println("--- AD7705 ESP32 Test ---");

    // Inizializza l'ADC (Configura SPI, Interrupt e Clock interno)
    // Inizializzazione a 60Hz per eliminare il rumore di rete (USA/Giappone)
    // o 50Hz per Europa.
    if (!adc.begin(AD7705_ESP32::RATE_50HZ))
    {
        Serial.println("Blocco sistema: hardware non trovato.");
        while (1)
            ; // Stop se l'hardware non c'è
    }
    
    Serial.println("ADC inizializzato.");
    // Registro clock
    Serial.print("Clock Reg  (0x0C if at 50Hz): ");
    Serial.println(adc.readRegister(0x28), HEX); // 0x28 è il comando per leggere il Clock Reg

    // Imposta la tensione di riferimento reale misurata sul pin VREF (es. 2.5V)
    adc.setVRef(2500.0);

    // Configurazione CANALE 1: Unipolare, Guadagno 1 (Range 0-2.5V)
    Serial.println("Configurazione Canale 1...");
    adc.setConfig(AD7705_ESP32::CH1, currentMode, AD7705_ESP32::G2);

    // debug rapido: lettura del Setup Register CH1
    delay(2);
    Serial.print("Setup Reg CH1: ");
    Serial.println(adc.readRegister(0x18), HEX); // 0x18 è il comando per leggere Setup Reg del CH1

    // Esegui calibrazione (richiede tempo, l'ADC campiona zero e fondo scala)
    Serial.println("Calibrazione Canale 1 in corso...");
    if (adc.calibrate(AD7705_ESP32::CH1))
    {
        delay(2);
        adc.getCalibrationData(AD7705_ESP32::CH1);
        Serial.println("Canale 1 pronto.");
    }
    else
    {
        Serial.println("Errore critico calibrazione.");
        while (1)
            ;
    }

    // Configurazione CANALE 2: Bipolare, Guadagno 8 (Range +/- 312.5mV)
    Serial.println("Configurazione Canale 2...");
    adc.setConfig(AD7705_ESP32::CH2, AD7705_ESP32::UNIPOLAR, AD7705_ESP32::G2);
    
    // debug rapido: lettura del Setup Register CH2
    delay(2);
    Serial.print("Setup Reg CH2: ");
    Serial.println(adc.readRegister(0x19), HEX); // 0x19 è il comando per leggere Setup Reg del CH9

    Serial.println("Calibrazione Canale 2 in corso...");
    if (adc.calibrate(AD7705_ESP32::CH2))
    {
        delay(2);
        adc.getCalibrationData(AD7705_ESP32::CH2);
        Serial.println("Canale 2 pronto.");
    }
    else
    {
        Serial.println("Errore critico calibrazione.");
        while (1)
            ;
    }

    // Inizializza il servizio ISR per i GPIO
    Serial.println("Inizio acquisizione...");

    // Seleziona il canale iniziale
    adc.selectChannel(AD7705_ESP32::CH1);
}

unsigned long lastSwitchTime = 0;
AD7705_ESP32::Channel currentCh = AD7705_ESP32::CH1;

void loop()
{
    // Verifichiamo se il semaforo è stato rilasciato dall'ISR
    if (adc.available())
    {
        int16_t raw = adc.getRawData();
        float voltage = adc.getVoltage();

        Serial.print("CH: ");
        Serial.print(currentCh == AD7705_ESP32::CH1 ? "1" : "2");
        Serial.print(" | Raw: ");
        // Serial.print(raw);
        if (currentMode == AD7705_ESP32::UNIPOLAR)
        {
            Serial.print((uint16_t)raw, HEX); // Forza la stampa come numero positivo 0-65535
        }
        else
        {
            Serial.print(raw); // Stampa correttamente con segno -32768 a +32767
        }
        Serial.print(" | Voltage: ");
        Serial.print(voltage, 4);
        Serial.println(" mV");
    }

    // Esempio di commutazione canale ogni 5 secondi
    if (millis() - lastSwitchTime > 5000)
    {
        if (currentCh == AD7705_ESP32::CH1)
        {
            currentCh = AD7705_ESP32::CH2;
        }
        else
        {
            currentCh = AD7705_ESP32::CH1;
        }

        Serial.print("\nCommutazione su Canale ");
        Serial.println(currentCh == AD7705_ESP32::CH1 ? "1" : "2");

        adc.selectChannel(currentCh);
        lastSwitchTime = millis();
    }

    // Il loop è libero di fare altro (es. gestire Wi-Fi o display)
    // L'ADC lavora "in sottofondo" tramite interrupt
    delay(100);
}