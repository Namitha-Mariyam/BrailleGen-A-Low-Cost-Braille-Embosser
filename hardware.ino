#include <Servo.h>

// ---------------- PIN DEFINITIONS ----------------
#define X_STEP 2
#define X_DIR 5
#define Y_STEP 4
#define Y_DIR 7
const int ENABLE = 8;
#define PUNCH_SERVO_PIN 44
#define CELL_SERVO_PIN 46
int END_LIMIT = 52;

Servo punchServo;
Servo cellServo;

// ---------------- SETTINGS ----------------
int stepDelay = 2000;
int charSpacingSteps = 600;
int lineSpacingSteps = 200;
const int MAX_X_STEPS = 15000;
long currentXPos = 0;

int punchDown = 120;
int punchUp = 50;
int rowPos[3] = { 0, 20, 40 }; // Top, Middle, Bottom
int dotOffset = 300;           // Space between left and right column
bool isMathMode = false;

// ---------------- BRAILLE MAPS ----------------

struct BrailleEntry {
  const char* key;
  const char* pattern; 
};

// --- GRADE 2 CONTRACTIONS (Standard UEB) ---
BrailleEntry grade2Map[] = {
  {"the", "011101"}, {"and", "111101"}, {"for", "111111"}, {"with", "011111"},
  {"ing", "001111"}, {"ch", "100001"}, {"sh", "100101"}, {"th", "100111"},
  {"wh", "100011"}, {"ed", "110101"}, {"er", "110111"}, {"ou", "110011"},
  {"st", "011100"}, {"ar", "001110"}
};

// --- ADVANCED NEMETH & CHEMISTRY ---
BrailleEntry scienceMap[] = {
  {"alpha", "100000"}, {"beta", "110000"}, {"gamma", "110110"}, // Greek
  {"delta", "100110"}, {"phi", "110100"}, {"pi", "111100"},
  {"root", "110101"}, // Radical Start (Square root)
  {"stop", "000111"}, // Termination Indicator (End of root/exponent)
  {"^", "000110"},    // Superscript (Exponent)
  {"_", "000011"},    // Subscript (Chemistry: H2O)
  {"+", "011010"}, {"-", "001001"}, {"*", "001010"}, {"/", "011001"}, {"=", "011011"},
  {"(", "111011"}, {")", "011111"}, {"[", "110101"}, {"]", "000111"}, {"\\frac", "011100"},
{"}{", "010001"}, {"}", "111100"}, {"omega", "010111"}, {"->", "100100010011"}
};

BrailleEntry greekNemeth[] = {
  // --- Lowercase Greek (\alpha, \beta...) ---
  {"\\alpha", "100000"}, {"\\beta", "110000"}, {"\\gamma", "110110"}, 
  {"\\delta", "100110"}, {"\\epsilon", "100010"}, {"\\zeta", "101011"},
  {"\\eta", "011101"}, {"\\theta", "011011"}, {"\\iota", "010100"},
  {"\\kappa", "101000"}, {"\\lambda", "111000"}, {"\\mu", "101100"},
  {"\\nu", "101110"}, {"\\xi", "101101"}, {"\\omicron", "101010"},
  {"\\pi", "111100"}, {"\\rho", "111010"}, {"\\sigma", "011100"},
  {"\\tau", "011110"}, {"\\phi", "110100"}, {"\\chi", "110101"},
  {"\\psi", "111101"}, {"\\omega", "010111"},

  // --- Uppercase Greek (\Delta, \Omega...) ---
  // Note: These include the Dot 6 Capital Indicator 000001
  {"\\Delta", "000001100110"}, 
  {"\\Omega", "000001010111"},
  {"\\Phi", "000001110100"},
  {"\\Pi", "000001111100"},
  {"\\Sigma", "000001011100"},
  {"\\Theta", "000001011011"}
};

// --- NUMBERS & ALPHABET ---
BrailleEntry nemethDigits[] = {{"1", "010000"}, {"2", "011000"}, {"3", "010010"}, {"4", "010011"}, {"5", "010001"}, {"6", "011010"}, {"7", "011011"}, {"8", "011001"}, {"9", "010010"}, {"0", "010011"}};
BrailleEntry alphabet[] = {{"a", "100000"}, {"b", "110000"}, {"c", "100100"}, {"d", "100110"}, {"e", "100010"}, {"f", "110100"}, {"g", "110110"}, {"h", "110010"}, {"i", "010100"}, {"j", "010110"}, {"k", "101000"}, {"l", "111000"}, {"m", "101100"}, {"n", "101110"}, {"o", "101010"}, {"p", "111100"}, {"q", "111110"}, {"r", "111010"}, {"s", "011100"}, {"t", "011110"}, {"u", "101001"}, {"v", "111001"}, {"w", "010111"}, {"x", "101101"}, {"y", "101111"}, {"z", "101011"}};

const char* CAP_IND = "000001";   
const char* NUM_IND = "001111";   
const char* GREEK_IND = "000101";

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  pinMode(X_STEP, OUTPUT);
  pinMode(X_DIR, OUTPUT);
  pinMode(Y_STEP, OUTPUT);
  pinMode(Y_DIR, OUTPUT);
  pinMode(END_LIMIT, INPUT_PULLUP);
  pinMode(ENABLE, OUTPUT);

  punchServo.attach(PUNCH_SERVO_PIN);
  cellServo.attach(CELL_SERVO_PIN);
  cellServo.write(rowPos[0]);
  punchServo.write(punchUp);

  // Initial Homing to Margin
  digitalWrite(ENABLE, LOW);
  while (digitalRead(END_LIMIT) == HIGH) {
    movetodefaultposition(10); // Small steps until home
  }
  digitalWrite(ENABLE, HIGH);
  currentXPos = 0;
  Serial.println("Started...");
}

// ---------------- LOOP ----------------
void loop() {
  if (Serial.available()) {
    String text = Serial.readStringUntil('\n');
    int i = 0;
    isMathMode = false;

    // --- STEP 1: PROCESS ALL CHARACTERS IN THE LINE ---
    while (i < text.length()) {
      String sub = text.substring(i);
      char current = text[i];

      // 1. Grade 2 Contractions
      bool found = false;
      for (auto &e : grade2Map) {
        if (sub.startsWith(e.key)) {
          printStringPattern(e.pattern);
          i += strlen(e.key);
          moveX(-charSpacingSteps);
          found = true; break;
        }
      }
      if (found) continue;

      // 2. Science/Math/Greek
      for (auto &e : scienceMap) {
        if (sub.startsWith(e.key)) {
          if (strcmp(e.key, "alpha") == 0 || strcmp(e.key, "pi") == 0) {
             printStringPattern(GREEK_IND);
             moveX(-charSpacingSteps);
          }
          printStringPattern(e.pattern);
          i += strlen(e.key);
          moveX(-charSpacingSteps);
          found = true; break;
        }
      }
      if (found) continue;

      // 3. Numbers
      if (isdigit(current)) {
        if (!isMathMode) { 
          printStringPattern(NUM_IND); 
          moveX(-charSpacingSteps); 
          isMathMode = true; 
        }
        for (auto &e : nemethDigits) {
          if (e.key[0] == current) { printStringPattern(e.pattern); break; }
        }
        i++; moveX(-charSpacingSteps); continue;
      }

      // 2b. Advanced Greek Check
      if (current == '\\') {
          bool greekFound = false;
          for (auto &e : greekNemeth) {
          if (sub.startsWith(e.key)) {
            // 1. Punch the Indicator (it returns to the start of the current cell)
            printStringPattern(GREEK_IND);
            // 2. Move to the NEXT cell position
            moveX(-charSpacingSteps);
            // 3. Punch the actual letter
            printStringPattern(e.pattern);
            
            i += strlen(e.key);
            // i++ is at the bottom of the loop, but we move to the next char here
            moveX(-charSpacingSteps); 
            greekFound = true; 
            break;
        }
    }
    if (greekFound) continue;
}
      // 4. Alphabet, Capitals, Spaces
      if (current == ' ') {
        moveX(-charSpacingSteps);
        isMathMode = false; 
      } else {
        if (isupper(current)) { 
          printStringPattern(CAP_IND); 
          moveX(-charSpacingSteps); 
        }
        for (auto &e : alphabet) {
          if (e.key[0] == tolower(current)) { printStringPattern(e.pattern); break; }
        }
      }
      i++; 
      moveX(-charSpacingSteps);
    }

    // --- STEP 2: LINE FINISHED MECHANICALLY ---
    delay(500);
    newLine(); // Moves Y and Homes X

    // --- STEP 3: THE HANDSHAKE ---
    // This is only sent ONCE per line, after all movements are done
    Serial.println("Ready"); 
  }
}
void printStringPattern(const char* pattern) {
  int totalBits = strlen(pattern);
  
  // Process in chunks of 6 (one Braille cell at a time)
  for (int cellOffset = 0; cellOffset < totalBits; cellOffset += 6) {
    
    // LEFT COLUMN (dots 1,2,3)
    for (int row = 0; row < 3; row++) {
      if (pattern[cellOffset + row] == '1') {
        cellServo.write(rowPos[row]);
        delay(1000); 
        punch();
      }
    }
    
    moveX(-dotOffset); // Move to right column of SAME cell
    
    // RIGHT COLUMN (dots 4,5,6)
    for (int row = 0; row < 3; row++) {
      if (pattern[cellOffset + row + 3] == '1') {
        cellServo.write(rowPos[row]);
        delay(1000);
        punch();
      }
    }

    // After finishing a 6-dot cell:
    if (totalBits - cellOffset > 6) {
      // If there's ANOTHER cell in this same pattern (like a Capital Greek)
      // Move to the next character position
      moveX(-charSpacingSteps + dotOffset); 
    } else {
      // If it's the last cell, just move back to the left side 
      moveX(dotOffset); 
    }
  }
}

void punch() {
  punchServo.write(punchDown);
  delay(1000);
  punchServo.write(punchUp);
  delay(1000);
}

void moveX(int steps) {
  currentXPos += abs(steps);
  if (currentXPos >= MAX_X_STEPS) {
    newLine();
    return; 
  }
  digitalWrite(ENABLE, LOW);
  digitalWrite(X_DIR, steps > 0 ? HIGH : LOW);
  for (int i = 0; i < abs(steps); i++) {
    digitalWrite(X_STEP, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(X_STEP, LOW);
    delayMicroseconds(stepDelay);
  }
  digitalWrite(ENABLE, HIGH);
}

void moveY(int steps) {
  digitalWrite(ENABLE, LOW);
  digitalWrite(Y_DIR, steps > 0 ? LOW : HIGH);
  for (int i = 0; i < abs(steps); i++) {
    digitalWrite(Y_STEP, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(Y_STEP, LOW);
    delayMicroseconds(stepDelay);
  }
  digitalWrite(ENABLE, HIGH);
}

void newLine() {
  moveY(lineSpacingSteps);
  digitalWrite(ENABLE, LOW);
  digitalWrite(X_DIR, HIGH); 
  while (digitalRead(END_LIMIT) == HIGH) {
    digitalWrite(X_STEP, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(X_STEP, LOW);
    delayMicroseconds(stepDelay);
  }
  currentXPos = 0;
  digitalWrite(ENABLE, HIGH);
  Serial.println("Ready"); 
}

void movetodefaultposition(int steps) {
  digitalWrite(ENABLE, LOW);
  digitalWrite(X_DIR, HIGH); 
  for (int i = 0; i < abs(steps); i++) {
    digitalWrite(X_STEP, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(X_STEP, LOW);
    delayMicroseconds(stepDelay);
  }
}
