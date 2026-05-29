/*
 * ============================================================
 *  CAPTEUR GRAPHITE - Machine à états
 * ============================================================
 *  Broches utilisées :
 *    A0        → Capteur graphite
 *    A4 / A5   → SDA / SCL (OLED I2C)
 *    2         → Encodeur CLK
 *    4         → Encodeur DT
 *    3         → Encodeur Switch
 *    7         → BT RX (→ TX du HC-05)
 *    8         → BT TX (→ RX du HC-05)
 *    10        → CS du MCP41010
 *    11/12/13  → MOSI/MISO/SCK SPI
 * ============================================================
 */

/*----------------------- LIBRAIRIES -----------------------*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <String.h>

/*----------------------- OLED -----------------------*/
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR    0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/*----------------------- BLUETOOTH -----------------------*/
#define RX_PIN 8   // Broche 7 Arduino → TX du HC-05
#define TX_PIN 7   // Broche 8 Arduino → RX du HC-05
SoftwareSerial Bluetooth(RX_PIN, TX_PIN);

/*----------------------- ENCODEUR -----------------------*/
#define ENC_CLK  2   // Interrupt 0
#define ENC_DT   4
#define ENC_SW   5

/*----------------------- MCP41010 -----------------------*/
#define CS_PIN       10
#define MCP_WRITE  0x11   // Commande écriture pot0
const int   MCP_MAX_POS  = 255;
const float MCP_RAB      = 10000.0;  // Résistance maximale de 10 kΩ

/*----------------------- CONSTANTES DU CIRCUIT -----------------------*/
//  Schéma : amplificateur transimpédance
//    R1 = 100kΩ, R3 = 10kΩ, R4 = 100kΩ, R2 = potentiomètre digital
const float R1  = 100000.0;
const float R3  =  10000.0;
const float R4  = 100000.0;
const float VCC =      5.0;

/*----------------------- VARIABLES GENERALES -----------------------*/
// Machine à états
//  1 = Calibration
//  2 = Menu
//  3 = Mesure (envoi BT + affichage)
int  etat            = 0;   // 0 = attente commande BT initiale
bool calibrage_fini  = false;

// Potentiomètre
int   pos_mcp        = 128; // Position initiale (milieu)
float R2             = 0.0; // Résistance calculée du MCP en Ohm

// Mesures
float V_capteur      = 0.0;
float R_capteur      = 0.0; // en MΩ

// Encodeur
volatile int  encoderValue    = 0;
int           lastEncoderValue = -1;
unsigned long lastButtonTime  = 0;
const int     DEBOUNCE_MS     = 200;

// Menu
const char* menuItems[]  = {"Voir Tension", "Voir Resistance"};
const int   TOTAL_ITEMS  = 2;
int   currentSelection   = 0;
bool  menu_confirme      = false;

// Envoi BT
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 200; // ms

/*=====================================================================
 *  FONCTIONS UTILITAIRES
 *=====================================================================*/

/* --- MCP41010 : écriture SPI --- */
void setPot(int pos) {
  pos = constrain(pos, 0, 255);
  pos_mcp = pos;
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(MCP_WRITE);
  SPI.transfer((byte)pos);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

/* --- Calcul résistance R2 du MCP selon position --- */
float calculerR2(int pos) {
  return (MCP_RAB/MCP_MAX_POS)*pos;
}

/* --- Calcul résistance capteur graphite en MΩ --- */
float calculerResistance(float Va0, float r2) {
  if (Va0 <= 0.05) {return 0.0;} // sécurité division par zéro
  float Rcapt = (1.0 + (R4 / r2)) * (R1 * VCC / Va0) - R1 - R3;
  return Rcapt / 1000000.0; // conversion en MΩ
}

/* --- ISR encodeur --- */
void updateEncoder() {
  if (digitalRead(ENC_DT) != digitalRead(ENC_CLK)) {
    encoderValue++;
  } else {
    encoderValue--;
  }
}

/* --- Dessin menu OLED --- */
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("--- MENU ---"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  for (int i = 0; i < TOTAL_ITEMS; i++) {
    display.setCursor(0, 18 + i * 12);
    if (i == currentSelection) {
      display.print(F("> "));
    } else {
      display.print(F("  "));
    }
    display.println(menuItems[i]);
  }
  display.setCursor(0, 54);
  display.println(F("Appuyer pour OK"));
  display.display();
}

/*=====================================================================
 *  SETUP
 *=====================================================================*/
void setup() {

  delay(300);

  /* 1. Serial moniteur */
  Serial.begin(9600);
  Serial.println(F("=== Demarrage ==="));

  /* 2. Encodeur */
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), updateEncoder, CHANGE);

  /* 3. OLED — I2C en premier, avant SPI */
  Wire.begin();
  delay(100);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED FAIL - boucle infinie"));
    while (1);
  }
  Serial.println(F("OLED OK"));
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawMenu();

  /* 4. SPI + MCP41010 */
  digitalWrite(CS_PIN, HIGH);e
  pinMode(CS_PIN, OUTPUT);
  SPI.begin();
  setPot(pos_mcp); // Position initiale aves le fain de 128
  Serial.println(F("MCP OK"));

  /* 5. Bluetooth en dernier */
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);
  Bluetooth.begin(9600);
  Serial.println(F("Bluetooth OK"));

  /* 6. Capteur */
  pinMode(A0, INPUT);

  Serial.println(F("=== Setup termine ==="));
}

/*=====================================================================
 *  LOOP
 *=====================================================================*/
void loop() {

  /* --- Lecture capteur (toujours active) --- */
  float lecture = analogRead(A0);
  V_capteur = lecture * (VCC / 1023.0);
  R2        = calculerR2(pos_mcp);
  R_capteur = calculerResistance(V_capteur, R2);

  /* --- Réception commande Bluetooth --- */
  if (Bluetooth.available() > 0) {
    int commande = Bluetooth.read();
    Serial.print(F("BT recu: "));
    Serial.println(commande);

    // L'appli envoie 1 pour démarrer la calibration
    if (commande == 1 && etat == 0) {
      etat = 1;
      calibrage_fini = false;
      Serial.println(F("-> Etat 1 : Calibration"));
    }
  }

  /*=====================================================================
   *  ÉTAT 1 — CALIBRATION
   *  But : asservir pos_mcp pour que V_capteur ≈ 2.5V
   *=====================================================================*/
  if (etat == 1) {

    // Si déjà calibré, on saute directement au menu
    if (calibrage_fini) {
      etat = 2;
    }
    else if (V_capteur > 2.6 && pos_mcp < 256) {
      pos_mcp++;
      setPot(pos_mcp);
      Serial.print("====== Valeur de pos_mpc :");
      Serial.println(pos_mcp);
      Serial.println(V_capteur);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,  0); display.print("CALIBRATION");
      display.setCursor(0, 25); display.print(V_capteur); display.print("V -->DOWN");
      display.setCursor(0, 50); display.print("Valeur analogique MCP : "); display.print(pos_mcp);
      display.display();

      delay(80);
    }
    else if (V_capteur < 2.4 && pos_mcp > 1) {
      pos_mcp--;
      setPot(pos_mcp);
      Serial.println(V_capteur);
      Serial.print("====== Valeur de pos_mpc :");
      Serial.println(pos_mcp);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,  0); display.print("CALIBRATION");
      display.setCursor(0, 25); display.print(V_capteur);  display.print("V -->UP");
      display.setCursor(0, 50); display.print("Valeur analogique MCP : "); display.print(pos_mcp);
      display.display();

      delay(80);
    }
    else {
      // Tension stabilisée entre 2.4V et 2.6V
      calibrage_fini = true;
      float r2kOhm = calculerR2(pos_mcp) / 1000.0;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,  0); display.print("CALIBRATION OK");
      display.setCursor(0, 25); display.print("R2="); + display.print(r2kOhm); display.print(" kOhm");
      display.setCursor(0, 50); display.print("V="); + display.print(V_capteur); display.print(" V");
      display.display();
      Bluetooth.println("OK");
      delay(10000);

      Serial.println(F("Calibration OK"));
      etat = 2;
    }
  }

  /*=====================================================================
   *  ÉTAT 2 — MENU (encodeur rotatif)
   *=====================================================================*/
  else if (etat == 2) {

    menu_confirme = false; // Réinitialise à chaque passage

    // Mise à jour sélection via encodeur
    if (encoderValue != lastEncoderValue) {
      // Navigation circulaire
      if (encoderValue >= TOTAL_ITEMS) encoderValue = 0;
      if (encoderValue < 0)            encoderValue = TOTAL_ITEMS - 1;
      currentSelection = encoderValue;
      lastEncoderValue = encoderValue;
      drawMenu();
    }
    else {
      // Affichage initial du menu
      drawMenu();
    }

    // Appui bouton encodeur = confirmation
    if (digitalRead(ENC_SW) == LOW) {
      if (millis() - lastButtonTime > DEBOUNCE_MS) {
        lastButtonTime = millis();
        menu_confirme  = true;
        Serial.print(F("Menu confirmé : "));
        Serial.println(currentSelection);
        delay(50);
        etat = 3;
      }
    }
  }

  /*=====================================================================
   *  ÉTAT 3 — ENVOI BLUETOOTH
   *=====================================================================*/
  else if (etat == 3) {

    // Envoi toutes les 200ms
    if (millis() - lastSend > SEND_INTERVAL) {
      Serial.println(R_capteur);
      Serial.print(F("MOhms "));
      Bluetooth.println(R_capteur);
      delay(100);
      lastSend = millis();
      etat = 4; // Passe immédiatement à l'affichage
    }

    
  }

  /*=====================================================================
   *  ÉTAT 4 — AFFICHAGE OLED
   *=====================================================================*/
  else if (etat == 4) {

    if (currentSelection == 0) {
      // Affichage tension

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,  0); display.print("TENSION (A0)");
      display.setCursor(0, 25); display.print(V_capteur, 3); display.print(" V");
      display.setCursor(0, 50); display.print("Clic -> Menu");
      display.display();

    } else {
      // Affichage résistance

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,  0); display.print("RESISTANCE");
      display.setCursor(0, 25); display.print(R_capteur, 3); display.print("MOhms");   // Affiche la tension avec 3 décimales [cite: 28, 62]
      display.setCursor(0, 50); display.print("Clic -> Menu");
      display.display();
    }

    // Appui bouton = retour menu
    if (digitalRead(ENC_SW) == LOW) {
      if (millis() - lastButtonTime > DEBOUNCE_MS) {
        lastButtonTime = millis();
        Serial.println(F("Retour menu"));
        delay(50);
        etat = 2;
      }
    }
    else {
    etat = 3; // Boucle entre 3 et 4 en permanence
    }
  }

  delay(30); // Légère pause pour stabilité
}
