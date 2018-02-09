/* ******************************************************************************************
 *  Persistent Storage - Initialization and access routines for configuration information
 *  stored in the internal eeprom.
 *  
 *  * LAYOUT
 *     Offset   Length    Description
 *     -------------------------------------------------------------------------------
 *         0        2     Magic Bytes - Indicates EEPROM storage has been initialized
 *         2        2     Host IF Baud Rate
 *         4        2     Printer Baud Rate
 *         6        1     Local Echo Enable (Enable = 1, Disable = 0) - Applies to terminal mode only
 *         7        1     Host IF Flow Control (XON/XOFF) Enable (Enable = 1, Disable = 0)
 *         8        1     Cursor Type (Blink = 1, Solid = 0)
 *         9        1     Tab Width
 *         10       1     CR/LF Handling (Convert either to CR+LF=1, Individual=0) - Applies
 *                        to terminal mode only
 *         11       1     Line Wrap (Enable = 1, Disable = 0) - Applies to terminal mode only
 *  
 * ******************************************************************************************/
#include <EEPROM.h>


// -----------------------------------------------------------------------------------
// MODULE CONSTANTS

// Magic Bytes are used to validate that the EEPROM storage contains valid data.  EEPROM
// data is not loaded by the Arduino development environment so this module must
// initialize EEPROM one time.  The Magic Bytes let the module "know" the EEPROM has
// already been initialized.  The value of the Magic Bytes must change if the EEPROM
// layout or contents change in a future firmware revision.
#define PS_MAGIC_BYTES    0xFACE
#define PS_MB_ADDR        0x0000
#define PS_MB_LEN         2

// Baud Rate configuration
#define PS_HOST_BAUD_FD   9600
#define PS_HOST_BAUD_ADDR (PS_MB_ADDR + PS_MB_LEN)
#define PS_HOST_ADDR_LEN  2

#define PS_PRT_BAUD_FD    9600
#define PS_PRT_BAUD_ADDR  (PS_HOST_BAUD_ADDR + PS_HOST_ADDR_LEN)
#define PS_PRT_BAUD_LEN   2

// Terminal Local Echo Enable
#define PS_TERM_ECHO_FD   1
#define PS_TERM_ECHO_ADDR (PS_PRT_BAUD_ADDR + PS_PRT_BAUD_LEN)
#define PS_TERM_ECHO_LEN  1

// Host IF Flow Control Enable
#define PS_HOST_FC_FD     1
#define PS_HOST_FC_ADDR   (PS_TERM_ECHO_ADDR + PS_TERM_ECHO_LEN)
#define PS_HOST_FC_LEN    1

// Cursor Type
#define PS_CURSOR_FD      1
#define PS_CURSOR_ADDR    (PS_HOST_FC_ADDR + PS_HOST_FC_LEN)
#define PS_CURSOR_LEN     1

// Tab Width
#define PS_TAB_FD         1
#define PS_TAB_ADDR       (PS_CURSOR_ADDR + PS_CURSOR_LEN)
#define PS_TAB_LEN        1

// CR/LF Handling
#define PS_CRLF_FD        0
#define PS_CRLF_ADDR      (PS_TAB_ADDR + PS_TAB_LEN)
#define PS_CRLF_LEN       1

// Line Wrap
#define PS_LINEWRAP_FD    1
#define PS_LINEWRAP_ADDR  (PS_CRLF_ADDR + PS_CRLF_LEN)
#define PS_LINEWRAP_LEN   1



// -----------------------------------------------------------------------------------
// MODULE API ROUTINES

void PsInit() {
  // Check if we need to initialize the EEPROM storage
  if ((EEPROM.read(PS_MB_ADDR) != (PS_MAGIC_BYTES >> 8)) ||
      (EEPROM.read(PS_MB_ADDR + 1) != (PS_MAGIC_BYTES & 0xFF))) {
        
    PsRestoreFactoryDefaults();
  }
}


int PsGetHostBaud() {
  return ((EEPROM.read(PS_HOST_BAUD_ADDR) << 8) | EEPROM.read(PS_HOST_BAUD_ADDR+1));
}


void PsSetHostBaud(int baud) {
  EEPROM.write(PS_HOST_BAUD_ADDR  , baud >> 8);
  EEPROM.write(PS_HOST_BAUD_ADDR+1, baud & 0xFF);
}


int PsGetPrtBaud() {
  return ((EEPROM.read(PS_PRT_BAUD_ADDR) << 8) | EEPROM.read(PS_PRT_BAUD_ADDR+1));
}


void PsSetPrtBaud(int baud) {
  EEPROM.write(PS_PRT_BAUD_ADDR  , baud >> 8);
  EEPROM.write(PS_PRT_BAUD_ADDR+1, baud & 0xFF);
}


boolean PsGetTermLocalEcho() {
  if (EEPROM.read(PS_TERM_ECHO_ADDR) == 0) {
    return false;
  } else {
    return true;
  }
}


void PsSetTermLocalEcho(boolean en) {
  EEPROM.write(PS_TERM_ECHO_ADDR, en ? 1 : 0);
}


boolean PsGetHostFlowControl() {
  if (EEPROM.read(PS_HOST_FC_ADDR) == 0) {
    return false;
  } else {
    return true;
  }
}


void PsSetHostFlowControl(boolean en) {
  EEPROM.write(PS_HOST_FC_ADDR, en ? 1 : 0);
}


boolean PsGetCursorBlink() {
  if (EEPROM.read(PS_CURSOR_ADDR) == 0) {
    return false;
  } else {
    return true;
  }
}


void PsSetCursorBlink(boolean en) {
  EEPROM.write(PS_CURSOR_ADDR, en ? 1 : 0);
}


int PsGetTabWidth() {
  return(EEPROM.read(PS_TAB_ADDR));
}


void PsSetTabWidth(int w) {
  EEPROM.write(PS_TAB_ADDR, w);
}


boolean PsGetConvertCRLF() {
  if (EEPROM.read(PS_CRLF_ADDR) == 0) {
    return false;
  } else {
    return true;
  }
}


void PsSetConvertCRLF(boolean en) {
  EEPROM.write(PS_CRLF_ADDR, en ? 1 : 0);
}


boolean PsGetLinewrap() {
  if (EEPROM.read(PS_LINEWRAP_ADDR) == 0) {
    return false;
  } else {
    return true;
  }
}


void PsSetLinewrap(boolean en) {
  EEPROM.write(PS_LINEWRAP_ADDR, en ? 1 : 0);
}


// -----------------------------------------------------------------------------------
// MODULE INTERNAL ROUTINES

void PsCheckAndLoad8(uint16_t a, uint8_t d) {
  if (EEPROM.read(a) != d) {
    EEPROM.write(a, d);
  }
}


void PsCheckAndLoad16(uint16_t a, uint16_t d) {
  uint8_t dh = d >> 8;
  uint8_t dl = d & 0xFF;
  
  if (EEPROM.read(a) != dh) {
    EEPROM.write(a, dh);
  }
  a++;
  if (EEPROM.read(a) != dl) {
    EEPROM.write(a, dl);
  }
}


void PsRestoreFactoryDefaults() {
  // First delete the Magic Bytes so that if this process is interrupted (e.g. unexpected power-down)
  // we will re-initialize EEPROM on the next power-up.
  EEPROM.write(PS_MB_ADDR, 0);
  EEPROM.write(PS_MB_ADDR+1, 0);

  // Load our default values
  PsCheckAndLoad16(PS_HOST_BAUD_ADDR, PS_HOST_BAUD_FD);
  PsCheckAndLoad16(PS_PRT_BAUD_ADDR, PS_PRT_BAUD_FD);
  PsCheckAndLoad8(PS_TERM_ECHO_ADDR, PS_TERM_ECHO_FD);
  PsCheckAndLoad8(PS_HOST_FC_ADDR, PS_HOST_FC_FD);
  PsCheckAndLoad8(PS_CURSOR_ADDR, PS_CURSOR_FD);
  PsCheckAndLoad8(PS_TAB_ADDR, PS_TAB_FD);
  PsCheckAndLoad8(PS_CRLF_ADDR, PS_CRLF_FD);
  PsCheckAndLoad8(PS_LINEWRAP_ADDR, PS_LINEWRAP_FD);

  // Finally set the Magic Bytes to indicate a successful restore
  EEPROM.write(PS_MB_ADDR, PS_MAGIC_BYTES >> 8);
  EEPROM.write(PS_MB_ADDR+1, PS_MAGIC_BYTES & 0xFF);
}


