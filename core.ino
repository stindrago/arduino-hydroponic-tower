#include <LiquidCrystal.h>
#include <EEPROM.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Pin config
const int motorPin = 9;
const int ledPin = 8;
const int buttonPin = 7;
const int potPin = A0;

// Pompa
const float portataMax = 1.67;
const int altezzaMax = 110;
const int pwmMinFunzionante = 130;

// Ciclo basato su tempo totale attivo
const int cicloOgni = 1200;   // Ogni 1200s = 20 min
const int durataOn = 30;      // Acceso per 30s

// EEPROM
const int EEPROM_ADDR_1 = 0;
const int EEPROM_ADDR_2 = EEPROM_ADDR_1 + sizeof(unsigned long);

// Variabili
unsigned long tempoUltimoSalvataggio = 0;
unsigned long tempoUltimoSecondo = 0;
unsigned long tempoAttivoTotale = 0;
unsigned long cicliCompletati = 0;

bool statoMotore = false;
bool resetEseguito = false;
bool buttonStatoPrecedente = HIGH;
unsigned long tempoInizioPressione = 0;
int paginaCorrente = 0;

// Anti-flicker
String ultimaRiga0 = "";
String ultimaRiga1 = "";

void setup() {
  pinMode(motorPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lcd.begin(16, 2);
  mostraMessaggio("Sistema Avviato", "Carico dati...");
  delay(1500);

  EEPROM.get(EEPROM_ADDR_1, cicliCompletati);
  EEPROM.get(EEPROM_ADDR_2, tempoAttivoTotale);

  mostraMessaggio("STEM 2025 code:", "github:stindrago");
  delay(5000);
}

void loop() {
  unsigned long ora = millis();

  // Lettura potenziometro
  int lettura = analogRead(potPin);
  int pwm = map(lettura, 0, 1023, 0, 255);

  // Calcolo portata e altezza (per LCD)
  float portata = 0.0;
  int altezza = 0;
  if (pwm >= pwmMinFunzionante) {
    float perc = (pwm - pwmMinFunzionante) / float(255 - pwmMinFunzionante);
    portata = perc * portataMax;
    altezza = perc * altezzaMax;
  }

  // Contatore tempo attivo (simula tempo reale)
  if (ora - tempoUltimoSecondo >= 1000) {
    tempoUltimoSecondo = ora;
    tempoAttivoTotale++;
  }

  // Irrigazione basata su tempo attivo totale
  int secondiNelCiclo = tempoAttivoTotale % cicloOgni;

  if (secondiNelCiclo < durataOn) {
    if (!statoMotore) {
      statoMotore = true;
      digitalWrite(ledPin, HIGH);
      cicliCompletati++;
    }
    analogWrite(motorPin, pwm);
  } else {
    if (statoMotore) {
      statoMotore = false;
      digitalWrite(ledPin, LOW);
    }
    analogWrite(motorPin, 0);
  }

  // SALVATAGGIO EEPROM ogni 10s
  if (ora - tempoUltimoSalvataggio >= 10000) {
    EEPROM.put(EEPROM_ADDR_1, cicliCompletati);
    EEPROM.put(EEPROM_ADDR_2, tempoAttivoTotale);
    tempoUltimoSalvataggio = ora;
  }

  // GESTIONE PULSANTE
  bool buttonStato = digitalRead(buttonPin);

  if (buttonStato == LOW) {
    if (buttonStatoPrecedente == HIGH) {
      tempoInizioPressione = ora;
      resetEseguito = false;
    } else {
      if (!resetEseguito && (ora - tempoInizioPressione >= 15000)) {
        cicliCompletati = 0;
        tempoAttivoTotale = 0;
        EEPROM.put(EEPROM_ADDR_1, cicliCompletati);
        EEPROM.put(EEPROM_ADDR_2, tempoAttivoTotale);

        paginaCorrente = 0;
        ultimaRiga0 = "";
        ultimaRiga1 = "";

        mostraMessaggio("RESET & START", "Giorno = 08:00");
        delay(3000);
        lcd.clear();
        delay(200);

        resetEseguito = true;
      }
    }
  }

  if (buttonStato == HIGH && buttonStatoPrecedente == LOW && !resetEseguito) {
    paginaCorrente = (paginaCorrente + 1) % 4;
  }

  buttonStatoPrecedente = buttonStato;

  aggiornaLCD(pwm, portata, altezza);
  delay(100);
}

void aggiornaLCD(int pwm, float portata, int altezza) {
  String riga0, riga1;

  switch (paginaCorrente) {
    case 0:
      riga0 = "Motore:";
      riga1 = statoMotore ? "IN FUNZIONE" : "IN PAUSA";
      break;

    case 1:
      riga0 = "Tempo attivo:";
      riga1 = formattaTempo(tempoAttivoTotale);
      break;

    case 2:
      riga0 = "Cicli completati";
      riga1 = String(cicliCompletati);
      break;

    case 3:
      riga0 = "PWM L/m    cm";
      riga1 = formattaPWMinfo(pwm, portata, altezza);
      break;
  }

  scriviRiga(0, riga0, ultimaRiga0);
  scriviRiga(1, riga1, ultimaRiga1);
}

String formattaPWMinfo(int pwm, float portata, int altezza) {
  int pInt = int(portata);
  int pDec = int((portata - pInt) * 10);
  char buf[17];
  snprintf(buf, sizeof(buf), "%3d %d.%d    %3d", pwm, pInt, pDec, altezza);
  return String(buf);
}

void scriviRiga(int riga, String testo, String &ultimaRiga) {
  if (testo != ultimaRiga) {
    lcd.setCursor(0, riga);
    String completo = testo;
    while (completo.length() < 16) completo += " ";
    lcd.print(completo);
    ultimaRiga = testo;
  }
}

void mostraMessaggio(String riga0, String riga1) {
  lcd.setCursor(0, 0);
  lcd.print(riga0 + String("                ").substring(riga0.length()));
  lcd.setCursor(0, 1);
  lcd.print(riga1 + String("                ").substring(riga1.length()));
}

String formattaTempo(unsigned long secondi) {
  unsigned int giorni = secondi / 86400;
  unsigned int ore = (secondi % 86400) / 3600;
  unsigned int minuti = (secondi % 3600) / 60;
  unsigned int sec = secondi % 60;

  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%03ud %02u:%02u:%02u", giorni, ore, minuti, sec);
  return String(buffer);
}
