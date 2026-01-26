# AD7705_ESP32 Library
Libreria ottimizzata per l'uso dell'ADC Sigma-Delta a 16 bit AD7705 con microcontrollori ESP32. 
La libreria gestisce la comunicazione SPI a bassa velocità (200kHz) per garantire la massima stabilità del segnale, supporta due canali differenziali indipendenti e include procedure di calibrazione automatica.

## Caratteristiche
Risoluzione: 16 bit.
Canali: 2 canali differenziali (AIN1, AIN2).
Calibrazione: Self-calibration interna per offset e guadagno.
Filtro Digitale: Configurato per 50Hz (reiezione disturbi di rete).
Compatibilità: Progettata specificamente per i livelli logici 3.3V dell'ESP32.

## Collegamenti Hardware (Pinout)
Sulla maggior parte dei moduli (es. QYF-973), assicurarsi dei seguenti collegamenti:

AD7705 PinESP32 Pin (Default)Descrizione
VCC3.3V / 5VAlimentazione
GNDGNDMassa comune
SCLKGPIO 18
Clock SPI
MISOGPIO 19Master In Slave Out
MOSIGPIO 23Master Out Slave In
CSGPIO 5 (o scelto dall'utente)Chip Select
DRDYGPIO 4 (o scelto dall'utente)Data Ready

[!IMPORTANT] Riferimento di massa: Se si effettuano misurazioni riferite a massa (Single-Ended), i pin AIN1- e/o AIN2- devono essere collegati fisicamente a GND.

# Metodi della Classe
## Inizializzazione e Configurazione
begin(SPIClass &spi): Inizializza l'interfaccia SPI, sincronizza il chip e imposta il clock interno a 4.9152MHz per un output rate di 50Hz.

resetSPI(): Invia 32 bit a '1' per resettare l'interfaccia seriale dell'AD7705 in caso di desincronizzazione.

setConfig(Channel ch, Mode mode, Gain gain): Configura il canale specifico.

Mode: UNIPOLAR (1) o BIPOLAR (0).

Gain: Da G1 a G128.

## Operazioni di Misura
calibrate(Channel ch): Esegue la calibrazione interna. Il pin DRDY rimarrà alto fino al completamento.
available(): Restituisce true se il dato è pronto per essere letto (DRDY LOW).
getRawData(Channel ch): Legge il valore grezzo a 16 bit (0-65535 in unipolare).
getVoltage(Channel ch, float vref): Converte il valore raw in tensione reale in base alla $V_{ref}$ fornita (default 2.50V).

## Diagnostica
readCalibrationRegisters(Channel ch): Legge e stampa su Serial i registri interni di Offset e Gain a 24 bit. Utile per verificare se la calibrazione ha avuto successo.

## Note Tecniche e Troubleshooting
Valori Raw Bloccati: Se il valore raw non cambia, verificare il pin DRDY. Se non pulsa mai a 50Hz, il chip non è configurato correttamente o il clock non è attivo.

Precisione: L'AD7705 è molto sensibile al rumore. Utilizzare condensatori di bypass da 0.1µF tra VCC e GND il più vicino possibile al chip.

SPI Speed: La libreria è impostata a 1MHz. Non aumentare oltre 1MHz se si riscontrano errori nella lettura dei registri a 24 bit.

## Formule di Conversione
L'AD7705 converte la tensione analogica in un valore digitale a 16 bit ($2^{16} = 65536$ livelli). La formula varia a seconda della modalità impostata (UNIPOLAR o BIPOLAR).

1. Modalità Unipolare (UNIPOLAR)
In questa modalità, l'ADC accetta tensioni da $0$ a $V_{ref}/Gain$. Il valore RAW $0x0000$ corrisponde a $0V$, mentre $0xFFFF$ corrisponde al fondo scala.$$V_{in} = \frac{ValoreRaw \cdot V_{ref}}{Gain \cdot 65535}$$

Esempio: Con $V_{ref} = 2.5V$, $Gain = 1$ e $ValoreRaw = 0x4590$ (17808):
$$V_{in} = \frac{17808 \cdot 2.5}{1 \cdot 65535} \approx 0.6793V$$

2. Modalità Bipolare (BIPOLAR)
In questa modalità, l'ADC utilizza un codice a offset binario. Il valore RAW $0x8000$ ($32768$) corrisponde allo zero ideale ($0V$). I valori inferiori sono tensioni negative, quelli superiori sono positivi.
$$V_{in} = \frac{(ValoreRaw - 32768) \cdot V_{ref}}{Gain \cdot 32768}$$
Range: La tensione misurabile spazia da $-V_{ref}/Gain$ a $+V_{ref}/Gain$.


## Tabella dei Range di Misura ($V_{ref} = 2.5V$)
Guadagno (Gain),Range Unipolare,Range Bipolare,Risoluzione (LSB Unip.)
G1,0…2.5V,±2.5V,38.15μV
G2,0…1.25V,±1.25V,19.07μV
G8,0…0.3125V,±0.3125V,4.76μV
G128,0…0.0195V,±0.0195V,0.29μV


## Nota sulla Calibrazione
La calibrazione interna agisce direttamente sui registri di Offset e Gain prima che il dato venga reso disponibile.

Zero-Scale Calibration: Sposta la funzione di trasferimento in modo che un ingresso di $0V$ produca esattamente $0x0000$ (Unipolare) o $0x8000$ (Bipolare).

Full-Scale Calibration: Regola la pendenza della retta in modo che l'ingresso massimo corrisponda a $0xFFFF$.

## Suggerimento finale: 
Se decidi di usare un guadagno superiore a 1 (es. G8), ricordati che la tua tensione massima in ingresso non deve superare $V_{ref}/Gain$. Nel tuo caso, con $V_{ref} = 2.5V$, se imposti G8 puoi misurare al massimo $312.5 mV$.
