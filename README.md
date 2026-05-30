##### **Promotion 2025-2026 Génie Physique INSA TOulouse** — *Marie-Charbel ADJASSE-ABENI & Baptiste CHAUPIN*
**Encadrants :** J. Grisolia, B. Mestre, A. Biganzoli, C. Crouzet

# 1.1 Projet Capteur

Ce projet collaboratif a pour but de mettre en place une chaîne de mesure complète, autonome et connectée permettant de calibrer, caractériser et analyser en temps réel un capteur de déformation low-tech à base de graphite tracé sur papier. 

Le projet se base directement des travaux de *Lin, CW, Zhao, Z., Kim, J. et al. (2014)* publiés dans *Nature*, intitulés **"Pencil Drawn Strain Gauges and Chemiresistors on Paper"**.

---

## 1.2 Contexte & Principe Physique

Le capteur est réalisé par dépôt de graphite sur une feuille de papier (épaisseur $0.35\text{ mm}$) à l'aide de différents crayons (B, 3B, 6B, 2H, HB). Ce dépôt forme un **système granulaire** dont la conductance dépend de la distance inter-grain :
* **En compression** : La distance inter-grain diminue, provoquant une augmentation de la conductance (chute de la résistance).
* **En traction/tension** : La distance augmente, entraînant une hausse de la résistance.

L'objectif de ce projet est de concevoir l'électronique d'acquisition, le système embarqué et l'interface applicative permettant de déduire fidèlement l'angle de flexion et la déformation du papier en fonction de sa résistance.

---

### 2.1 Livrables du Projet
1. Un **Shield PCB personnalisé** pour Arduino UNO (mesure, amplification et communication).
2. Un **Code source Arduino (C++)** structuré en machine à états gérant l'acquisition et l'IHM.
3. Une **Application Android (.APK)** sous MIT App Inventor assurant la supervision et le tracé graphique en temps réel.
4. Une **Fiche technique (Datasheet)** de caractérisation du capteur en graphite.

---

### 2.2 Composants Utilisés
* **Microcontrôleur :** Carte Arduino UNO Rev 3.
* **Amplification (AOP) :** LTC1050.
* **Passifs d'instrumentation :** Résistances ($2 \times 1\text{ k}\Omega$, $1 \times 10\text{ k}\Omega$, $2 \times 100\text{ k}\Omega$) & Condensateurs ($2 \times 100\text{ nF}$, $1 \times 1\text{ }\mu\text{F}$).
* **Rétroaction Numérique :** Potentiomètre digital SPI MCP41010 ($10\text{ k}\Omega$, $256$ pas).
* **Interface (IHM) :** Écran OLED 0.96" SSD1306 (I2C) & Encodeur rotatif Keyes KY-040.
* **Communication :** Module Bluetooth HC-05.
* **Comparaison avec un capteur disponible sur le marché :** Capteur de flexion commercial (Flex Sensor Flexpoint / LLC 1070) + Résistance de charge de $47\text{ k}\Omega$.

---
## 3. Simulation Électronique & Conditionnement (LTSpice)

Le capteur de graphite présente une résistance à vide très élevée (de l'ordre du $\text{M}\Omega$ au $\text{G}\Omega$ selon le crayon utilisé). Sous une alimentation de $5\text{V}$, le courant généré est de l'ordre du nanoampère ($\text{nA}$). Un étage d'amplification est indispensable pour adapter le signal à la dynamique du convertisseur analogique-numérique (ADC) de l'Arduino ($0 - 5\text{V}$).

### Circuit Amplificateur Transimpédance (LTC1050)
Pour exploiter le très faible courant traversant le capteur à haute résistance, un montage amplificateur transimpédance a été modélisé sur LTSpice. Le rapport signal/bruit est optimisé par **trois filtres passe-bas** :
1. **Filtre d'entrée (175 Hz)** : Élimine les vibrations mécaniques et tremblements parasites ($C_1 = 100\text{ nF}$, $R_1 = 100\text{ k}\Omega$, // $R_4 = 10\text{ k}\Omega$).
2. **Filtre actif de rétroaction (1.6 Hz)** : Atténue le bruit secteur de $50\text{ Hz}$ du réseau électrique ($C_3 = 1\text{ µF}$, $R_3 = 100\text{ k}\Omega$).
3. **Filtre de sortie (1.6 kHz)** : Élimine les parasites haute fréquence générés par l'électronique ($C_2 = 100\text{nF}$, $R_5 = 1\text{ k}\Omega$).
<img width="1903" height="838" alt="image" src="https://github.com/user-attachments/assets/f9453fb7-c03e-4f91-a1b2-26242c6600e3" />

La fonction de transfert globale liant la tension mesurée par l'ADC ($V_{ADC}$) à la résistance du capteur ($R_c$) est régie par l'équation :

$$V_{ADC} = \left(1 + \frac{R_3}{R_2}\right) \cdot \frac{R_1}{R_1 + R_c + R_5} \cdot V_{cc}$$

D'où l'extraction mathématique de la résistance du capteur opérée par le firmware :

$$R_c = \left(1 + \frac{R_3}{R_2}\right) \cdot R_1 \cdot \frac{V_{cc}}{V_{ADC}} - R_1 - R_5$$

*Note : La résistance $R_2$ est ajustée dynamiquement en modifiant l'indice du potentiomètre numérique MCP41010 ($R_2 = \text{indice} \times 37\,\Omega + 58\,\Omega$).*

---
## Architecture Matérielle (Shield PCB)

Le circuit imprimé, conçu sous KiCad et fabriqué par gravure chimique (insolation UV + perchlorure de fer), se branche comme un Shield sur un Arduino Uno.





### Configuration des Broches (Pinout PCB)

| Composant | Rôle / Description | Interface / Broches Arduino |
| :--- | :--- | :--- |
| **AOP LTC1050** | Amplificateur de signal transimpédance | Entrée Analogique `A0` |
| **MCP41010** | Potentiomètre numérique (10 kΩ) | Bus SPI (`CS: D8` ou `D10`, `MOSI: D11`, `SCK: D13`) |
| **OLED 0.96"** | Écran d'affichage I2C (SSD1306) | Bus I2C (`SDA: A4`, `SCL: A5`) |
| **Encodeur** | Navigation dans le menu déroulat de l'écran OLED | Interruptions (`CLK: D2`, `DT: D4`, `SW: D5` ou `D6`) |
| **HC-05 / HC-06** | Module de communication Bluetooth | SoftwareSerial (`RX: D5` ou `D8`, `TX: D3` ou `D7`) |
| **Flex Sensor** | Capteur commercial de comparaison | Pont diviseur de tension ($47\text{ k}\Omega$) |

---
## Fonctionnalités Principales du Système

* **Auto-Calibration Dynamique (Asservissement)** : Compensation automatique de la résistance à plat ($R_0$) du capteur (de l'ordre du $\text{M}\Omega$) via un potentiomètre digital (MCP41010) afin de centrer la tension de repos à $V_{cc}/2$ ($2.5\text{V} \pm 2\%$).
* **Machine à États Finis (FSM)** : Architecture logicielle Arduino robuste structurée en 4 états distincts : `CALIBRATION`, `CHOIX_MENU`, `MESURE` et `AFFICHAGE`.
* **Interface Homme-Machine (IHM) Autonome** : Navigation par encodeur rotatif (gestion par interruptions) à travers des menus dynamiques affichés sur un écran OLED SSD1306 (I2C).
* **Supervision Mobile en Temps Réel** : Application Android dédiée (MIT App Inventor) affichant les graphiques de déformation, les indicateurs de stabilité et un module d'exportation de données scientifiques au format `.txt`.

---
## Structure du Dépôt GitHub

```text
├── Arduino/
│   └── Graphene_Analyzer_Pro.ino  # Code complet de la machine à états et des menus
├── Mobile_App/
│   ├── Graphene_Supervision.aia   # Fichier source MIT App Inventor (Modifiable)
│   └── Graphene_Supervision.apk   # Application compilée prête à installer sur Android
├── Hardware_PCB/
│   ├── KiCad_Project/             # Schématique (.sch) et Routage (.kicad_pcb)
│   └── Gerber/                    # Fichiers de fabrication pour gravure / commande
└── Datasheet_Et_Mesures/
    ├── Data_Caracterisation.xlsx  # Relevés sur banc de test (crayons B, 3B, 6B, 2H, HB)
    └── Datasheet_Graphite_Sensor.pdf # Fiche technique finale du capteur
