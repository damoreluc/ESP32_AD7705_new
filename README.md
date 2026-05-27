# AD7705_ESP32 Library
Libreria ottimizzata per l'uso dell'ADC Sigma-Delta a 16 bit AD7705 con microcontrollori ESP32. 
La libreria gestisce la comunicazione SPI a bassa velocità (fino a 1MHz) per garantire la massima stabilità del segnale, supporta due canali differenziali indipendenti e include procedure di calibrazione automatica.

### Caratteristiche
* __Risoluzione__: 16 bit.
* __Canali__: 2 canali differenziali (AIN1, AIN2).
* __Calibrazione__: Self-calibration interna per offset e guadagno.
* __Filtro Digitale__: Configurato per 50Hz (reiezione disturbi di rete).
* __Compatibilità__: Progettata specificamente per i livelli logici 3.3V dell'ESP32.

### Collegamenti Hardware (Pinout)
Sulla maggior parte dei moduli (es. QYF-973), assicurarsi dei seguenti collegamenti:

|AD7705 | PinESP32 Pin (Default) | Descrizione|
|--- |--- |--- |
| VCC | 3.3V / 5V | Alimentazione |
| GND | GND | Massa comune |
| SCLK | GPIO 18 | Clock SPI |
| MISO | GPIO 19 | Master In Slave Out |
| MOSI | GPIO 23 | Master Out Slave In |
| CS | GPIO 5 (o scelto dall'utente) | Chip Select |
| DRDY | GPIO 4 (o scelto dall'utente) | Data Ready |
| RST | GPIO 17 (o scelto dall'utente) | Chip reset |

__IMPORTANTE__ Riferimento di massa: Se si effettuano misurazioni riferite a massa (Single-Ended), i pin AIN1- e AIN2- devono essere collegati fisicamente a GND.

## Enumeratori della classe

Canali:
```C
enum Channel
{
    CH1 = 0x0,
    CH2 = 0x1
};
```

Modalità funzionamento del canale:
```C
enum Mode
{
    BIPOLAR = 0x0,        
    UNIPOLAR = 0x1
};
```

Guadagni di tensione nel buffer dell'ADC:
```C
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
```

Velocità di output regolate per clock a 4.9152 MHz (CLKDIV=1)
```C
enum UpdateRate
{
    RATE_50HZ = 0x00,  // FS1=0, FS0=0
    RATE_60HZ = 0x01,  // FS1=0, FS0=1
    RATE_250HZ = 0x02, // FS1=1, FS0=0
    RATE_500HZ = 0x03  // FS1=1, FS0=1
};
```

## Metodi della Classe

### Istanza della classe

| Metodo | Descrizione |
|--- |--- |
| `AD7705_ESP32(SPIClass &spiBus, uint8_t csPin, uint8_t drdyPin, uint8_t rstPin)` | Crea l'istanza di un oggetto di classe AD7705_ESP32. |

__Parametri__

| Parametro | Descrizione |
|--- |--- |
| `SPIClass &spiBus` |  Riferimento alla porta SPI: HSPI o VSPI |
| `uint8_t csPin` | Pin di chip select |
| `uint8_t drdyPin` | Pin per il segnale Data Ready |
| `uint8_t rstPin` | Pin per il reset hardware |

__Esempio__
```C
// Definizione dei pin per ESP32
#define PIN_CS 5
#define PIN_DRDY 16
#define PIN_RST 17
#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCK 18

// creazione dell'oggetto adc come istanza della classe usando la VSPI
AD7705_ESP32 adc(VSPI, PIN_CS, PIN_DRDY, PIN_RST);
```

### Inizializzazione e Configurazione

| Metodo | Descrizione |
|--- |--- |
| `begin(SPIClass &spi)` | Inizializza l'interfaccia SPI indicata dal parametro `spi`, sincronizza il chip e imposta il clock interno a 4.9152MHz per un output rate di 50Hz |
| `hardReset()` | Comanda il pin RST per resettare l'interfaccia seriale dell'AD7705 in caso di desincronizzazione |
| `setConfig(Channel ch, Mode mode, Gain gain)`| Configura il canale specifico.<br>Mode: UNIPOLAR (1) o BIPOLAR (0).<br>Gain: Da G1 a G128. |
| `setVRef(float vRefMv)` | Imposta la tensione di riferimento, espressa in $mV$ |

### Operazioni di Misura

| Metodo | Descrizione |
|--- |--- |
| `selectChannel(Channel ch)` | Seleziona il canale sul quale svolgere le operazioni di lettura seguenti |
| `calibrate(Channel ch)` | Esegue la calibrazione interna. Il pin DRDY rimarrà alto fino al completamento |
| `available()` | Restituisce true se il dato è pronto per essere letto (DRDY LOW) |
| `getRawData()` | Legge il valore grezzo a 16 bit (0-65535 in unipolare) del canale selezionato |
| `getVoltage()` | Legge il valore della tensione in $mV$ sul canale selezionato in base alla $V_{ref}$ fornita (default 2.50V) |

### Diagnostica

| Metodo | Descrizione |
|--- |--- |
| `getCalibrationData(Channel ch)` | Legge e stampa su Serial i registri interni di Offset e Gain a 24 bit. Utile per verificare se la calibrazione ha avuto successo |

### Note Tecniche e Troubleshooting
1. __Valori Raw Bloccati__: Se il valore raw non cambia, verificare il pin DRDY. Se non pulsa mai a 50Hz, il chip non è configurato correttamente o il clock non è attivo.

2. __Precisione__: L'AD7705 è molto sensibile al rumore. Utilizzare condensatori di bypass da 0.1µF tra VCC e GND il più vicino possibile al chip.

3. __SPI Speed__: La libreria è impostata a 1MHz nel metodo `begin()`. Non aumentare oltre 1MHz se si riscontrano errori nella lettura dei registri a 24 bit.

## Formule di Conversione
L'AD7705 converte la tensione analogica in un valore digitale a 16 bit ($2^{16} = 65536$ livelli). La formula varia a seconda della modalità impostata (UNIPOLAR o BIPOLAR).

1. Modalità Unipolare (UNIPOLAR)
In questa modalità, l'ADC accetta tensioni da $0$ a $V_{ref}/Gain$. Il valore RAW $0x0000$ corrisponde a $0V$, mentre $0xFFFF$ corrisponde al fondo scala.

$$V_{in} = \frac{ValoreRaw \cdot V_{ref}}{Gain \cdot 65535}$$

Esempio: Con $V_{ref} = 2.5V$, $Gain = 1$ e $ValoreRaw = 0x4590$ (17808):

$$V_{in} = \frac{17808 \cdot 2.5}{1 \cdot 65535} \approx 0.6793V$$

2. Modalità Bipolare (BIPOLAR)
In questa modalità, l'ADC utilizza un codice a offset binario. Il valore RAW $0x8000$ ($32768$) corrisponde allo zero ideale ($0V$). I valori inferiori sono tensioni negative, quelli superiori sono positivi.

$$V_{in} = \frac{(ValoreRaw - 32768) \cdot V_{ref}}{Gain \cdot 32768}$$

Range: La tensione misurabile spazia da $-V_{ref}/Gain$ a $+V_{ref}/Gain$.


### Tabella dei Range di Misura ($V_{ref} = 2.5V$)

| Guadagno (Gain) | Range Unipolare | Range Bipolare | Risoluzione (LSB Unip.) |
|--- |--- |--- |--- |
| G1 | 0…2.5V | ±2.5V | 38.15μV |
| G2 | 0…1.25V | ±1.25V | 19.07μV |
| G4 | 0…0.625V | ±0.625V | 9.53μV |
| G8 | 0…0.3125V | ±0.3125V | 4.76μV |
| … | … | … | … |
| G128 | 0…0.0195V | ±0.0195V | 0.29μV |


### Nota sulla Calibrazione
* La calibrazione interna agisce direttamente sui registri di Offset e Gain prima che il dato venga reso disponibile.

* __Zero-Scale Calibration__: Sposta la funzione di trasferimento in modo che un ingresso di $0V$ produca esattamente $0x0000$ (Unipolare) o $0x8000$ (Bipolare).

* __Full-Scale Calibration__: Regola la pendenza della retta in modo che l'ingresso massimo corrisponda a $0xFFFF$.

## Suggerimento finale: 
Se decidi di usare un guadagno superiore a 1 (es. G8), ricordati che la tua tensione massima in ingresso non deve superare $V_{ref}/Gain$. Nel tuo caso, con $V_{ref} = 2.5V$, se imposti G8 puoi misurare al massimo $312.5 mV$.

## Esempio di utilizzo
```C
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
AD7705_ESP32::Channel currentCh = AD7705_ESP32::CH1;

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

    // Imposta la tensione di riferimento reale misurata sul pin VREF (es. 2.5V)
    adc.setVRef(2500.0);

    // Configurazione CANALE 1: Unipolare, Guadagno 1 (Range 0-2.5V)
    Serial.println("Configurazione Canale 1...");
    adc.setConfig(AD7705_ESP32::CH1, currentMode, AD7705_ESP32::G1);

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
    adc.setConfig(AD7705_ESP32::CH2, AD7705_ESP32::UNIPOLAR, AD7705_ESP32::G8);

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
    adc.selectChannel(currentCh);
}

unsigned long lastSwitchTime = 0;

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

```