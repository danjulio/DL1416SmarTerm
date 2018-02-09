/* ******************************************************************************************
 *  dl1416SmartTerm - Terminal program for the DL1416 display array and hall-effect serial
 *  keyboard with built-in Tiny Basic based on Scott Lawrence's "TinyBasic Plus" which was
 *  based on Mike Field's port.  Provide ANSI-inspired terminal functionality with a
 *  48 character x 12 line screens, support for either USB or RS232 host interface, a 
 *  printer/aux RS232 interface, sound and general IO (for Tiny Basic use).
 *  
 *  A switch selects terminal or Tiny Basic computer operation.  While in terminal mode
 *  a configuration menu may be brought up to change terminal operating characteristics.
 *  Ten digital IOs and two analog inputs are available in Tiny Basic mode.  Analog inputs
 *  are referenced to the Teensy's 3.3 volt rail.
 *  
 *  Tiny Basic has access to a FAT32 formatted SD Card for file storage.  Hot swapping is
 *  not supported.  It also has access to the Teensy DAC out for more sophisticated sound
 *  generation than the BELL control character (although it has access to that too).
 *  
 *  Teensy 3.2 IO Assignment
 *    D0      - Serial1 RX (Host IF port)
 *    D1      - Serial1 TX
 *    D2      - Profiling Output (High during terminal evaluation, low otherwise)
 *    D3      - Terminal/nComputer select
 *    D4      - SD Card CSn
 *    D5      - 
 *    D6      -
 *    D7      - Serial3 RX (Aux IF port)
 *    D8      - Serial3 TX
 *    D9      - Serial2 RX (Keyboard input)
 *    D10     - Serial2 TX (DL1416 output/Keyboard output for BEL)
 *    D11     - SD Card MOSI
 *    D12     - SD Card MISO
 *    D13     - SD Card SCK
 *    D14/A0  - Digital input/output (D0) / Analog input (A0) for Tiny Basic
 *    D15/A1  - Digital input/output (D1) / Analog input (A1) for Tiny Basic
 *    D16/A2  - Digital input/output (D2) / Analog input (A2) for Tiny Basic
 *    D17/A3  - Digital input/output (D3) / Analog input (A3) for Tiny Basic
 *    D18/A4  - Digital input/output (D4) / Analog input (A4) for Tiny Basic
 *    D19/A5  - Digital input/output (D5) / Analog input (A5) for Tiny Basic
 *    D20/A6  - Digital input/output (D6) / Analog input (A6) for Tiny Basic (PWM)
 *    D21/A7  - Digital input/output (D7) / Analog input (A7) for Tiny Basic (PWM)
 *    D22/A8  - Digital input/output (D8) / Analog input (A8) for Tiny Basic (PWM)
 *    D23/A9  - (unused, use seems to interfere with serial port 2)
 *    A10     -
 *    A11     -
 *    A12     - 
 *    A13     - 
 *    DAC/A14 - Audio DAC out
 *    D24     -
 *    D25     -
 *    D26/A15 -
 *    D27/A16 -
 *    D28/A17 -
 *    D29/A18 -
 *    D30/A19 -
 *    D31/A20 -
 *    D32     -
 *    D33     -
 *    
 *  Serial Port Assignment
 *                  Terminal         Computer
 *    Host USB:      Host IF          Aux keyboard/screen (output is selectable)
 *    Serial1 :      Host IF          Aux keyboard/screen (output is selectable)
 *    Serial2 :      Keyboard/DL1416  Keyboard/DL1416
 *    Serial3 :      Printer          Aux screen (output is selectable) (note)
 *  
 *  Printer Port Note
 *    The printer port is designed to work with my arduino program serial_2_parallel
 *    that converts a serial interface to a centronics parallel interface.  This program
 *    supports XON/XOFF flow control on printer RX data, and special control characters
 *    sent on the printer TX as follows:
 *      CTRL-E ("ENQ") : Request printer status sent when this program starts
 *      CTRL-X ("CAN") : Reset Printer and flush printer and serial_2_parallel buffers
 *    
 *    This program processes the following special control characters sent from the printer
 *    as follows:
 *      CTRL-F ("ACK") to indicate power on
 *      CTRL-U ("NAK") to indicate power off (when arduino still powered by USB)
 *      CTRL-R ("DC2") to indicate paper present status (when paper is loaded)
 *      CTRL-T ("DC4") to indicate paper out status
 *      
 *    It should work with other serial printers but will not collect status or provide the ability
 *    to quickly reset the printer (other than possible printer-specific escape sequences).
 *    
 *  Serial Port Flow Control
 *    Host USB/Serial1 interfaces send XON/XOFF on their TX ports (configurable)
 *    Printer Serial3 interface interprets XON/XOFF on its RX port
 *    
 *  ANSI sequences primarily taken from http://ascii-table.com/ansi-escape-sequences.php and
 *  http://vt100.net/docs/vt102-ug/chapter5.html and should support basic ANSI and VT102/220
 *  sessions.  Other ANSI sequences will be decoded but ignored.
 *    
 *  ANSI Sequences Implemented (numeric arguments in decimal ASCII notation are contained with <>)
 *    ESC[<line>;<column>H             Cursor Position
 *    ESC[<line>;<column>f             Horizontal and Vertical Position (same as Cursor Position)
 *    ESC[<value>A                     Cursor Up
 *    ESC[<value>B                     Cursor Down
 *    ESC[<value>C                     Cursor Forward
 *    ESC[<value>D                     Cursor Backward
 *    ESC[s                            Save Cursor Position
 *    ESC[u                            Restore Cursor Position
 *    ESC7                             Save Cursor Position, origin mode and cursor attributes (*)
 *    ESC8                             Restore Cursor Position, origin mode and cursor attributes (*)
 *    ESC[J                            Erase Down
 *    ESC[1J                           Erase Up
 *    ESC[2J                           Erase Display
 *    ESC[K                            Erase End of Line
 *    ESC[1K                           Erase Start of Line
 *    ESC[2K                           Erase Line
 *    ESC[<value>L                     Insert Line
 *    ESC[<value>M                     Delete Line
 *    ESC[<value>P                     Delete Character
 *    ESC[c / ESC[0c / ESCZ            Query Device Attributes (responds with VT102 ESC[?6c)
 *    ESC[5n                           Query Device Status (responds with Report Read, no malfunctions ESC[0n)
 *    ESC[6n                           Query Cursor Position (responds with Report Cursor Position ESC[<row>;<col>R)
 *    ESC[?15n                         Query Printer Status (responds with 
 *                                       ESC[?13n if printer is offline
 *                                       ESC[?11n if printer is out of paper
 *                                       ESC[?10n if printer is ready to print
 *    ESCc                             Reset Device
 *    ESCD                             Scroll Down One Line
 *    ESCE                             Next Line
 *    ESCM                             Scroll Up One Line
 *    ESCH                             Set Tab
 *    ESC[0g / ESC[g                   Clear Tab at position
 *    ESC[3g                           Clear all tabs
 *    ESC[<top>;<bot>r                 Set scrolling region
 *    ESC[r                            Reset scrolling region
 *    ESC[4h                           Enable Character Insertion
 *    ESC[4l                           Disable Character Insertion
 *    ESC[7h                           Enable Line Wrap
 *    ESC[7l                           Disable Line Wrap
 *    ESC[12h                          Disable Local Echo
 *    ESC[12l                          Enable Local Echo
 *    ESC[20h                          Set New Line Mode (new line also includes CR, CR sends CR+LF)
 *    ESC[20l                          Reset New Line Mode
 *    ESC[?6h                          Set Origin Mode
 *    ESC[?6l                          Reset Origin Mode
 *    ESC[?25h                         Make Cursor visible
 *    ESC[?25l                         Make Cursor invisible
 *    ESC[0<SPACE>q / ESC[1<SPACE>q    Cursor Blink (**)
 *    ESC[2<SPACE>q                    Cursor Solid (**)
 *    ESC[i / ESC[0i                   Print Screen or scrolling region (based on Printer Extend Mode)
 *    ESC[?1i                          Print Line with cursor (followed by CR/LF)
 *    ESC[4i                           Disable Print log
 *    ESC[5i                           Start Print log (send incoming data to printer instead of screen)
 *    ESC[?4i                          Turn off Auto Print
 *    ESC[?5i                          Turn on Auto Print (print line after cursor moved off line w/ LF, FF, VT)
 *    ESC[?18h                         Set FF as print screen terminator
 *    ESC[?18l                         Set nothing as print screen terminator
 *    ESC[?19h                         Set full screen to print during print screen
 *    ESC[?19l                         Set scrolling region to print during print screen
 *    ESC#<specifier>                  Line Attributes (IGNORED)
 *    ESC(<specifier>                  Select Character Set (IGNORED)
 *    ESC)<specifier>                  Select Character Set (IGNORED)
 *    ESC]<value>;<text>BEL            Operating System Command (IGNORED)
 *  Notes:
 *   *  This isn't ANSI compliant, instead of storing character attribute, we are storing cursor attributes
 *   ** These are VT520 commands
 *  
 *  Control Characters Implemented
 *    CTRL-C                           EOT - Break command for Tiny Basic (ignored in Terminal mode)
 *    CTRL-G                           BELL - sends BELL to keyboard (rings keyboard bell)
 *    CTRL-H                           BS - backspace cursor
 *    CTRL-I                           TAB - forwardspace cursor to next tab position (default values: 1, 2, 4
 *                                           or 8 positions, configurable)
 *    CTRL-J                           LF - Linefeed character (moves cursor down one row; may also
 *                                          be configured to include a CR)
 *    CTRL-K                           VT - Vertical tab (moves cursor up one position) (**)
 *    CTRL-L                           FF - Formfeed (clear screen) (**)
 *    CTRL-M                           CR - Carriage return (moves cursor to start of line; may also
 *                                          be configured to include a LF)
 *    ESC (*)                          Escape - starts escape sequence
 *    DEL                              Delete - backspace cursor, deleting character first (**)
 *    
 *    Notes:
 *     *  The EOT key on the keyboard is treated as an Escape in terminal mode and a CTRL-C
 *        (break) in Tiny Basic mode.
 *     ** These control characters are not VT100/ANSI compliant.  They exist for compatibility with
 *        other, more primitive, terminals.
 *  
 *  Anti-confusion note: The DL1416 display firmware interprets control characters in its own way
 *  that may not correspond with traditional functionality for those characters.  To complicate
 *  matters further, this firmware treats some control characters specially, either for its own
 *  purpose or for Tiny Basic's.  For example, this firmware uses SI and SO from the keyboard to
 *  control displaying the configuration menu.  Please see either the firmware or the dl1416_instructions
 *  file for the DL1416's control character command set.  Basically I used control characters however
 *  I wanted except at the highest terminal protocol level where I tried to map them to expected
 *  behavior.
 *
 *
 * Copyright (c) 2016-2017 Dan Julio
 * All rights reserved.
 * Released as open source in hopes that it may be helpful to someone else.  No warranties or other guarantees.
 *
 * Release Change Log
 * Version   Date          Author      Description
 * -----------------------------------------------------------------------------
 *   1.0     10-31-2016    DJD         Initial Version
 *   2.0     10-20-2017    DJD         Added sound capability to Tiny Basic using the
 *                                     Teensy 3 audio library (see tiny_basic.ino for
 *                                     details).  Incremented Tiny Basic version to 1.0.
 *   2.1     02-08-2018    DJD         Fixed bug in Tiny Basic NEW command when run from
 *                                     program.
 *  
 * ******************************************************************************************/


/* ******************************************************************************************
 *  User configuration constants
 * ******************************************************************************************/

// Version - "Major"."Minor"
//   Major - Major new functionality (user visible)
//   Minor - bug fixes or modifications to existing functionality
#define kTermVersion      "v1.0"

// Terminal display constants
#define kNumTermLines     12
#define kNumCharPerLine   48

// Buffer sizes (all must fit in Teensy 3 RAM with stack and other dynamic room left over)
#define kHostRxBufLen     256
#define kHostRxFcLen      (kHostRxBufLen - 64)
#define kHostTxBufLen     128
#define kTermDispBufLen   (kNumTermLines * kNumCharPerLine)
#define kTermOutputBufLen 64
#define kDL1416BufLen     (3 * kTermDispBufLen)
#define kPrinterRxBufLen  8
#define kPrinterTxBufLen  (kTermDispBufLen + (2 * kNumTermLines) + 2)
#define kTinyBasicRam     (32 * 1024)
#define kTinyBasicRxLen   256
#define kTinyBasicTxLen   64

// Time between characters sent to the DL1416 display to prevent overrun of its internal
// buffers since it can't keep up with data at 57600 baud (in micro-seconds)
#define kDL1416delay      500


// Uncomment out the following line to echo charactes received from the USB Host port to
// the Serial1 Host port (useful for debugging escape sequences for example, you can see
// what escape sequences have been sent when this program malfunctions).
//#define ECHO_USB_INCOMING 1


/* ******************************************************************************************
 *  Terminal internal constants
 * ******************************************************************************************/

// ASCII Characters
#define NUL     0x00
#define EOT     0x04
#define CR      '\r'
#define NL      '\n'
#define ENQ     0x05
#define ACK     0x06
#define BS      0x08
#define LF      0x0a
#define VT      0x0b
#define FF      0x0c
#define TAB     '\t'
#define BELL    0x07
#define SO      0x0e
#define SI      0x0f
#define DC1     0x11
#define DC2     0x12
#define DC3     0x13
#define DC4     0x14
#define NACK    0x15
#define XON     0x11
#define XOFF    0x13
#define CAN     0x18
#define SUB     0x1a
#define ESC     0x1b
#define SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC   0x03
#define CTRLH   0x08
#define CTRLS   0x13
#define CTRLX   0x18
#define DEL     0x7f


// Teensy Pins
#define TEENSY_PROFILE_OUT 2
#define SWITCH_IN          3


// Terminal Mode
enum kTermMode {
  kModeIsTerm = 0,
  kModeIsTinyBasic,
  kModeIsConfig
};


// Terminal character processing state
enum kTermProcState {
  kStateIsImm = 0,
  kStateIsEsc
};


// Terminal escape sequence processing state
enum kTermEscState {
  kStateEscIdle = 0,
  kStateEscEsc,             // Saw ESC character
  kStateEscQues,            // Saw ESC[? CSI
  kStateEscBracket,         // Saw ESC[ CSI
  kStateEscSpace,           // Saw ESC[<value>SPACE and waiting for command character
  kStateEscIgnore,          // Waiting for single character to end ignored sequences
  kStateEscIgnoreOSC        // Waiting for BEL character to end Operating System Command sequence
};


// Forward Declarations for macros
void IncPushIndex(int* indexPtr, int* indexCount, int maxIndex);
void IncPopIndex(int* indexPtr, int* indexCount, int maxIndex);
boolean IsControlChar(char c);


// Macros
#define TERM_BUFF_INDEX() (termLineIndex*kNumCharPerLine + termCharIndex)

// USB IF
#define PUSH_HOST_RX1(c)   hostRxBuffer1[hostRxPush1I] = c; IncPushIndex(&hostRxPush1I, &hostRxNum1, kHostRxBufLen)
#define PEEK_HOST_RX1()    hostRxBuffer1[hostRxPop1I]
#define POP_HOST_RX1()     IncPopIndex(&hostRxPop1I, &hostRxNum1, kHostRxBufLen)
#define HOST_REQ_FC_RX1()  (hostRxNum1 >= kHostRxFcLen)

#define PUSH_HOST_TX1(c)   hostTxBuffer1[hostTxPush1I] = c; IncPushIndex(&hostTxPush1I, &hostTxNum1, kHostTxBufLen)
#define PEEK_HOST_TX1()    hostTxBuffer1[hostTxPop1I]
#define POP_HOST_TX1()     IncPopIndex(&hostTxPop1I, &hostTxNum1, kHostTxBufLen)
#define HOST_TX1_FULL()    (hostTxNum1 >= kHostTxBufLen)

// Serial 1 IF
#define PUSH_HOST_RX2(c)   hostRxBuffer2[hostRxPush2I] = c; IncPushIndex(&hostRxPush2I, &hostRxNum2, kHostRxBufLen)
#define PEEK_HOST_RX2()    hostRxBuffer2[hostRxPop2I]
#define POP_HOST_RX2()     IncPopIndex(&hostRxPop2I, &hostRxNum2, kHostRxBufLen)
#define HOST_REQ_FC_RX2()  (hostRxNum2 >= kHostRxFcLen)

#define PUSH_HOST_TX2(c)   hostTxBuffer2[hostTxPush2I] = c; IncPushIndex(&hostTxPush2I, &hostTxNum2, kHostTxBufLen)
#define PEEK_HOST_TX2()    hostTxBuffer2[hostTxPop2I]
#define POP_HOST_TX2()     IncPopIndex(&hostTxPop2I, &hostTxNum2, kHostTxBufLen)
#define HOST_TX2_FULL()    (hostTxNum1 >= kHostTxBufLen)

// DL1416 IF
#define PUSH_DL1416(c)    dl1416TxBuffer[dl1416PushI] = c; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define PEEK_DL1416()     dl1416TxBuffer[dl1416PopI]
#define POP_DL1416()      IncPopIndex(&dl1416PopI, &dl1416Num, kDL1416BufLen)
#define DL1416_BUF_FULL() (dl1416Num >= kDL1416BufLen/2)

#define DL1416_CLS()      dl1416TxBuffer[dl1416PushI] = FF; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_SET_SHDW() dl1416TxBuffer[dl1416PushI] = SO; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_SET_DISP() dl1416TxBuffer[dl1416PushI] = SI; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_LD_DISP()  dl1416TxBuffer[dl1416PushI] = SUB; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)

#define DL1416_EN_CUR()   dl1416TxBuffer[dl1416PushI] = DC1; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_DIS_CUR()  dl1416TxBuffer[dl1416PushI] = DC2; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_CUR_SLD()  dl1416TxBuffer[dl1416PushI] = DC4; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)
#define DL1416_CUR_BLNK() dl1416TxBuffer[dl1416PushI] = DC3; IncPushIndex(&dl1416PushI, &dl1416Num, kDL1416BufLen)


// Terminal output IF
#define PUSH_TERM_OUT(c)  termOutputBuffer[termOutputPushI] = c; IncPushIndex(&termOutputPushI, &termOutputNum, kTermOutputBufLen)
#define PEEK_TERM_OUT()   termOutputBuffer[termOutputPopI]
#define POP_TERM_OUT()    IncPopIndex(&termOutputPopI, &termOutputNum, kTermOutputBufLen)


// Computer IF
#define PUSH_TB_TX(c)     tinyBasicTxBuffer[tinyBasicTxPushI] = c; IncPushIndex(&tinyBasicTxPushI, &tinyBasicTxNum, kTinyBasicTxLen)
#define PEEK_TB_TX()      tinyBasicTxBuffer[tinyBasicTxPopI]
#define POP_TB_TX()       IncPopIndex(&tinyBasicTxPopI, &tinyBasicTxNum, kTinyBasicTxLen)
#define TB_TX_AVAIL()     (tinyBasicTxNum > 0)
#define TB_TX_FULL()      (tinyBasicTxNum >= kTinyBasicTxLen)

#define PUSH_TB_RX(c)     tinyBasicRxBuffer[tinyBasicRxPushI] = c; IncPushIndex(&tinyBasicRxPushI, &tinyBasicRxNum, kTinyBasicRxLen)
#define PEEK_TB_RX()      tinyBasicRxBuffer[tinyBasicRxPopI]
#define POP_TB_RX()       IncPopIndex(&tinyBasicRxPopI, &tinyBasicRxNum, kTinyBasicRxLen)
#define TB_RX_FULL()      (tinyBasicRxNum >= kTinyBasicRxLen)


// Printer IF
#define PUSH_PRT_TX(c)    printerTxBuffer[printerTxPushI] = c; IncPushIndex(&printerTxPushI, &printerTxNum, kPrinterTxBufLen)
#define PEEK_PRT_TX()     printerTxBuffer[printerTxPopI]
#define POP_PRT_TX()      IncPopIndex(&printerTxPopI, &printerTxNum, kPrinterTxBufLen)
#define PRT_TX_FULL()     (printerTxNum >= kPrinterTxBufLen)

#define PUSH_PRT_RX(c)    printerRxBuffer[printerRxPushI] = c; IncPushIndex(&printerRxPushI, &printerRxNum, kPrinterRxBufLen)
#define PEEK_PRT_RX(c)    printerRxBuffer[printerRxPopI]
#define POP_PRT_RX()      IncPopIndex(&printerRxPopI, &printerRxNum, kPrinterRxBufLen)

#define PRT_ONLINE()      (printerOn == true)
#define PRT_PAPER_OUT()   (printerPaperOut == true)


// Value to write to AIRCR to reset the CPU - must spin after writing this value
// because there is a delay until the reset occurs
#define RESTART_VAL        0x05FA0004

// Maximum number of escape arguments in a mode-set or other multi-parameter escape sequence
#define MAX_ESC_ARGS       10

// Tiny Basic input sources
#define TB_INPUT_MASK_KEYBOARD   0x01
#define TB_INPUT_MASK_USB_HOST   0x02
#define TB_INPUT_MASK_SER_HOST   0x04
#define TB_INPUT_MASK_PRINTER    0x08



/* ******************************************************************************************
 *  Terminal related variables
 * ******************************************************************************************/

// Terminal State
enum kTermMode termMode;
enum kTermProcState termState;
enum kTermEscState escState;
int scrollTop;                  // Top line of scroll region
int scrollBot;                  // Bottom line of scroll region
int savedScrollTop;             // Saved version for Save/Restore Cursor Position with Attributes ESC sequence
int savedScrollBot;             // Saved version for Save/Restore Cursor Position with Attributes ESC sequence
int escArgs[MAX_ESC_ARGS];      // Holds the escape arguments as they come in
int curEscArgIndex;             // Index into escArgs and also lets us know how many arguments we collected
int lastRxPort;                 // Set to 1 or 2 to indicate which terminal-mode port most recently sent us data

boolean originMode;             // Set when origin mode is tied to scrollable region, clear for entire screen
boolean savedOriginMode;        // Saved version for Save/Restore Cursor Position with Attributes ESC sequence
boolean termLocalEcho;          // Echo keyboard characters to screen
boolean termConvertCRLF;        // Set to convert either CR or LF to CR+LF combinations
boolean termInsertChars;        // Set to insert characters at current position instead of overwriting
boolean hostFlowControlEn;      // Set to enable XON/XOFF flow control for the Serial1 Host IF
boolean hostInFc1;              // Set to true when the USB Host IF is in flow control (has sent XOFF)
boolean hostInFc2;              // Set to true when the Serial1 Host IF is in flow control
boolean printerInFc;            // Set to true when the printer IF is in flow control (has received XOFF)
boolean printerOn;              // Set to false by default and when we detect the printer turned back on after being turned off
boolean printerPaperOut;        // Set to false by default and when we detect the printer tell us it is out of paper; set when it
                                //   tells us it has paper again
boolean enablePrintLog;         // Set to true when sending incoming data to printer instead of display
boolean enableAutoPrint;        // Set to true when printing each line when the cursor moves off of it
boolean enablePrintRegion;      // Set to true when the print screen ESC sequence should only print the current scrolling region
boolean enableScreenPrintTerm;  // Set to true when the print screen ESC sequence follows with a FF character
boolean cursorVisible;          // Set when cursor is visible on the display
boolean savedCursorVisible;     // Saved version for Save/Restore Cursor Position with Attributes ESC sequence
boolean cursorBlink;            // Set when the cursor is blinking, clear when solid
boolean savedCursorBlink;       // Saved version for Save/Restore Cursor Position with Attributes ESC sequence
boolean linewrapEn;             // Set to enable line wrap (or clear for data past the right side of the display to be ignored)

// Tab related
int tabWidth;                   // Number of characters to use for tab commands
boolean tabArray[kNumCharPerLine];
 

// Host communication channel buffers
//   hostRxBuffers loaded by either USB Serial or Serial1, unloaded by terminal logic when
//     device is in terminal mode, available to Tiny Basic when it is running
//   hostTxBuffers loaded by either keyboard handler or terminal logic when device is in
//     terminal mode, available to Tiny Basic when it is running
char hostRxBuffer1[kHostRxBufLen];
char hostTxBuffer1[kHostTxBufLen];
char hostRxBuffer2[kHostRxBufLen];
char hostTxBuffer2[kHostTxBufLen];

// Host communication channel indices
int hostRxPush1I;
int hostRxPop1I;
int hostRxNum1;
int hostTxPush1I;
int hostTxPop1I;
int hostTxNum1;

int hostRxPush2I;
int hostRxPop2I;
int hostRxNum2;
int hostTxPush2I;
int hostTxPop2I;
int hostTxNum2;


// Master terminal data buffer - contains all current lines of text.
// Organized as a simple 2-dimensional array of kNumTermLines by kNumCharPerLine
char termBuffer[kTermDispBufLen];

// Terminal data/display buffer indices
int termCharIndex;                            // Position for next displayable character on a line
                                              //   (0 - kNumcharPerLine-1)
int termLineIndex;                            // Current line in the termBuffer for next displayable character
                                              //   (0 - kNumScrollLines-1)

int termSaveCharIndex, termSaveLineIndex;     // Saved versions for Save/Restore Cursor Position ESC sequences


// DL1416 display buffer - contains data being written to DL1416 display but subject to rate
// limiting to prevent a hardware over-run case
char dl1416TxBuffer[kDL1416BufLen];

// DL1416 display buffer indices
int dl1416PushI;
int dl1416PopI;
int dl1416Num;

// DL1416 timestamp used to rate control outgoing characters
unsigned long dl1416PrevT;

// Terminal output buffer - contains data being sent from the terminal in response to a command
char termOutputBuffer[kTermOutputBufLen];

// Terminal output buffer indices
int termOutputPushI;
int termOutputPopI;
int termOutputNum;


// Printer communication buffers
//  RX buffer - contains data being sent to the terminal (flow control)
//  TX buffer - contains data being sent to the printer
char printerRxBuffer[kPrinterRxBufLen];
char printerTxBuffer[kPrinterTxBufLen];

// Printer communication buffer indices
int printerRxPushI;
int printerRxPopI;
int printerRxNum;
int printerTxPushI;
int printerTxPopI;
int printerTxNum;


// Tiny Basic communication buffers
//  RX buffer - contains data being sent to the terminal for processing and display
//  TX buffer - contains data being sent to tiny basic after processing by the terminal
char tinyBasicRxBuffer[kTinyBasicRxLen];
char tinyBasicTxBuffer[kTinyBasicTxLen];

// Tiny Basic communication buffer indices
int tinyBasicRxPushI;
int tinyBasicRxPopI;
int tinyBasicRxNum;
int tinyBasicTxPushI;
int tinyBasicTxPopI;
int tinyBasicTxNum;


// Interval timer to schedule terminal activities so Tiny Basic can take main thread
//  TERM_SCHEDULE_PERIOD is in uSec
//
#define TERM_SCHEDULE_PERIOD 500
IntervalTimer termSchedulerObj;



/* ******************************************************************************************
 *  Arduino entry-points
 * ******************************************************************************************/

void setup() {
  PsInit();   // First so it can (re)initialize EEPROM if necessary
  SwitchInit();
  TermInit();
  if (termMode == kModeIsTinyBasic) {
    TinyBasicInit();
  }

  // TermEval profiling output
  pinMode(TEENSY_PROFILE_OUT, OUTPUT);
  digitalWrite(TEENSY_PROFILE_OUT, LOW);
}


// Each eval routine should execute as quickly as possible (and be non-blocking)
void loop() {
  switch(termMode) {
    case kModeIsTerm:
      TermEval();
      break;
    case kModeIsTinyBasic:
      TinyBasicEval();
      break;
    case kModeIsConfig:
      MenuEval();
      break;
  }
}



/* ******************************************************************************************
 *  Serial port handler subroutines
 * ******************************************************************************************/
// Incoming data from Host USB IF
void serialEvent() {
  char c;
  
  while (Serial.available()) {
    c = Serial.read();
    PUSH_HOST_RX1(c & 0x7F);
  }
}


// Incoming data from Host RS232 interface
void serialEvent1() {
  char c;

  while (Serial1.available()) {
    c = Serial1.read();
    PUSH_HOST_RX2(c & 0x7F);
  }
}


// Incoming data from Printer interface
void SerialEvent3() {
  char c;

  while (Serial3.available()) {
    c = Serial3.read();
    PUSH_PRT_RX(c & 0x7F);
  }
}



/* ******************************************************************************************
 *  Terminal related subroutines
 * ******************************************************************************************/

void TermInit() {

  // Initialize the communication interfaces
  Serial.begin(57600);  // Baud rate doesn't matter since this is USB
  Serial1.begin(PsGetHostBaud());
  Serial2.begin(57600); // Baud rate matches the DL1416 display board and keyboard controller
  Serial3.begin(PsGetPrtBaud());

  // Initialize the host communication buffers
  hostRxPush1I = 0;
  hostRxPush2I = 0;
  hostRxPop1I = 0;
  hostRxPop2I = 0;
  hostRxNum1 = 0;
  hostRxNum2 = 0;
  hostTxPush1I = 0;
  hostTxPush2I = 0;
  hostTxPop1I = 0;
  hostTxPop2I = 0;
  hostTxNum1 = 0;
  hostTxNum2 = 0;
  termOutputPushI = 0;
  termOutputPopI = 0;
  termOutputNum = 0;
  tinyBasicRxPushI = 0;
  tinyBasicRxPopI = 0;
  tinyBasicRxNum = 0;
  tinyBasicTxPushI = 0;
  tinyBasicTxPopI = 0;
  tinyBasicTxNum = 0;

  // Initialize terminal state
  termMode = SwitchIsTerminalMode() ? kModeIsTerm : kModeIsTinyBasic;
  termState = kStateIsImm;
  escState = kStateEscIdle;
  scrollTop = 0;
  scrollBot = kNumTermLines - 1;
  savedScrollTop = 0;
  savedScrollBot = kNumTermLines - 1;
  originMode = false;
  savedOriginMode = false;
  lastRxPort = 1;
  termLocalEcho = PsGetTermLocalEcho();
  termConvertCRLF = PsGetConvertCRLF();
  termInsertChars = false;
  hostFlowControlEn = PsGetHostFlowControl();
  linewrapEn = PsGetLinewrap();
  termCharIndex = 0;
  termLineIndex = 0;
  termSaveCharIndex = 0;
  termSaveLineIndex = 0;
  hostInFc1 = false;
  hostInFc2 = false;
  printerInFc = false;
  printerOn = false;
  printerPaperOut = false;
  enablePrintLog = false;
  enableAutoPrint = false;
  enablePrintRegion = false;
  enableScreenPrintTerm = false;

  // Setup the default tabs
  tabWidth = PsGetTabWidth();
  for (int i = 0; i<kNumCharPerLine-1; i++) {
    tabArray[i] = (i%tabWidth) == 0;
  }

  // Set the display cursor type
  if (PsGetCursorBlink()) {
    DL1416_CUR_BLNK();
    cursorBlink = true;
  } else {
    DL1416_CUR_SLD();
    cursorBlink = false;
  }
  savedCursorBlink = cursorBlink;
  cursorVisible = true; // Always visible to start with
  savedCursorVisible = cursorVisible;
  DL1416_EN_CUR();

  // Clear the terminal buffer
  for (int i = 0; i<kTermDispBufLen; i++) {
    termBuffer[i] = SPACE;
  }
  // And the actual display
  DL1416_CLS();

  // Try to get the printer status
  PUSH_PRT_TX(ENQ);

  dl1416PrevT = 0;  // Ready to send first character immediately

  if (termMode == kModeIsTinyBasic) {
    // Setup terminal evaluation in the background (Tiny Basic will evaluate as main process)
    termSchedulerObj.begin(TermEval,  TERM_SCHEDULE_PERIOD);
  }
}


void TermEval() {
  char c;
  boolean valid;

  // Profile indicator on
  digitalWrite(TEENSY_PROFILE_OUT, HIGH);

  // Evaluate the mode switch here since Tiny Basic may have taken over the main thread
  SwitchEval();

  // Look for Serial3 data (Kludge!: At the time of this code, the library didn't seem to implement the Serial3 event)
  SerialEvent3();

  // -----------------------------------------------------------------------------------------------
  // Process incoming data (to terminal from host or tiny basic)
  //  Process control characters
  //    BELL - pass to display
  //    Backspace - backspace our pointer if possible, update display
  //    Tab - expand, update our pointer, update display
  //    Linefeed - update our pointer, update display
  //    Vertical Tab - update our pointer, update display
  //    Formfeed - update our pointer to scroll down one page, update display
  //    Carriage return - update our pointer, update display (pass-through)
  //    ESC - start processing escape sequence
  //    DEL - delete terminal buffer character, update pointer, update display (pass-through, end of prev line, do not pass)
  //    All others - throw away
  //  Update display with all other characters
  valid = false;
  if (termMode == kModeIsTinyBasic) {
    // Look for data from tiny basic to our terminal
    if (tinyBasicRxNum > 0) {
      c = PEEK_TB_RX();
      valid = true;
    }
  } else {
    if (hostRxNum1 > 0) {
      c = PEEK_HOST_RX1();
      valid = true;
      lastRxPort = 1;
    } else if (hostRxNum2 > 0) {
      c = PEEK_HOST_RX2();
      valid = true;
      lastRxPort = 2;
    }
  }
  if (valid) {
    if (enablePrintLog) {
      // Data is going to the printer - we handle one escape sequence here - ESC[4i - to exit print logging.  All
      // other escape sequences and characters are just passed on to the printer. 
      if (PRT_TX_FULL()) {
        valid = false;
      } else {
        if (escState != kStateEscIdle) {
          // Look to see if we have seen the sequence to exit print logging
          ProcessPrinterEscSequence(c);
        }
        if (c == ESC) {
          // Restart ESC sequence detection every time we see an ESC character (vastly simplifies processing all
          // the possible sequences including ones we don't know about that the printer might use)
          escState = kStateEscEsc;
        }
        PUSH_PRT_TX(c);
      }
    } else {
      // Data is going to the terminal display, make sure there is room in the DL1416 buffer for any updates
      // we have to do (which could be more than a screen's worth of data)
      if (DL1416_BUF_FULL()) {
        valid = false;
      } else {
        if (escState != kStateEscIdle) {
          valid = ProcessEscSequence(c);
        } else if (IsControlChar(c)) {
          switch (c) {
            case BELL:
              DisplayCharacter(c);
              break;
            case BS:
              DisplayBackspace(false);
              break;
            case TAB:
              DisplayTab();
              break;
            case LF:
              if (termConvertCRLF) {
                DisplayCarriageRet();
              }
              valid = DisplayLinefeed(true);
              break;
            case VT:
              valid = DisplayVerticalTab(false, true);
              break;
            case FF:
              valid = DisplayScreenClear(true);
              break;
            case CR:
              DisplayCarriageRet();
              break;
            case ESC:
              escState = kStateEscEsc;
              break;
            case DEL:
              DisplayBackspace(true);
          }
        } else {
          DisplayCharacter(c);
        }
      }
    }
  }
  // Pop if we successfully processed the character (because valid is still set here)
  if (valid) {
    if (termMode == kModeIsTinyBasic) {
      POP_TB_RX();
    } else {
      if (lastRxPort == 1) {
        POP_HOST_RX1();
#ifdef ECHO_USB_INCOMING
        PUSH_HOST_TX2(c);
#endif
      } else {
        POP_HOST_RX2();
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Look for data to send to Tiny Basic from any selected host source (otherwise it is silently tossed)
  if (termMode == kModeIsTinyBasic) {
    if (hostRxNum1 > 0) {
      c = PEEK_HOST_RX1();
      if ((GetInputMask() & TB_INPUT_MASK_USB_HOST) == TB_INPUT_MASK_USB_HOST) {
        if (!TB_TX_FULL()) {
          // Send to tiny basic
          PUSH_TB_TX(c);
          POP_HOST_RX1();
        }
      } else {
        // Silently toss
        POP_HOST_RX1();
      }
    }
    
    if (hostRxNum2 > 0) {
      c = PEEK_HOST_RX2();
      if ((GetInputMask() & TB_INPUT_MASK_SER_HOST) == TB_INPUT_MASK_SER_HOST) {
        if (!TB_TX_FULL()) {
          // Send to tiny basic
          PUSH_TB_TX(c);
          POP_HOST_RX2();
        }
      } else {
        // Silently toss
        POP_HOST_RX2();
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Flow control for host RX buffers
  if (hostFlowControlEn) {
    if (!hostInFc1 && HOST_REQ_FC_RX1()) {
      // Start flow control if possible
      if (!HOST_TX1_FULL()) {
        hostInFc1 = true;
        PUSH_HOST_TX1(XOFF);
      }
    } else if (hostInFc1 && !HOST_REQ_FC_RX1()) {
      // End flow control if possible
      if (!HOST_TX1_FULL()) {
        hostInFc1 = false;
        PUSH_HOST_TX1(XON);
      }
    }

    if (!hostInFc2 && HOST_REQ_FC_RX2()) {
      // Start flow control if possible
      if (!HOST_TX2_FULL()) {
        hostInFc2 = true;
        PUSH_HOST_TX2(XOFF);
      }
    } else if (hostInFc2 && !HOST_REQ_FC_RX2()) {
      // End flow control if possible
      if (!HOST_TX2_FULL()) {
        hostInFc2 = false;
        PUSH_HOST_TX2(XON);
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Process outgoing data (from terminal to host or tiny basic)
  if (termOutputNum > 0) {
    c = PEEK_TERM_OUT();
    if (termMode == kModeIsTinyBasic) {
      if (!TB_TX_FULL()) {
        // Send to tiny basic
        PUSH_TB_TX(c);
        POP_TERM_OUT();
      }
    } else {
      if (!(HOST_TX1_FULL() || HOST_TX2_FULL())) {
        PUSH_HOST_TX1(c);
        PUSH_HOST_TX2(c);
        POP_TERM_OUT();
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // Process data from keyboard
  //  Special case handling for Terminal Mode
  //    SO/SI - On-screen Menu control (SO = Enable Menu, SI = Disable Menu)
  //    FF/VT - Page Down/Up
  //    EOT - Convert to ESC
  //  Special case handling for Tiny Basic
  //    EOT - Convert to Break (CTRLC)
  //
  if (Serial2.available()) {
    valid = true;  // Assume there will be room
    if (termMode == kModeIsTinyBasic) {
      if (TB_TX_FULL()) {
        valid = false;
      }
    } else {
      if (HOST_TX1_FULL() || HOST_TX2_FULL()) {
        valid = false;
      }
    }
    if (valid) {
      c = Serial2.read();
      // Look for special cases
      if (c == EOT) {
        if (termMode == kModeIsTinyBasic) {
          c = CTRLC;
        } else {
          c = ESC;
        }
      } else if (c == SO) {
        if (termMode == kModeIsTerm) {
          // Bring up the menu when we're in terminal mode
          termMode = kModeIsConfig;
          MenuInit();
        }
      }

      // Push into appropriate buffers
      if (termMode == kModeIsTinyBasic) {
        // Send to tiny basic if enabled
        if ((GetInputMask() & TB_INPUT_MASK_KEYBOARD) == TB_INPUT_MASK_KEYBOARD) {
          PUSH_TB_TX(c);
        }
      } else {
        // Send to host
        PUSH_HOST_TX1(c);
        PUSH_HOST_TX2(c);

        // Local echo if requested using whatever host port was most recently active
        if (termLocalEcho) {
          if (lastRxPort == 1) {
            PUSH_HOST_RX1(c);
          } else {
            PUSH_HOST_RX2(c);
          }
        }

        // Handle CRLF conversion for CR characters if enabled
        if (termConvertCRLF && (c == CR)) {
          // Also send a LF
          PUSH_HOST_TX1(LF);
          PUSH_HOST_TX2(LF);

          // Local echo if requested
          if (termLocalEcho) {
            if (lastRxPort == 1) {
              PUSH_HOST_RX1(LF);
            } else {
              PUSH_HOST_RX2(LF);
            }
          }
        }
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // See if there is data to send to the DL1416 displays and if it's time
  if (dl1416Num != 0) {
    if (termMode == kModeIsTinyBasic) {
      // Just send the character immediately since we are being timesliced already
        c = PEEK_DL1416();
        POP_DL1416();
        Serial2.write(c);
    } else {
      // Flow control when we're the main thread
      if (InactivityTimeout(dl1416PrevT, kDL1416delay)) {
        c = PEEK_DL1416();
        POP_DL1416();
        Serial2.write(c);
        dl1416PrevT = micros();
      }
    }
  }

  // -----------------------------------------------------------------------------------------------
  // See if there is data to send back to the host
  if (hostTxNum1 != 0) {
    c = PEEK_HOST_TX1();
    POP_HOST_TX1();
    if (Serial.dtr()) {
      // USB Host IF connected
      Serial.write(c);
    }
  }
  if (hostTxNum2 != 0) {
    c = PEEK_HOST_TX2();
    POP_HOST_TX2();
    Serial1.write(c);
  }

  // -----------------------------------------------------------------------------------------------
  // See if the printer has sent flow control or status information or Tiny Basic is requesting 
  // its output be forwarded
  if (printerRxNum > 0) {
    valid = true;  // Assume there will be room
    
    c = PEEK_PRT_RX();

    // Forward to Tiny Basic if requested and able
    if ((termMode == kModeIsTinyBasic) &&
        ((GetInputMask() & TB_INPUT_MASK_PRINTER) == TB_INPUT_MASK_PRINTER)) {
    
      if (TB_TX_FULL()) {
        valid = false;
      } else {
        PUSH_TB_TX(c);
      }
    }

    switch (c) {
      case XON:
        printerInFc = false;
        break;
      case XOFF:
        printerInFc = true;
        break;
      case ACK:
        printerOn = true;
        break;
      case NACK:
        printerOn = false;
        break;
      case DC2:
        printerPaperOut = false;
        break;
      case DC4:
        printerPaperOut = true;
        break;
    }

    if (valid) {
      POP_PRT_RX();
    }
  }

  
  // -----------------------------------------------------------------------------------------------
  // See if there is data to send to the printer
  if (!printerInFc && (printerTxNum > 0)) {
    c = PEEK_PRT_TX();
    POP_PRT_TX();
    Serial3.write(c);
  }

  // Profile indicator off
  digitalWrite(TEENSY_PROFILE_OUT, LOW);
}


void TinyBasicInit() {
  // Configure for Tiny Basic operation, overwriting some terminal configuration values
  termLocalEcho = true; // ??? Not necessary
  termConvertCRLF = false;
  linewrapEn = true;
  tinyBasicRxPushI = 0;
  tinyBasicRxPopI = 0;
  tinyBasicRxNum = 0;
  tinyBasicTxPushI = 0;
  tinyBasicTxPopI = 0;
  tinyBasicTxNum = 0;
  tb_setup();
}


void TinyBasicEval() {
  tb_loop();
}


void DisplayCharacter(char c) {
  if (IsControlChar(c)) {
    // Pass control characters to display
    PUSH_DL1416(c);
  } else {
    if ((termCharIndex < kNumCharPerLine) && (termLineIndex < kNumTermLines)) {
      if (termInsertChars) {
        // Disable cursor
        DL1416_DIS_CUR();
        
        // Load new character at displayed position so it's cursor is advanced to the next location
        PUSH_DL1416(c);

        // Load remaining displayable characters onto the display
        for (int i = termCharIndex; i < (kNumCharPerLine-1); i++) {
          PUSH_DL1416(termBuffer[ComputeTermBuffIndex(termLineIndex, i)]);
        }
          
        // Push characters from the current position over one position to the right in the terminal buffer
        for (int i = kNumCharPerLine-1; i > termCharIndex; i--) {
          termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = termBuffer[ComputeTermBuffIndex(termLineIndex, i - 1)];
        }

        // Update the terminal buffer
        termBuffer[TERM_BUFF_INDEX()] = c;

        // Bump position on line
        termCharIndex++;
        
        // Reset the cursor
        UpdateCursor();
      } else {
        // Not inserting: just overwrite character at current position
        PUSH_DL1416(c);
        termBuffer[TERM_BUFF_INDEX()] = c;

        // Bump position on line
        termCharIndex++;
      }
    }

    // Now if line wrap is enabled and we just went off the right end of the visible
    // line then set the cursor to the next line (and scroll the entire display if necessary)
    if (linewrapEn && (termCharIndex >= kNumCharPerLine)) {
      if (termLineIndex < scrollBot) {
        // Just move cursor to the next line
        TermNextLine();
      } else if (termLineIndex == scrollBot) {
        // Scroll down one line
        TermScrollLine(true);
      } else if (termLineIndex < kNumTermLines) {
        // Below scrollable region, but above display bottom so move cursor to next line
        TermNextLine();
      } else {
        // At bottom of display, below scrollable region so just reset to start of line
        termCharIndex = 0;
        UpdateCursor();
      }
    }    
  }
}


void DisplayBackspace(boolean destructive) {
  // Backup one character position if possible
  if (termCharIndex != 0) {
    --termCharIndex;

    // Display the backspace if visible
    if (termCharIndex < kNumCharPerLine) {
      if (destructive) {
        // Delete the character if requested to do so
        termBuffer[TERM_BUFF_INDEX()] = SPACE;
        PUSH_DL1416(DEL);
      } else {
        PUSH_DL1416(BS);
      }
    }
  }
}


void DisplayTab() {
  int i;

  // Find the next tab location if visible
  if (termCharIndex < kNumCharPerLine) {
    for (i=termCharIndex+1; i<kNumCharPerLine; i++) {
      if (tabArray[i] == true) break;
    }
    if (i >= kNumCharPerLine) i = kNumCharPerLine - 1;
    termCharIndex = i;
    UpdateCursor();
  }
}


boolean DisplayLinefeed(boolean allowAutoPrint) {
  if (allowAutoPrint && enableAutoPrint) {
    // See if we can print the current line - otherwise don't process the character yet
    if (!PrintLine(termLineIndex, LF)) {
      return(false);
    }
  }
  
  if (termLineIndex >= scrollBot) {
    if (termLineIndex == scrollBot) {
      // Scroll screen when we're at the bottom of the scrollable region (otherwise don't scroll)
      TermScrollLine(false);
    }
  } else {
    // Just move the cursor to the next line even if it is out of our display area
    // (don't scroll anything)
    termLineIndex++;
    UpdateCursor();
  }

  return(true);
}


void DisplayCarriageRet() {
  // Set cursor to start of line
  termCharIndex = 0;

  // Update cursor on screen
  UpdateCursor();
}


boolean DisplayVerticalTab(boolean enScrollUp, boolean allowAutoPrint) {
  if (allowAutoPrint && enableAutoPrint) {
    // See if we can print the current line - otherwise don't process the character yet
    if (!PrintLine(termLineIndex, LF)) {
      return(false);
    }
  }
  
  if (termLineIndex < scrollTop) {
    // Above scrollable region - only scroll up to the top of the display
    if (termLineIndex > 0) {
      // Set cursor to previous line
      --termLineIndex;
    
      // Update cursor on screen
      UpdateCursor();
    }
  } else if (termLineIndex <= scrollBot) {
    // Inside scrollable region
    if (termLineIndex == scrollTop) {
      // At top of scrollable region - Scroll up region if requested, otherwise leave cursor where it is
      if (enScrollUp) {
        TermScrollUpLine(false);
      }
    } else {
      // Inside scrollable region so just move cursor up one line
      --termLineIndex;
    
      // Update cursor on screen
      UpdateCursor();
    }
  } else {
    // Below scrollable region: just move cursor up one line
    --termLineIndex;
    
    // Update cursor on screen
    UpdateCursor();
  }

  return(true);
}


boolean DisplayScreenClear(boolean allowAutoPrint) {
  int i;

  if (allowAutoPrint && enableAutoPrint) {
    // See if we can print the current line - otherwise don't process the character yet
    if (!PrintLine(termLineIndex, FF)) {
      return(false);
    }
  }

  // Clear terminal buffer
  for (i=0; i<kTermDispBufLen; i++) {
    termBuffer[i] = SPACE;
  }

  // Send a FF to clear the display
  PUSH_DL1416(FF);

  // Reset our pointers and update the cursor
  termLineIndex = 0;
  termCharIndex = 0;
  UpdateCursor();

  return(true);
}


// Set position to start of next line in case where we don't have to scroll
void TermNextLine() {
  // Increment the line pointer to the next line
  ++termLineIndex;

  // First character position in this line
  termCharIndex = 0;

  // Update the display's cursor
  UpdateCursor();
}


// Scroll down one line when we are at the end of the scrollable region
void TermScrollLine(boolean incCarriageReturn) {
  int i;
  
  // Ensure last line
  termLineIndex = scrollBot;

  // First character position in this line
  if (incCarriageReturn) {
    termCharIndex = 0;
  }

  // Move data up one line
  for (i=scrollTop*kNumCharPerLine; i<scrollBot*kNumCharPerLine; i++) {
    termBuffer[i] = termBuffer[i+kNumCharPerLine];
  }
  
  // Clear the last line
  for (i=0; i<kNumCharPerLine; i++) {
    termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = SPACE;
  }

  // Repaint display
  RedrawDisplay();
  UpdateCursor();
}


// Scroll up one line when we are the top of the scrollable region
void TermScrollUpLine(boolean incCarriageReturn) {
  int i;

  // Ensure first line
  termLineIndex = scrollTop;

  // First character position in this line
  if (incCarriageReturn) {
    termCharIndex = 0;
  }

  // Move data down one line
  for (i = ComputeTermBuffIndex(scrollBot, kNumCharPerLine-1); i >= ComputeTermBuffIndex(termLineIndex, 0); i--) {
    termBuffer[i] = termBuffer[i-kNumCharPerLine];
  }

  // Clear the first line
  for (i=0; i<kNumCharPerLine; i++) {
    termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = SPACE;
  }

  // Repaint display
  RedrawDisplay();
  UpdateCursor();
}


// Insert line(s) starting below current line - lines below bottom margin are lost
void InsertLines(int numLines) {
  // Only insert as many lines as will fit in the space between the cursor and bottom margin
  if (numLines > (scrollBot - termLineIndex + 1)) {
    numLines = scrollBot - termLineIndex + 1;
  } else if (numLines == 0) {
    numLines = 1;
  } else if (numLines < 0) {
    return;
  }

  // Copy current lines down numLines
  for (int i = scrollBot; i >= termLineIndex+numLines; i--) {
    for (int j=0; j<kNumCharPerLine; j++) {
      termBuffer[ComputeTermBuffIndex(i, j)] = termBuffer[ComputeTermBuffIndex(i-numLines, j)];
    }
  }

  // Create blank lines
  for (int i = termLineIndex; i<termLineIndex+numLines; i++) {
    for (int j=0; j<kNumCharPerLine; j++) {
      termBuffer[ComputeTermBuffIndex(i, j)] = SPACE;
    }
  }

  // Repaint display
  RedrawDisplay();
  UpdateCursor();
}


// Delete line(s) starting at line with cursor, moving lines up from bottom of scrollable region and
// inserting blank lines at bottom of region
void DeleteLines(int numLines) {
  if (numLines == 0) numLines = 1;
  
  // See if there is data to move up
  if (numLines <= (scrollBot - termLineIndex)) {
    // Copy data below delete region up
    for (int i = termLineIndex+numLines; i <= scrollBot; i++) {
      for (int j=0; j<kNumCharPerLine; j++) {
        termBuffer[ComputeTermBuffIndex(i-numLines, j)] = termBuffer[ComputeTermBuffIndex(i, j)];
      }
    }
  }
  
  // Create blank lines at the bottom
  if (numLines > (scrollBot - termLineIndex + 1)) {
    // Clip to area in scrollable region
    numLines = scrollBot - termLineIndex + 1;
  }
  for (int i = scrollBot-numLines+1; i <= scrollBot; i++) {
    for (int j=0; j<kNumCharPerLine; j++) {
       termBuffer[ComputeTermBuffIndex(i, j)] = SPACE;
     }
  }
  
  // Repaint display
  RedrawDisplay();
  UpdateCursor();
}


// Delete characters starting with the current character
void DeleteCharacters(int numChars) {
  int curIndex = ComputeTermBuffIndex(termLineIndex, termCharIndex);
  
  if (numChars == 0) numChars = 1;
  
  // See if we have to move some characters left
  if ((termCharIndex + numChars) < kNumCharPerLine) {
    // Move characters past deleted section to the current cursor position
    for (int i = termCharIndex + numChars; i < kNumCharPerLine; i++) {
      termBuffer[curIndex] = termBuffer[curIndex+numChars];
      curIndex++;
    }
  }

  // Erase characters past the moved set
  while (curIndex < kNumCharPerLine) {
    termBuffer[ComputeTermBuffIndex(termLineIndex, curIndex)] = SPACE;
    curIndex++;
  }
  
  // Repaint display
  RedrawDisplay();
  UpdateCursor();
}


// Set cursor position on display to match state variables
void UpdateCursor() {
  if ((termLineIndex < kNumTermLines) && (termCharIndex < kNumCharPerLine)) {
    // If onscreen, update cursor position
    if (cursorVisible) {
      DL1416_EN_CUR();
    }
    SetCursorPosition(termLineIndex, termCharIndex);
  } else {
    // otherwise blank cursor
    DL1416_DIS_CUR();
  }
}


// Load the escape sequence to set a specific cursor position, row and col must be valid (0-based)
void SetCursorPosition(int row, int col) {
  // Initiate Display escape sequence to set absolute cursor position
  PUSH_DL1416(ESC);
  
  // Column
  if (col > 9) {
    if (col > 99) col = 99;
    // Tens digit
    PUSH_DL1416('0' + (col/10));
    // Units digit
    PUSH_DL1416('0' + (col%10));
  } else {
    // Units digit only
    PUSH_DL1416('0' + col);
  }

  // Column/Row separator
  PUSH_DL1416(';');

  // Row
  if (row > 9) {
    if (row > 99) row = 99;
    // Tens digit
    PUSH_DL1416('0' + (row/10));
    // Units digit
    PUSH_DL1416('0' + (row%10));
  } else {
    // Units digit only
    PUSH_DL1416('0' + row);
  }

  // Terminator
  PUSH_DL1416('H');
}


// Redraw the contents of the display using the shadow memory to hide the (re)loading of data
void RedrawDisplay() {
  int row, col;
  int firstValidPos, lastValidPos;
  char c;

  // Switch to shadow memory
  PUSH_DL1416(SO);

  // Clear shadow memory (sets [invisible] shadow cursor to start of display)
  PUSH_DL1416(FF);

  // Update shadow memory
  for (row = 0; row <= (kNumTermLines - 1); row++) {
    // Find the first valid (non-SPACE) character position
    for (firstValidPos=0; firstValidPos<kNumCharPerLine; firstValidPos++) {
      if (termBuffer[ComputeTermBuffIndex(row, firstValidPos)] != SPACE) {
        break;
      }
    }
    if (firstValidPos < kNumCharPerLine) {
      // At least one character on this line
      SetCursorPosition(row, firstValidPos);

      // Look to see if there are spaces at the end of the line we can skip sending
      for (lastValidPos = kNumCharPerLine-1; lastValidPos >= firstValidPos; lastValidPos--) {
        if (termBuffer[ComputeTermBuffIndex(row, lastValidPos)] != SPACE) {
          break;
        }
      }
      for (col = firstValidPos; col <= lastValidPos; col++) {
        c = termBuffer[ComputeTermBuffIndex(row, col)];
        PUSH_DL1416(c);
      }
    }
  }

  // Switch back to display memory
  PUSH_DL1416(SI);

  // Clear visible display
  PUSH_DL1416(FF);

  // Copy shadow memory to display
  PUSH_DL1416(SUB);
}


// Returns false if execution blocked because a buffer was full
boolean ProcessEscSequence(char c) {
  boolean retVal = true;  // Set false if blocked
  
  switch (escState) {
    case kStateEscEsc:  // Saw Escape character, looking for type of sequence
      switch (c) {
        case '[':       // Saw CSI: Building escape sequence
          escState = kStateEscBracket;
          for (int i=0; i < MAX_ESC_ARGS; i++) {
            escArgs[i] = 0;
          }
          curEscArgIndex = 0;
          break;
          
        case ']':      // Saw OSC: Ignore
          escState = kStateEscIgnoreOSC;
          break;
          
        case 'D':      // Scroll Down One Line
          (void) DisplayLinefeed(false);
          escState = kStateEscIdle;
          break;

        case 'E':      // Next Line
          (void) DisplayLinefeed(false);
          DisplayCarriageRet();
          escState = kStateEscIdle;
          break;
          
        case 'H':      // Set tab
          if (termCharIndex < kNumCharPerLine) {
            tabArray[termCharIndex] = true;
          }
          escState = kStateEscIdle;
          break;
          
        case 'M':      // Scroll Up One Line
          (void) DisplayVerticalTab(true, false);
          escState = kStateEscIdle;
          break;

        case 'Z':      // Query Device Attributes
          ReportDeviceAttributes();
          escState = kStateEscIdle;
          break;
         
        case 'c':   // Reset Device - NOTE: This code must be updated when new state is added to the code
          termCharIndex = 0;
          termLineIndex = 0;
          termSaveCharIndex = 0;
          termSaveLineIndex = 0;
          scrollTop = 0;
          scrollBot = kNumTermLines - 1;
          savedScrollTop = 0;
          savedScrollBot = kNumTermLines - 1;
          originMode = false;
          savedOriginMode = false;
          linewrapEn = PsGetLinewrap();
          termLocalEcho = PsGetTermLocalEcho();
          termConvertCRLF = PsGetConvertCRLF();
          termInsertChars = false;
          enablePrintLog = false;
          enableAutoPrint = false;
          enablePrintRegion = false;
          enableScreenPrintTerm = false;
          if (PsGetCursorBlink()) {
            DL1416_CUR_BLNK();
            cursorBlink = true;
          } else {
            DL1416_CUR_SLD();
            cursorBlink = false;
          }
          savedCursorBlink = cursorBlink;
          cursorVisible = true;
          savedCursorVisible = true;
          DL1416_EN_CUR();
          DL1416_CLS();
          tabWidth = PsGetTabWidth();
          for (int i = 0; i<kNumCharPerLine-1; i++) {
            tabArray[i] = (i%tabWidth) == 0;
          }
          escState = kStateEscIdle;
          break;
            
        case '7':      // Save Cursor and Attributes
          termSaveCharIndex = termCharIndex;
          termSaveLineIndex = termLineIndex;
          savedCursorVisible = cursorVisible;
          savedCursorBlink = cursorBlink;
          savedScrollTop = scrollTop;
          savedScrollBot = scrollBot;
          savedOriginMode = originMode;
          escState = kStateEscIdle;
          break;
          
        case '8':      // Restore Cursor and Attributes
          termCharIndex = termSaveCharIndex;
          termLineIndex = termSaveLineIndex;
          originMode = savedOriginMode;
          scrollTop = savedScrollTop;
          scrollBot = savedScrollBot;
          cursorVisible = savedCursorVisible;
          if (cursorVisible) {
            DL1416_EN_CUR();
          } else {
            DL1416_DIS_CUR();
          }
          cursorBlink = savedCursorBlink;
          if (cursorBlink) {
            DL1416_CUR_BLNK();
          } else {
            DL1416_CUR_SLD();
          }
          UpdateCursor();
          escState = kStateEscIdle;
          break;

        case '#':      // Line Attributes (IGNORED)
        case '(':      // Select Character Set (IGNORED)
        case ')':      // Select Character Set (IGNORED)
          escState = kStateEscIgnore;
          break;
          
        default:
          // Unknown escape sequence
          escState = kStateEscIdle;
      }
      break;
      
    case kStateEscQues:  // Processing ESC[?
      if (IsNumericChar(c)) {
        escArgs[curEscArgIndex] = (escArgs[curEscArgIndex]*10) + (c - '0');
      } else switch (c) {
        case 'h':   // Set Mode
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 6:    // Set Origin Mode
                originMode = true;
                // Set cursor to margin home
                termCharIndex = 0;
                termLineIndex = scrollTop;
                UpdateCursor();
                break;

              case 18:   // Set FF as print screen terminator
                enableScreenPrintTerm = true;
                break;
                
              case 19:   // Set full screen to print during print screen
                enablePrintRegion = false;
                break;
                
              case 25:   // Cursor visible
                cursorVisible = true;
                DL1416_EN_CUR();
                break;
            }
          }
          escState = kStateEscIdle;
          break;
          
        case 'l':   // Reset Mode
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 6:    // Reset Origin Mode
                originMode = false;
                // Set cursor to display home
                termCharIndex = 0;
                termLineIndex = 0;
                UpdateCursor();
                break;

              case 18:   // Set nothing as print screen terminator
                enableScreenPrintTerm = false;
                break;
                
              case 19:   // Set scrolling region to print during print screen
                enablePrintRegion = true;
                break;
                
              case 25:   // Cursor Invisible
                cursorVisible = false;
                DL1416_DIS_CUR();
                break;
            }
          }
          escState = kStateEscIdle;
          break;
          
        case 'i':   // Media Copy
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 1:    // Print Line with cursor
                retVal = PrintLine(termLineIndex, LF);
                break;

              case 4:    // Turn off Auto Print
                enableAutoPrint = false;
                break;

              case 5:    // Turn on Auto Print
                enableAutoPrint = true;
                break;
            }
          }
          if (retVal) {
            escState = kStateEscIdle;
          }
          break;
  
        case 'n':   // Reports
          // Only attempt to create a report if all previous reports in this escape sequence have succeeded
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 15:
                // Create and push a Printer Status into the host TX Buffer(s)
                if (retVal) {
                  retVal = ReportPrinterStatus();
                }
                break;
            }
          }
          if (retVal) {
            escState = kStateEscIdle;
          }
          break;
        default:
          // Unknown escape sequence
          escState = kStateEscIdle;
      }
      break;
      
    case kStateEscBracket: // Processing primary CSI types
      if (IsNumericChar(c)) {
        escArgs[curEscArgIndex] = (escArgs[curEscArgIndex]*10) + (c - '0');
      } else switch(c) {
        case 'A':   // Cursor up
          if (escArgs[0] == 0) escArgs[0] = 1;
          for (int i = 0; i<escArgs[0]; i++) {
            if (termLineIndex <= scrollTop) {
              // Don't scroll beyond margin
              break;
            }
            (void) DisplayVerticalTab(false, false);
          }
          escState = kStateEscIdle;
          break;
          
        case 'B':   // Cursor down
          if (escArgs[0] == 0) escArgs[0] = 1;
          for (int i = 0; i<escArgs[0]; i++) {
            if (termLineIndex >= scrollBot) {
              // Don't scroll beyond margin
              break;
            }
            (void) DisplayLinefeed(false);
          }
          escState = kStateEscIdle;
          break;
          
        case 'C':   // Cursor forward
          if (escArgs[0] == 0) escArgs[0] = 1;
          termCharIndex = termCharIndex + escArgs[0];
          if (termCharIndex >= kNumCharPerLine) termCharIndex = kNumCharPerLine-1;
          UpdateCursor();
          escState = kStateEscIdle;
          break;
          
        case 'D':   // Cursor backward
          if (escArgs[0] == 0) escArgs[0] = 1;
          termCharIndex = termCharIndex - escArgs[0];
          if (termCharIndex < 0) termCharIndex = 0;
          UpdateCursor();
          escState = kStateEscIdle;
          break;
          
        case 'J':   // Erase sequences
          // Turn off the cursor while we erase
          DL1416_DIS_CUR();
          
          switch (escArgs[0]) {
            case 0:  // Erase Down (from current cursor position)
              {
                int startPos, endPos;

                endPos = ComputeTermBuffIndex(kNumTermLines-1, kNumCharPerLine-1);
                if (termCharIndex < kNumCharPerLine) {
                  startPos = ComputeTermBuffIndex(termLineIndex, termCharIndex);
                } else {
                  startPos = ComputeTermBuffIndex(termLineIndex+1, 0);
                }

                for (int i = startPos; i <= endPos; i++) {
                  termBuffer[i] = SPACE;
                }

                // Update display
                RedrawDisplay();
                UpdateCursor();
              }
              break;
            case 1:  // Erase Up (from current cursor position)
              {
                int endPos;

                if (termLineIndex >= kNumTermLines) {
                  endPos = ComputeTermBuffIndex(kNumTermLines-1, kNumCharPerLine-1);
                } else {
                  if (termCharIndex < kNumCharPerLine) {
                    endPos = ComputeTermBuffIndex(termLineIndex, termCharIndex);
                  } else {
                    endPos = ComputeTermBuffIndex(termLineIndex, kNumCharPerLine-1);
                  }
                }

                for (int i = 0; i <= endPos; i++) {
                  termBuffer[i] = SPACE;
                }

                // Update display
                RedrawDisplay();
                UpdateCursor();
              }
              break;
            case 2:  // Erase Screen
              (void) DisplayScreenClear(false);
              break;
          }

          if (cursorVisible) {
            DL1416_EN_CUR();
          }
          escState = kStateEscIdle;
          break;

        case 'f':
        case 'H':   // Set Cursor position (1-based arguments)
          // Adjust 1-based ESC sequence to our 0-based addressing
          if (escArgs[0] > 0) escArgs[0] -= 1;
          if (escArgs[1] > 0) escArgs[1] -= 1;

          if (originMode) {
            // Home starts at top of scrolling range
            escArgs[0] += scrollTop;
          }

          // Clip to on-screen positions
          if (escArgs[0] > (kNumTermLines-1)) escArgs[0] = kNumTermLines-1;
          if (escArgs[1] > (kNumCharPerLine-1)) escArgs[1] = kNumCharPerLine-1;

          // Update the position
          termLineIndex = escArgs[0];
          termCharIndex = escArgs[1];
          UpdateCursor();
          escState = kStateEscIdle;
          break;
           
        case 'K':   // Erase Line sequences
          // Turn off the cursor while we erase the line
          DL1416_DIS_CUR();

          switch (escArgs[0]) {
            case 0:   // Erase End of Line
              for (int i = termCharIndex; i<kNumCharPerLine; i++) {
                termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = SPACE;
                PUSH_DL1416(SPACE);
              }
              break;
            case 1:   // Erase Start of Line
              SetCursorPosition(termLineIndex, 0);
              for (int i = 0; i<=termCharIndex; i++) {
                if (i < kNumCharPerLine) {
                  termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = SPACE;
                  PUSH_DL1416(SPACE);
                }
              }
              break;
            case 2:   // Erase Entire Line
              SetCursorPosition(termLineIndex, 0);
              for (int i = 0; i<=kNumCharPerLine; i++) {
                termBuffer[ComputeTermBuffIndex(termLineIndex, i)] = SPACE;
                PUSH_DL1416(SPACE);
              }
              break;
          }

          // Reset the display cursor position
          UpdateCursor();
          escState = kStateEscIdle;
          break;

        case 'L':   // Insert Line(s)
          if ((termLineIndex >= scrollTop) && (termLineIndex <= scrollBot)) { 
            // Only insert if we are in the scrollable region
            InsertLines(escArgs[0]);
          }
          escState = kStateEscIdle;
          break;

        case 'M':   // Delete Line(s)
          if ((termLineIndex >= scrollTop) && (termLineIndex <= scrollBot)) { 
            // Only delete if we are in the scrollable region
            DeleteLines(escArgs[0]);
          }
          escState = kStateEscIdle;
          break;
          
        case 'P':   // Delete Character
          DeleteCharacters(escArgs[0]);
          escState = kStateEscIdle;
          break;

        case 'c':      // Query Device Attributes
          if (escArgs[0] == 0) {
            ReportDeviceAttributes();
          }
          escState = kStateEscIdle;
          break;

        case 'g':   // Clear Tab
          if (escArgs[0] == 0) {
            // Clear tab at current position
            if (termCharIndex < kNumCharPerLine) {
              tabArray[termCharIndex] = false;
            }
          } else if (escArgs[0] == 3) {
            // Clear all horizontal tabs
            for (int i=0; i<kNumCharPerLine; i++) {
              tabArray[i] = false;
            }
          }
          escState = kStateEscIdle;
          break;
          
        case 'h':   // Set Modes
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 4: // Enable Character Insertion Mode
                termInsertChars = true;
                break;
                
              case 7: // Enable Line Wrap
                linewrapEn = true;
                break;

              case 12: // Disable Local Echo
                termLocalEcho = false;
                break;

              case 20: // Enable New Line Mode
                termConvertCRLF = true;
                break;
            }
          }
          escState = kStateEscIdle;
          break;
          
        case 'l':   // Reset Modes
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 4: // Disable Character Insertion Mode
                termInsertChars = false;
                break;
                
              case 7: // Disable Line Wrap
                linewrapEn = false;
                break;

              case 12: // Enable Local Echo
                termLocalEcho = true;
                break;

              case 20: // Disable New Line Mode
                termConvertCRLF = false;
                break;
            }
          }
          escState = kStateEscIdle;
          break;
          
        case 'i':   // Media Copy
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 0:  // Print Screen or scrolling region (based on Printer Extend Mode)
                retVal = PrintScreen();
                break;
                
              case 4:  // Disable print log
                enablePrintLog = false;
                break;
                
              case 5:  // Enable print log
                enablePrintLog = true;
                break;
            }
          }
          if (retVal) {
            escState = kStateEscIdle;
          }
          break;
          
        case 'n':   // Reports
          // Only attempt to create a report if all previous reports in this escape sequence have succeeded
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 5:
                // Create and push a Report Device Status into the host TX Buffer(s)
                if (retVal) {
                  retVal = ReportDeviceStatus();
                }
                break;
                
              case 6: 
                // Create and push a Report Cursor Position escape sequence into the host TX Buffer(s)
                if (retVal) {
                  retVal = ReportCursorPosition();
                }
                break;
            }
          }
          if (retVal) {
            escState = kStateEscIdle;
          }
          break;

        case 'r':   // Set/Reset Scrolling region (1-based arguments)
          if (curEscArgIndex == 0) {
            // Reset scrolling region
            scrollTop = 0;
            scrollBot = kNumTermLines - 1;
            termLineIndex = 0;
          } else {
            // Ensure arguments are within our displayable region
            if (escArgs[1] <= escArgs[0]) escArgs[1] = escArgs[0] + 2;
            if (escArgs[0] > kNumTermLines) escArgs[0] = kNumTermLines;
            if (escArgs[1] > kNumTermLines) escArgs[1] = kNumTermLines;
          
            scrollTop = escArgs[0] - 1;
            scrollBot = escArgs[1] - 1;

            // Set cursor to start of region
            if (originMode) {
              termLineIndex = scrollTop;
            } else {
              termLineIndex = 0;
            }
          }
          
          termCharIndex = 0;
          UpdateCursor();
          escState = kStateEscIdle;
          break;
          

        case 's':   // Save Cursor Position
          termSaveCharIndex = termCharIndex;
          termSaveLineIndex = termLineIndex;
          escState = kStateEscIdle;
          break;

        case 'u':   // Restore Cursor Position
          termCharIndex = termSaveCharIndex;
          termLineIndex = termSaveLineIndex;
          UpdateCursor();
          escState = kStateEscIdle;
          break;
          
        case ';':   // Switch to looking for next argument (or just keep updating
                    // last argument)
          if (curEscArgIndex < (MAX_ESC_ARGS-1)) {
            curEscArgIndex++;
          } else {
            escArgs[curEscArgIndex] = 0;
          }
          break;

        case '?':   // Look for ESC[? sequence
          if ((escArgs[0] == 0) && (curEscArgIndex == 0)) {
            escState = kStateEscQues;
          } else {
            // Unknown
            escState = kStateEscIdle;
          }
          break;

        case SPACE:  // Look for ESC[<value>SPACE sequence
          escState = kStateEscSpace;
          break;
          
        default:
          // Unknown escape sequence
          escState = kStateEscIdle;
      }
      break;

    case kStateEscSpace:    // Processing ESC[<value>SPACE sequences
      switch (c) {
        case 'q':
          switch (escArgs[0]) {
            case 0:   // Cursor Blink
            case 1:
              DL1416_CUR_BLNK();
              cursorBlink = true;
              break;
            
            case 2:   // Cursor Solid
              DL1416_CUR_SLD();
              cursorBlink = false;
              break;
          }
          escState = kStateEscIdle;
          break;
      
        default:
          // Unknown escape sequence
          escState = kStateEscIdle;
    }
    
    case kStateEscIgnore:   // Ignoring a set of sequences that have a single-character specifier
      escState = kStateEscIdle;
      break;
      
    case kStateEscIgnoreOSC:  // Ignoring Operating System Command (7-bit version)
      if (c == BELL) {
        escState = kStateEscIdle;
      }
      break;
      
    default:
      escState = kStateEscIdle;
  }

  return(retVal);
}


void ProcessPrinterEscSequence(char c) {
  switch (escState) {
    case kStateEscEsc:  // Saw Escape character, looking for type of sequence
      switch (c) {
        case '[':       // Saw CSI: Building escape sequence
          escState = kStateEscBracket;
          for (int i=0; i < MAX_ESC_ARGS; i++) {
            escArgs[i] = 0;
          }
          curEscArgIndex = 0;
          break;
    
        default:
          escState = kStateEscIdle;
      }
      break;

    case kStateEscBracket: // Processing primary CSI types
      if (IsNumericChar(c)) {
        escArgs[curEscArgIndex] = (escArgs[curEscArgIndex]*10) + (c - '0');
      } else switch(c) {
        case 'i':   // Media Copy
          for (int i = 0; i <= curEscArgIndex; i++) {
            switch (escArgs[i]) {
              case 4:  // Disable print log
                enablePrintLog = false;
                break;
            }
          }
          escState = kStateEscIdle;
          break;

        default:
          escState = kStateEscIdle;
      }
      break;

    default:
      escState = kStateEscIdle;
  }
}


boolean ReportCursorPosition() {
  char buf[10];
  char c;
  int i;
  
  // If the host buffers are available, create and push a Report Cursor Position ANSI sequence
  if (termMode == kModeIsTinyBasic) {
    if (tinyBasicTxNum > (kTinyBasicTxLen-10)) {
      return(false);
    }
  } else {
    if ((hostTxNum1 > (kHostTxBufLen-10)) || (hostTxNum2 > (kHostTxBufLen-10))) {
      return(false);
    }
  }

  // Create a string containing the response (1-based position)
  for (i=0; i<10; i++) buf[i] = 0;
  buf[0] = ESC;
  i = termLineIndex+1;
  if (originMode) i -= scrollTop;   // Adjust report to represent relative to scrolling region if in originMode
  sprintf(&buf[1], "[%0d;%0dR", i, termCharIndex+1);

  // Push the string into the outgoing buffers
  i = 0;
  while ((c = buf[i++]) != 0) {
    if (termMode == kModeIsTinyBasic) {
      PUSH_TB_TX(c);
    } else {
      // Send to host
      PUSH_HOST_TX1(c);
      PUSH_HOST_TX2(c);
    }
  }

  return(true);
}


boolean ReportDeviceStatus() {
  char buf[8];
  char c;
  int i;
  
  // If the host buffers are available, create and push a Report Device Status ANSI sequence
  if (termMode == kModeIsTinyBasic) {
    if (tinyBasicTxNum > (kTinyBasicTxLen-8)) {
      return(false);
    }
  } else {
    if ((hostTxNum1 > (kHostTxBufLen-8)) || (hostTxNum2 > (kHostTxBufLen-8))) {
      return(false);
    }
  }

  // Create a string containing the response
  for (i=0; i<8; i++) buf[i] = 0;
  buf[0] = ESC;
  sprintf(&buf[1], "[0n");

  // Push the string into the outgoing buffers
  i = 0;
  while ((c = buf[i++]) != 0) {
    if (termMode == kModeIsTinyBasic) {
      PUSH_TB_TX(c);
    } else {
      // Send to host
      PUSH_HOST_TX1(c);
      PUSH_HOST_TX2(c);
    }
  }

  return(true);
}


boolean ReportDeviceAttributes() {
  char buf[8];
  char c;
  int i;
  
  // If the host buffers are available, create and push a Report Device Attributes ANSI sequence
  if (termMode == kModeIsTinyBasic) {
    if (tinyBasicTxNum > (kTinyBasicTxLen-8)) {
      return(false);
    }
  } else {
    if ((hostTxNum1 > (kHostTxBufLen-8)) || (hostTxNum2 > (kHostTxBufLen-8))) {
      return(false);
    }
  }

  // Create a string containing the response
  for (i=0; i<8; i++) buf[i] = 0;
  buf[0] = ESC;
  sprintf(&buf[1], "[?6c");  // VT102

  // Push the string into the outgoing buffers
  i = 0;
  while ((c = buf[i++]) != 0) {
    if (termMode == kModeIsTinyBasic) {
      PUSH_TB_TX(c);
    } else {
      // Send to host
      PUSH_HOST_TX1(c);
      PUSH_HOST_TX2(c);
    }
  }

  return(true);
}


boolean ReportPrinterStatus() {
  char buf[10];
  char c;
  int i;
  
  // If the host buffers are available, create and push a Report Device Status ANSI sequence
  if (termMode == kModeIsTinyBasic) {
    if (tinyBasicTxNum > (kTinyBasicTxLen-10)) {
      return(false);
    }
  } else {
    if ((hostTxNum1 > (kHostTxBufLen-10)) || (hostTxNum2 > (kHostTxBufLen-10))) {
      return(false);
    }
  }

  // Create a string containing the response
  for (i=0; i<10; i++) buf[i] = 0;
  buf[0] = ESC;
  if (!printerOn) {
    sprintf(&buf[1], "[?13n");
  } else if (printerPaperOut) {
    sprintf(&buf[1], "[?11n");
  } else {
    sprintf(&buf[1], "[?10n");
  }

  // Push the string into the outgoing buffers
  i = 0;
  while ((c = buf[i++]) != 0) {
    if (termMode == kModeIsTinyBasic) {
      PUSH_TB_TX(c);
    } else {
      // Send to host
      PUSH_HOST_TX1(c);
      PUSH_HOST_TX2(c);
    }
  }

  return(true);
}

void ResetPrinter() {
  if (printerTxNum > 0) {
    // Flush our buffer
    printerTxPushI = 0;
    printerTxPopI = 0;
    printerTxNum = 0;

    // Send a reset to the printer
    PUSH_PRT_TX(CAN);
  }
}


boolean PrintLine(int lineNum, char termChar) {
  char c;
  int i;
  
  // If the printer TX buffer is available then print the current line followed by CR/LF
  if (printerTxNum > (kPrinterTxBufLen - (kNumCharPerLine + 2))) {
    return(false);
  }

  // Push the line into the printer buffer
  for (i=0; i<kNumCharPerLine; i++) {
    c = termBuffer[ComputeTermBuffIndex(lineNum, i)];
    PUSH_PRT_TX(c);
  }

  // Terminate line
  PUSH_PRT_TX(termChar);
  PUSH_PRT_TX(CR);        // Always reset to start of line on printer

  return(true);
}


boolean PrintScreen() {
  char c;
  int i, j;
  int top, bot;
  int bytesRequired;

  // Compute the number of bytes required 
  if (enablePrintRegion) {
    bytesRequired = (scrollBot - scrollTop + 1) * (kNumCharPerLine + 2);
    top = scrollTop;
    bot = scrollBot;
  } else {
    bytesRequired = kNumTermLines * (kNumCharPerLine + 2);
    top = 0;
    bot = kNumTermLines-1;
  }
  if (enableScreenPrintTerm) bytesRequired++;
  
  // If the printer TX buffer is available then print the selected region optionally followed by FF
  if (printerTxNum > (kPrinterTxBufLen - bytesRequired)) {
    return(false);
  }

  // Push the (selected) screen into the printer buffer
  for (i=top; i<=bot; i++) {
    for (j=0; j<kNumCharPerLine; j++) {
      c = termBuffer[ComputeTermBuffIndex(i, j)];
      PUSH_PRT_TX(c);
    }
    PUSH_PRT_TX(CR);
    PUSH_PRT_TX(LF);
  }

  // Terminate screen dump if requested
  if (enableScreenPrintTerm) {
    PUSH_PRT_TX(FF);
  }

  return(true);
}


/* ******************************************************************************************
 *  Utility subroutines
 * ******************************************************************************************/

/* *****************************************************************************
 * InactivityTimeout - Return true if the current timestamp is greater than 
 * a specified timeout from the last timestamp.  Handle the base platform
 * roll-over in its time function.
 *
 * On entry: prevT contains the previous timestamp (in micro-seconds)
 *           timeout contains the specified timeout to compare
 *
 * On exit: True if timeout has expired since prevT, false otherwise
 **************************************************************************** */
boolean InactivityTimeout(unsigned long prevT, unsigned long timeout) {
  unsigned long curT = micros();
  unsigned long deltaT;
  
  if (curT > prevT) {
    deltaT = curT - prevT;
  } else {
    // Handle wrap
    deltaT = ~(prevT - curT) + 1;
  }
  
  return (deltaT >= timeout);
}


/* *****************************************************************************
 * IncPushIndex - Increment a push index accounting for its maximum value
 * and incrementing the buffer's count
 **************************************************************************** */
void IncPushIndex(int* indexPtr, int* indexCount, int maxIndex) {
  if (*indexCount < maxIndex) {
    if (++(*indexPtr) == maxIndex) {
      *indexPtr = 0;
    }

    cli();
    (*indexCount)++;
    sei();
  }
}


/* *****************************************************************************
 * IncPopIndex - Increment a pop index accounting for its maximum value
 * and decrementing the buffer's count
 **************************************************************************** */
void IncPopIndex(int* indexPtr, int* indexCount, int maxIndex) {
  if (*indexCount > 0) {
    if (++(*indexPtr) == maxIndex) {
      *indexPtr = 0;
    }

    cli();
    (*indexCount)--;
    sei();
  }
}


/* *****************************************************************************
 * IsControlChar - Return true if the character is non-displayable, false if
 * displayable
 **************************************************************************** */
boolean IsControlChar(char c) {
  return ((c < 0x20) || (c == DEL));
}


/* *****************************************************************************
 * IsNumericChar - Return true if the character is a number, false otherwise
 **************************************************************************** */
boolean IsNumericChar(char c) {
  return ((c >= '0') && (c <= '9'));
}


/* *****************************************************************************
 * ComputeTermBuffIndex - Return an index for accessing termBuffer for a given
 * valid row and col.
 **************************************************************************** */
int ComputeTermBuffIndex(int row, int col) {
  return(row*kNumCharPerLine + col);
}


/* *****************************************************************************
 * ResetTeensy - Trigger a software reset in the Teensy 3
 *
 * On entry: none
 *
 * On exit: should not exit
 **************************************************************************** */
void ResetTeensy() {
  // Write the Application Interrupt and Reset Control Register to initiate a reset
  SCB_AIRCR = RESTART_VAL;
  while (1) {}; // Wait for the reset to occur
}
