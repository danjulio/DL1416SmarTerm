/* ******************************************************************************************
    Eliza chat bot - ported from the PC port done by Patricia Danielson and Paul Hashfield
    of the original Creative Computing port of Eliza to BASIC (primarily because this is the
    only source I could find to download to make copying the strings easy...).

 * ******************************************************************************************/
 

// ===========================================================================================
// Constants
// ===========================================================================================
const int e_MaxLineLength = 80;
const int e_TimeoutSecs = 300;


// ===========================================================================================
// Messages
// ===========================================================================================
//                     |         48-character width                     |
const char e_Intro[] = "THE ORIGINAL ELIZA PROGRAM WAS WRITTEN IN 1966  " \
                       "BY JOSEPH WEIZENBAUM. IT WAS BASED ON THERAPIST " \
                       "CARL ROGERS OPEN ENDED QUESTIONING. ALTHOUGH    " \
                       "VERY SIMPLE, IT WAS CONSIDERED A FORERUNNER OF  " \
                       "THINKING MACHINES.  WEIZENBAUM WAS SHOCKED TO   " \
                       "SEE HOW SERIOUSLY PEOPLE TOOK IT AND BECAME A   " \
                       "CRITIC OF AI.\r\n\r\nPRESS ANY KEY TO TRY IT OUT.";

const char e_MsgHello[] = "HI! I'M ELIZA. WHAT'S YOUR PROBLEM?";
const char e_MsgPrompt[] = "? ";
const char e_MsgBye[] = "O.K. IF YOU FEEL THAT WAY I'LL SHUT UP....";
const char e_MsgNoRepeat[] = "PLEASE DON'T REPEAT YOURSELF!";
const char e_MsgElaborate[] = "YOU WILL HAVE TO ELABORATE MORE FOR ME TO HELP YOU";


// ===========================================================================================
// Keywords
// ===========================================================================================
const int e_N1 = 36;
const char* e_keywords[] = {
  "CAN YOU ",
  "CAN I ",
  "YOU ARE ",
  "YOU'RE ",
  "I DON'T ",
  "I FEEL ",
  "WHY DON'T YOU ",
  "WHY CAN'T I ",
  "ARE YOU ",
  "I CAN'T ",
  "I AM ",
  "I'M ",
  "YOU ",
  "I WANT ",
  "WHAT ",
  "HOW ",
  "WHO ",
  "WHERE ",
  "WHEN ",
  "WHY ",
  "NAME ",
  "CAUSE ",
  "SORRY ",
  "DREAM ",
  "HELLO ",
  "HI ",
  "MAYBE ",
  "NO",
  "YOUR ",
  "ALWAYS ",
  "THINK ",
  "ALIKE ",
  "YES ",
  "FRIEND ",
  "COMPUTER",
  "NOKEYFOUND"
};

// ===========================================================================================
// String data for conjugations
// ===========================================================================================
const int e_N2 = 14;

const char* e_wordin[] = {
  " ARE ",
  " WERE ",
  " YOU ",
  " YOUR ",
  " I'VE ",
  " I'M ",
  " ME "
};

const char* e_wordout[] = {
  " AM ",
  " WAS ",
  " I ",
  " MY ",
  " YOU'VE ",
  " YOU'RE ",
  " YOU "
};


// ===========================================================================================
// Replies
// ===========================================================================================
const int e_N3 = 112;

const char* e_replies[] = {
  "DON'T YOU BELIEVE THAT I CAN*",
  "PERHAPS YOU WOULD LIKE TO BE LIKE ME*",
  "YOU WANT ME TO BE ABLE TO*",
  "PERHAPS YOU DON'T WANT TO*",
  "DO YOU WANT TO BE ABLE TO*",
  "WHAT MAKES YOU THINK I AM*",
  "DOES IT PLEASE YOU TO BELIEVE I AM*",
  "PERHAPS YOU WOULD LIKE TO BE*",
  "DO YOU SOMETIMES WISH YOU WERE*",
  "DON'T YOU REALLY*",
  "WHY DON'T YOU*",
  "DO YOU WISH TO BE ABLE TO*",
  "DOES THAT TROUBLE YOU*",
  "DO YOU OFTEN FEEL*",
  "DO YOU OFTEN FEEL*",
  "DO YOU ENJOY FEELING*",
  "DO YOU REALLY BELIEVE I DON'T*",
  "PERHAPS IN GOOD TIME I WILL*",
  "DO YOU WANT ME TO*",
  "DO YOU THINK YOU SHOULD BE ABLE TO*",
  "WHY CAN'T YOU*",
  "WHY ARE YOU INTERESTED IN WHETHER OR NOT I AM*",
  "WOULD YOU PREFER IF I WERE NOT*",
  "PERHAPS IN YOUR FANTASIES I AM*",
  "HOW DO YOU KNOW YOU CAN'T*",
  "HAVE YOU TRIED?",
  "PERHAPS YOU CAN NOW*",
  "DID YOU COME TO ME BECAUSE YOU ARE*",
  "HOW LONG HAVE YOU BEEN*",
  "DO YOU BELIEVE IT IS NORMAL TO BE*",
  "DO YOU ENJOY BEING*",
  "WE WERE DISCUSSING YOU--NOT ME.",
  "OH, I*",
  "YOU'RE NOT REALLY TALKING ABOUT ME, ARE YOU?",
  "WHAT WOULD IT MEAN TO YOU IF YOU GOT*",
  "WHY DO YOU WANT*",
  "SUPPOSE YOU SOON GOT*",
  "WHAT IF YOU NEVER GOT*",
  "I SOMETIMES ALSO WANT*",
  "WHY DO YOU ASK?",
  "DOES THAT QUESTION INTEREST YOU?",
  "WHAT ANSWER WOULD PLEASE YOU THE MOST?",
  "WHAT DO YOU THINK?",
  "ARE SUCH QUESTIONS ON YOUR MIND OFTEN?",
  "WHAT IS IT THAT YOU REALLY WANT TO KNOW?",
  "HAVE YOU ASKED ANYONE ELSE?",
  "HAVE YOU ASKED SUCH QUESTIONS BEFORE?",
  "WHAT ELSE COMES TO MIND WHEN YOU ASK THAT?",
  "NAMES DON'T INTEREST ME.",
  "I DON'T CARE ABOUT NAMES --PLEASE GO ON.",
  "IS THAT THE REAL REASON?",
  "DON'T ANY OTHER REASONS COME TO MIND?",
  "DOES THAT REASON EXPLAIN ANYTHING ELSE?",
  "WHAT OTHER REASONS MIGHT THERE BE?",
  "PLEASE DON'T APOLOGIZE!",
  "APOLOGIES ARE NOT NECESSARY.",
  "WHAT FEELINGS DO YOU HAVE WHEN YOU APOLOGIZE?",
  "DON'T BE SO DEFENSIVE!",
  "WHAT DOES THAT DREAM SUGGEST TO YOU?",
  "DO YOU DREAM OFTEN?",
  "WHAT PERSONS APPEAR IN YOUR DREAMS?",
  "ARE YOU DISTURBED BY YOUR DREAMS?",
  "HOW DO YOU DO ...PLEASE STATE YOUR PROBLEM.",
  "YOU DON'T SEEM QUITE CERTAIN.",
  "WHY THE UNCERTAIN TONE?",
  "CAN'T YOU BE MORE POSITIVE?",
  "YOU AREN'T SURE?",
  "DON'T YOU KNOW?",
  "ARE YOU SAYING NO JUST TO BE NEGATIVE?",
  "YOU ARE BEING A BIT NEGATIVE.",
  "WHY NOT?",
  "ARE YOU SURE?",
  "WHY NO?",
  "WHY ARE YOU CONCERNED ABOUT MY*",
  "WHAT ABOUT YOUR OWN*",
  "CAN YOU THINK OF A SPECIFIC EXAMPLE?",
  "WHEN?",
  "WHAT ARE YOU THINKING OF?",
  "REALLY, ALWAYS?",
  "DO YOU REALLY THINK SO?",
  "BUT YOU ARE NOT SURE YOU*",
  "DO YOU DOUBT YOU*",
  "IN WHAT WAY?",
  "WHAT RESEMBLANCE DO YOU SEE?",
  "WHAT DOES THE SIMILARITY SUGGEST TO YOU?",
  "WHAT OTHER CONNECTIONS DO YOU SEE?",
  "COULD THERE REALLY BE SOME CONNECTION?",
  "HOW?",
  "YOU SEEM QUITE POSITIVE.",
  "ARE YOU SURE?",
  "I SEE.",
  "I UNDERSTAND.",
  "WHY DO YOU BRING UP THE TOPIC OF FRIENDS?",
  "DO YOUR FRIENDS WORRY YOU?",
  "DO YOUR FRIENDS PICK ON YOU?",
  "ARE YOU SURE YOU HAVE ANY FRIENDS?",
  "DO YOU IMPOSE ON YOUR FRIENDS?",
  "PERHAPS YOUR LOVE FOR FRIENDS WORRIES YOU.",
  "DO COMPUTERS WORRY YOU?",
  "ARE YOU TALKING ABOUT ME IN PARTICULAR?",
  "ARE YOU FRIGHTENED BY MACHINES?",
  "WHY DO YOU MENTION COMPUTERS?",
  "WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM?",
  "DON'T YOU THINK COMPUTERS CAN HELP PEOPLE?",
  "WHAT IS IT ABOUT MACHINES THAT WORRIES YOU?",
  "SAY, DO YOU HAVE ANY PSYCHOLOGICAL PROBLEMS?",
  "WHAT DOES THAT SUGGEST TO YOU?",
  "I SEE.",
  "I'M NOT SURE I UNDERSTAND YOU FULLY.",
  "COME COME ELUCIDATE YOUR THOUGHTS.",
  "CAN YOU ELABORATE ON THAT?",
  "THAT IS QUITE INTERESTING."
};


// ===========================================================================================
// Data for finding right replies (original BASIC 1-based indices)
// ===========================================================================================
const int e_replyData[] = {
  1, 3, 4, 2, 6, 4, 6, 4, 10, 4, 14, 3, 17, 3, 20, 2, 22, 3, 25, 3,
  28, 4, 28, 4, 32, 3, 35, 5, 40, 9, 40, 9, 40, 9, 40, 9, 40, 9, 40, 9,
  49, 2, 51, 4, 55, 4, 59, 4, 63, 1, 63, 1, 64, 5, 69, 5, 74, 2, 76, 4,
  80, 3, 83, 7, 90, 3, 93, 6, 99, 7, 106, 6
};


// ===========================================================================================
// Variables - based on original source BASIC variables names (ugh...I know...but easier to port)
// ===========================================================================================

// Eliza-code variables
int e_S[e_N1];
int e_R[e_N1];
int e_N[e_N1];
char e_I$[e_MaxLineLength];
char e_P$[e_MaxLineLength];
char e_F$[e_MaxLineLength];
char e_C$[e_MaxLineLength];
char e_T$[e_MaxLineLength];
int e_K;
int e_L;
int e_X;
int e_Y;

// Control variables
bool e_isIntro;
int e_inputStringIndex;
unsigned long e_lastKeyPressT;



// ===========================================================================================
// Main entry points
// ===========================================================================================
void eliza_setup()
{
  // Parse replyData array (convert to C's 0-based indices)
  for (int i = 0; i < e_N1; i++) {
    e_S[i] = e_replyData[i * 2] - 1;
    e_R[i] = e_S[i];
    e_N[i] = e_S[i] + e_replyData[i * 2 + 1] - 1;
  }

  // Initialize variables
  e_isIntro = true;
  e_inputStringIndex = 0;

  e_PrintMsg(e_Intro);
}


void eliza_loop()
{
  bool isDone = false;
  bool loopBreak;
  char c;

  if (e_isIntro) {
    if (TB_TX_AVAIL()) {
      POP_TB_TX();
      e_isIntro = false;
      e_ClearScreen();
      e_PrintMsg(e_MsgHello);
      e_PrintPrompt();
      e_lastKeyPressT = millis();
    }
  } else {
    if (TB_TX_AVAIL()) {
      c = PEEK_TB_TX();
      POP_TB_TX();
      e_lastKeyPressT = millis();

      if (c == ESC) {
        // Immediate Restart
        e_ClearScreen();
        eliza_setup();
      } else {
        // Process input
        if (e_InputString(c)) {
          sprintf(e_T$, "  %s  ", e_I$);
          (void) strcpy(e_I$, e_T$);

          // Look for exit command
          e_X = strlen(e_I$) - 3;
          for (e_L = 0; e_L < e_X; e_L++) {
            if (strcmp(e_MID$(e_I$, e_L, 4), "SHUT") == 0) {
              isDone = true;
              break;
            }
          }
          if (isDone) {
            e_PrintMsg(e_MsgBye);
            delay(1000);  // Let the user see this
            e_ClearScreen();
            eliza_setup();
            return;
          }

          // Evaluate ELIZA
          if (strcmp(e_I$, e_P$) == 0) {
            // No repeating oneself!
            e_PrintMsg(e_MsgNoRepeat);
            e_PrintPrompt();
          } else {
            // Find keyword
            //   on exit of this block:
            //   loopBreak set if keyword was found
            //   e_K points to keyword or last keyword entry (if no keyword was found)
            //   e_L points to start of keyword in string if one is found
            loopBreak = false;
            for (e_K = 0; (e_K < e_N1) && !loopBreak; e_K++) {
              e_X = strlen(e_I$);
              e_Y = strlen(e_keywords[e_K]);
              if (e_X < e_Y) continue;  // Skip keywords that are longer than our input
              e_X = e_X - e_Y;
              for (e_L = 0; e_L < (e_X + 1); e_L++) {
                if (strcmp(e_MID$(e_I$, e_L, e_Y), e_keywords[e_K]) == 0) {
                  // Found a match
                  loopBreak = true;
                  if (e_K == 12) {
                    // Special case for YOU/YOUR
                    if (strcmp(e_MID$(e_I$, e_L, strlen(e_keywords[28])), e_keywords[28]) == 0) {
                      e_K = 28;
                    }
                  }
                  break;
                }
              }
            }
            if (loopBreak) {
              // Decrement e_K to undo final loop increment
              e_K--;
            } else {
              // No keyword found
              e_K = e_N1 - 1;
            }

            if (loopBreak) {
              // Found keyword: Take part of string and conjugate it using list of strings to be swapped
              (void) strcpy(e_F$, e_keywords[e_K]);
              sprintf(e_C$, " %s ", e_RIGHT$(e_I$, strlen(e_I$) - strlen(e_F$) - e_L));

              for (e_X = 0; e_X < e_N2 / 2; e_X++) {
                for (e_L = 0; e_L < strlen(e_C$); e_L++) {
                  if (((e_L + strlen(e_wordin[e_X])) <= strlen(e_C$)) &&
                      (strcmp(e_MID$(e_C$, e_L, strlen(e_wordin[e_X])), e_wordin[e_X]) == 0)) {
                    sprintf(e_T$, "%s%s%s", e_LEFT$(e_C$, e_L), e_wordout[e_X], e_RIGHT$(e_C$, strlen(e_C$) - e_L - strlen(e_wordin[e_X])));
                    (void) strcpy(e_C$, e_T$);
                    e_L = e_L + strlen(e_wordout[e_X]);
                  } else {
                    if (((e_L + strlen(e_wordout[e_X])) <= strlen(e_C$)) &&
                        (strcmp(e_MID$(e_C$, e_L, strlen(e_wordout[e_X])), e_wordout[e_X]) == 0)) {
                      sprintf(e_T$, "%s%s%s", e_LEFT$(e_C$, e_L), e_wordin[e_X], e_RIGHT$(e_C$, strlen(e_C$) - e_L - strlen(e_wordout[e_X])));
                      (void) strcpy(e_C$, e_T$);
                      e_L = e_L + strlen(e_wordin[e_X]);
                    }
                  }
                }
              }

              while (e_C$[1] == ' ') {
                // Only 1 space
                (void) strcpy(e_C$, e_RIGHT$(e_C$, strlen(e_C$) - 1));
              }
              for (e_L = 0; e_L < strlen(e_C$); e_L++) {
                // Strip exclamation points
                while (strcmp(e_MID$(e_C$, e_L, 1), "!") == 0) {
                  sprintf(e_T$, "%s%s", e_LEFT$(e_C$, e_L - 1), e_RIGHT$(e_C$, strlen(e_C$) - e_L));
                  (void) strcpy(e_C$, e_T$);
                }
              }
            }

            // Now, using the keyword number (K) get reply
            strcpy(e_F$, e_replies[e_R[e_K]]);

            e_R[e_K] = e_R[e_K] + 1;
            if (e_R[e_K] > e_N[e_K]) {
              e_R[e_K] = e_S[e_K];
            }
            if (strcmp(e_RIGHT$(e_F$, 1), "*") != 0) {
              e_PrintString(e_F$);
              (void) strcpy(e_P$, e_I$);
            } else {
              if (strcmp(e_C$, "   ") != 0) {
                sprintf(e_P$, "%s%s", e_LEFT$(e_F$, strlen(e_F$) - 1), e_C$);
                e_PrintString(e_P$);
                (void) strcpy(e_P$, e_I$);
              } else {
                e_PrintMsg(e_MsgElaborate);
              }
            }
            e_PrintPrompt();
          }
        }
      }
    } else {
      // Look for inactivity timeout
      if (InactivityTimeoutMsec(e_lastKeyPressT, e_TimeoutSecs * 1000)) {
        // Restart
        e_ClearScreen();
        eliza_setup();
      }
    }
  }
  delay(1);
}


// ===========================================================================================
// Internal routines
// ===========================================================================================
bool e_InputString(char c)
{
  boolean retVal = false;
  boolean validChar = true;
  boolean echoChar = true;

  if ((c == CR) || (c == NL)) {
    if (e_inputStringIndex > 0) {
      e_I$[e_inputStringIndex] = 0; // Terminate line
      e_inputStringIndex = 0;
      retVal = true;
    }
    if (c == CR) {
      // Also move to next line (NL automatically forces CR with out setup in ElizaInit)
      e_OutChar(NL);
    }
    validChar = false;
  } else if ((c == BS) || (c == DEL)) {
    // Backspace one character in the buffer
    if (e_inputStringIndex > 0) e_inputStringIndex--;
    validChar = false;
  } else if (c < 0x20) {
    // Ignore this character (control characters and apostrophes)
    validChar = false;
    echoChar = false;
  } else if ((c >= 'a') && (c <= 'z')) {
    // Convert to upper-case
    c -= 0x20;
  }

  if (validChar) {
    if (e_inputStringIndex < (e_MaxLineLength - 2)) {
      e_I$[e_inputStringIndex++] = c;
    }
  }

  if (echoChar) {
    e_OutChar(c);
  }

  return (retVal);
}


void e_OutChar(char c)
{
  while (TB_RX_FULL()) {
    delay(1);
  }
  PUSH_TB_RX(c);
}


void e_PrintMsg(const char* m)
{
  while (*m != 0) {
    e_OutChar((char) *m++);
  }
  e_OutChar(NL);
  e_OutChar(CR);
}


void e_PrintPrompt()
{
  const char* m = e_MsgPrompt;

  while (*m != 0) {
    e_OutChar((char) *m++);
  }
}


void e_PrintString(char* m)
{
  while (*m != 0) {
    e_OutChar(*m++);
  }
  e_OutChar(NL);
  e_OutChar(CR);
}

void e_ClearScreen()
{
  // Clear screen (ANSI)
  e_OutChar(ESC);
  e_OutChar('[');
  e_OutChar('2');
  e_OutChar('J');
}


char* e_MID$(char* s, int start, int len)
{
  int i = 0;
  static char midArray[e_MaxLineLength];

  while (len--) {
    midArray[i++] = *(s + start++);
  }
  midArray[i] = 0;

  return (midArray);
}


char* e_RIGHT$(char* s, int len)
{
  int i = 0;
  int l = strlen(s) - len;
  static char rightArray[e_MaxLineLength];

  while (len--) {
    rightArray[i++] = *(s + l++);
  }
  rightArray[i] = 0;

  return (rightArray);
}


char* e_LEFT$(char* s, int len)
{
  int i = 0;
  static char leftArray[e_MaxLineLength];

  while (len--) {
    leftArray[i++] = *s++;
  }
  leftArray[i] = 0;

  return (leftArray);
}

