/*
 * Utility routines for use by virtual hosts in terminal mode
 * 
 * Mostly dealing with interfacing with the terminal itself
 * 
 */

// -----------------------------------------------------------------------------------
// MODULE INTERNAL ROUTINES

void vh_setup()
{
  output_mask = OUTPUT_MASK_SCREEN;
  input_mask = TB_INPUT_MASK_KEYBOARD;
}


void vh_OutChar(char c)
{
  while (TB_RX_FULL()) {
    yield();
  }
  PUSH_TB_RX(c);
}


void vh_PrintMsg(const char* m)
{
  while (*m != 0) {
    vh_OutChar((char) *m++);
  }
  vh_OutChar(NL);
  vh_OutChar(CR);
}


void vh_PrintPrompt(const char* m)
{
  while (*m != 0) {
    vh_OutChar((char) *m++);
  }
}


void vh_PrintString(char* m)
{
  while (*m != 0) {
    vh_OutChar(*m++);
  }
  vh_OutChar(NL);
  vh_OutChar(CR);
}


void vh_ClearScreen()
{
  // Clear screen (ANSI)
  vh_OutChar(ESC);
  vh_OutChar('[');
  vh_OutChar('2');
  vh_OutChar('J');
}


void vh_DeleteLines(uint8_t row, uint8_t num)
{
  vh_SetCursor(row, 1);

  // Delete Line(s) (ANSI)
  vh_OutChar(ESC);
  vh_OutChar('[');

  if (num > 9) {
    if (num > 99) num = 99;
    // Tens digit)
    vh_OutChar('0' + (num/10));
    // Units digit
    vh_OutChar('0' + (num%10));
  } else {
    // Units digit only
    vh_OutChar('0' + num);
  }

  // Terminator
  vh_OutChar('M');
}


void vh_SetCursor(uint8_t row, uint8_t col)
{
  // Cursor Position (ANSI)
  vh_OutChar(ESC);
  vh_OutChar('[');

  if (row > 9) {
    if (row > 99) row = 99;
    // Tens digit)
    vh_OutChar('0' + (row/10));
    // Units digit
    vh_OutChar('0' + (row%10));
  } else {
    // Units digit only
    vh_OutChar('0' + row);
  }

  vh_OutChar(';');

  if (col > 9) {
    if (col > 99) col = 99;
    // Tens digit)
    vh_OutChar('0' + (col/10));
    // Units digit
    vh_OutChar('0' + (col%10));
  } else {
    // Units digit only
    vh_OutChar('0' + col);
  }

  // Terminator
  vh_OutChar('H');
}


void vh_CharacterInsertEnable(bool en)
{
  // Enable/Disable Character Insertion (ANSI)
  vh_OutChar(ESC);
  vh_OutChar('[');
  vh_OutChar('4');
  vh_OutChar(en ? 'h' : 'l');
}

