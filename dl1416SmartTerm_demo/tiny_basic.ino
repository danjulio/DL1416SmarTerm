////////////////////////////////////////////////////////////////////////////////
// TinyBasic Plus (Plus)
////////////////////////////////////////////////////////////////////////////////
//
// Based on TinyBasic Plus v0.13.  Portability to console stripped out because
// it became too hard to maintain for the new functionality.  Storing programs
// in EEPROM stripped out because DL1416SmartTerm uses the EEPROM for its own
// purposes and it has access to a huge SD Card.  TONE functionality stripped out.
//
// Original Authors: Mike Field <hamster@snap.net.nz>
//                   Scott Lawrence <yorgle@gmail.com>
//
// Modified for DL1416SmartTerm: Dan Julio <dan@danjuliodesigns.com>
//
//
// v0.14: 2016-09-04 - 2016-10-31
//      Support for Teensy 3 instead of Arduino Due - renamed Tiny Basic Plus Plus (LOL).
//      Integrated with dl1416SmartTerm - output ANSI compliant ESC sequences.
//      Added CLS, PCHR, INCHR, CURSOR, ERASE, INSRC, OUTDST, REBOOT, MKDIR, RMDIR,
//        DNDIR, UPDIR, HLIST, PLIST, HELP.
//      Added logical operators: &, |, ^
//      Fixed INPUT to actually get a number.
//      Finished implementation of PEEK and POKE (within Tiny Basic's RAM) and added
//        DATA@ and PMEM to be able to load table information in memory for access.
//      Modified LIST to take a second argument to allow listing ranges.
//      Added list of signals for Arduino Pin IO.
//      Added support for printer status and for break to flush any ongoing printer
//        activity.
//      Increased stack size to 8.
//      Fixed a few bugs here and there, mostly related to putting multiple statements
//        on one line.
// v0.15: 2016-11-12
//      Added ASC and CHR$
// v1.0 : 2017-10-20
//      Added audio support: commands DRUM, NOTE, WAV and function PLAYING?.
//      Added keyword RENUM (unimplemented now).  Changed UPDIR and DNDIR to
//      be able to be program statements.  Enabled Autorun functionality.
// v1.1 : 2018-02-08
//      Fixed bug in NEW when run from program.
// v1.2 : 2018-10-15
//      Modified DEMO_MODE support to run "demorun.bas" file and indicate demo
//      mode in sign-on message.  Shortened demo timeout to 10 minutes.
//
////////////////////////////////////////////////////////////////////////////////
#include <Audio.h>
#include <avr/pgmspace.h>
#include <SD.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <TimerOne.h>
#include <Wire.h>


////////////////////////////////////////////////////////////////////////////////
// Feature option configuration...

// this turns on "autorun".  if there's FileIO, and a file "autorun.bas" or "demorun.bas",
// then it will load it and run it when starting up
#define ENABLE_AUTORUN 1
//#undef ENABLE_AUTORUN
// and this is the file that gets run
#ifdef DEMO_MODE
#define kAutorunFilename  "demorun.bas"
#else
#define kAutorunFilename  "autorun.bas"
#endif

////////////////////////////////////////////////////////////////////////////////
// Constants...

// Version
#define kTbVersion      "v1.2"

// Memory available to Tiny Basic
#define kRamSize  (kTinyBasicRam-1)

// Arduino-specific configuration
// set this to the card select for your SD shield
#define kSD_CS 4

#define kSD_Fail  0
#define kSD_OK    1

// Maximum number of directory levels for DNDIR and UPDIR commands
#define kMaxDirLevels   16
// 8.3 filenames => 8 characters + 1 '.' + 3 characters + nul string terminator
#define kMaxFilenameLen 13

#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'

#define STACK_SIZE (sizeof(struct stack_for_frame)*8)
#define VAR_SIZE sizeof(short int) // Size of variables in bytes

#define OUTPUT_MASK_SCREEN    0x01
#define OUTPUT_MASK_USB_HOST  0x02
#define OUTPUT_MASK_SER_HOST  0x04
#define OUTPUT_MASK_PRINTER   0x08

typedef short unsigned LINENUM;

////////////////////////////////////////////////////////////////////////////////
// Porting stuff...

#ifndef boolean
#define boolean int
#define true 1
#define false 0
#endif

#ifndef byte
typedef unsigned char byte;
#endif

// some catches for AVR based text string stuff...
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte( A ) *(A)
#endif

////////////////////////////////////////////////////////////////////////////////
// Variables...

File fp;
char * filenameWord(void);
static boolean sd_is_initialized = false;
char cur_file_name[kMaxFilenameLen];
char dir_array[kMaxDirLevels][kMaxFilenameLen];
char expanded_file_name[kMaxDirLevels*kMaxFilenameLen+1];
int cur_dir_level;            // Current directory level, 0 = root level, max is kMaxDirLevels-1

boolean inhibitOutput = false;
static boolean runAfterLoad = false;
static boolean triggerRun = false;

// these will select, at runtime, where IO happens through for load/save
enum {
  kStreamSerial = 0,
  kStreamFile
};
static unsigned char inStream = kStreamSerial;
static unsigned char outStream = kStreamSerial;

static unsigned char program[kRamSize];
static unsigned char *txtpos, *list_line;
static unsigned char expression_error;
static unsigned char *tempsp;

static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *variables_begin;
static unsigned char *current_line;
static unsigned char *sp;
static unsigned char table_index;
static LINENUM linenum;

static unsigned int output_mask;
static unsigned int saved_output_mask;
static unsigned int input_mask;

struct stack_for_frame {
  char frame_type;
  char for_var;
  short int terminal;
  short int step;
  unsigned char *current_line;
  unsigned char *txtpos;
};

struct stack_gosub_frame {
  char frame_type;
  unsigned char *current_line;
  unsigned char *txtpos;
};


////////////////////////////////////////////////////////////////////////////////
// Keyword tables and constants - the last character has 0x80 added to it
// Note: Any keyword must not be a subset of another keyword because matching stops on the 
//       first matched string.
static const unsigned char keywords[] PROGMEM = {
  'L', 'I', 'S', 'T' + 0x80,
  'H', 'L', 'I', 'S', 'T' + 0x80,
  'P', 'L', 'I', 'S', 'T' + 0x80,
  'L', 'O', 'A', 'D' + 0x80,
  'N', 'E', 'W' + 0x80,
  'R', 'U', 'N' + 0x80,
  'S', 'A', 'V', 'E' + 0x80,
  'E', 'R', 'A', 'S', 'E' + 0x80,
  'N', 'E', 'X', 'T' + 0x80,
  'L', 'E', 'T' + 0x80,
  'I', 'F' + 0x80,
  'G', 'O', 'T', 'O' + 0x80,
  'G', 'O', 'S', 'U', 'B' + 0x80,
  'R', 'E', 'T', 'U', 'R', 'N' + 0x80,
  'R', 'E', 'M' + 0x80,
  'F', 'O', 'R' + 0x80,
  'I', 'N', 'P', 'U', 'T' + 0x80,
  'P', 'R', 'I', 'N', 'T' + 0x80,
  'C', 'H', 'R', '$' + 0x80,
  'P', 'C', 'H', 'R' + 0x80,
  'I', 'N', 'C', 'H', 'R' + 0x80,
  'C', 'U', 'R', 'S', 'O', 'R' + 0x80,
  'I', 'N', 'S', 'R', 'C' + 0x80,
  'O', 'U', 'T', 'D', 'S', 'T' + 0x80,
  'D', 'A', 'T', 'A', '@' + 0x80,
  'P', 'O', 'K', 'E' + 0x80,
  'S', 'T', 'O', 'P' + 0x80,
  'B', 'Y', 'E' + 0x80,
  'F', 'I', 'L', 'E', 'S' + 0x80,
  'M', 'E', 'M' + 0x80,
  'C', 'L', 'S' + 0x80,
  '?' + 0x80,
  '\'' + 0x80,
  'A', 'W', 'R', 'I', 'T', 'E' + 0x80,
  'D', 'W', 'R', 'I', 'T', 'E' + 0x80,
  'D', 'E', 'L', 'A', 'Y' + 0x80,
  'D', 'R', 'U', 'M' + 0x80,
  'N', 'O', 'T', 'E' + 0x80,
  'W', 'A', 'V' + 0x80,
  'E', 'N', 'D' + 0x80,
  'R', 'S', 'E', 'E', 'D' + 0x80,
  'C', 'H', 'A', 'I', 'N' + 0x80,
  'R', 'E', 'B', 'O', 'O', 'T' + 0x80,
  'M', 'K', 'D', 'I', 'R' + 0x80,
  'R', 'M', 'D', 'I', 'R' + 0x80,
  'D', 'N', 'D', 'I', 'R' + 0x80,
  'U', 'P', 'D', 'I', 'R' + 0x80,
  'H', 'E', 'L', 'P' + 0x80,
  'R', 'E', 'N', 'U', 'M' + 0x80,
  0
};

// by moving the command list to an enum, we can easily remove sections
// above and below simultaneously to selectively obliterate functionality.
enum {
  KW_LIST = 0,
  KW_HLIST,
  KW_PLIST,
  KW_LOAD,
  KW_NEW,
  KW_RUN,
  KW_SAVE,
  KW_ERASE,
  KW_NEXT,
  KW_LET,
  KW_IF,
  KW_GOTO,
  KW_GOSUB,
  KW_RETURN,
  KW_REM,
  KW_FOR,
  KW_INPUT,
  KW_PRINT,
  KW_CHR,
  KW_PCHR,
  KW_INCHR,
  KW_CURSOR,
  KW_INSRC,
  KW_OUTDST,
  KW_DATA,
  KW_POKE,
  KW_STOP,
  KW_BYE,
  KW_FILES,
  KW_MEM,
  KW_CLS,
  KW_QMARK,
  KW_QUOTE,
  KW_AWRITE,
  KW_DWRITE,
  KW_DELAY,
  KW_DRUM,
  KW_NOTE,
  KW_WAV,
  KW_END,
  KW_RSEED,
  KW_CHAIN,
  KW_REBOOT,
  KW_MKDIR,
  KW_RMDIR,
  KW_DNDIR,
  KW_UPDIR,
  KW_HELP,
  KW_RENUM,
  KW_DEFAULT /* always the final one*/
};

static const unsigned char func_tab[] PROGMEM = {
  'P', 'E', 'E', 'K' + 0x80,
  'A', 'B', 'S' + 0x80,
  'A', 'R', 'E', 'A', 'D' + 0x80,
  'D', 'R', 'E', 'A', 'D' + 0x80,
  'R', 'N', 'D' + 0x80,
  'P', 'E', 'N', 'D' + 0x80,
  'A', 'S', 'C' + 0x80,
  'P', 'L', 'A', 'Y', 'I', 'N', 'G', '?' + 0x80,
  0
};

#define FUNC_PEEK    0
#define FUNC_ABS     1
#define FUNC_AREAD   2
#define FUNC_DREAD   3
#define FUNC_RND     4
#define FUNC_PEND    5
#define FUNC_ASC     6
#define FUNC_PLAYING 7
#define FUNC_UNKNOWN 8

static const unsigned char to_tab[] PROGMEM = {
  'T', 'O' + 0x80,
  0
};

static const unsigned char step_tab[] PROGMEM = {
  'S', 'T', 'E', 'P' + 0x80,
  0
};

static const unsigned char mathop_tab[] PROGMEM = {
  '+' + 0x80,
  '-' + 0x80,
  '*' + 0x80,
  '/' + 0x80,
  '&' + 0x80,
  '|' + 0x80,
  '^' + 0x80,
  0
};

static const unsigned char relop_tab[] PROGMEM = {
  '>', '=' + 0x80,
  '<', '>' + 0x80,
  '>' + 0x80,
  '=' + 0x80,
  '<', '=' + 0x80,
  '<' + 0x80,
  '!', '=' + 0x80,
  0
};

#define RELOP_GE    0
#define RELOP_NE    1
#define RELOP_GT    2
#define RELOP_EQ    3
#define RELOP_LE    4
#define RELOP_LT    5
#define RELOP_NE_BANG   6
#define RELOP_UNKNOWN 7

static const unsigned char highlow_tab[] PROGMEM = {
  'H', 'I', 'G', 'H' + 0x80,
  'H', 'I' + 0x80,
  'L', 'O', 'W' + 0x80,
  'L', 'O' + 0x80,
  0
};

#define HIGHLOW_HIGH    1
#define HIGHLOW_UNKNOWN 4


////////////////////////////////////////////////////////////////////////////////
// Messages...
static const unsigned char okmsg[]            PROGMEM = "OK";
static const unsigned char whatmsg[]          PROGMEM = "What? ";
static const unsigned char howmsg[]           PROGMEM = "How?";
static const unsigned char sorrymsg[]         PROGMEM = "Sorry!";
static const unsigned char initmsg[]          PROGMEM = "TinyBasic Plus " kTbVersion;
static const unsigned char demomsg[]          PROGMEM = "Demo Mode - Resets after 10 min inactivity";
static const unsigned char memorymsg[]        PROGMEM = " bytes free.";
static const unsigned char breakmsg[]         PROGMEM = "break!";
static const unsigned char unimplimentedmsg[] PROGMEM = "Unimplemented";
static const unsigned char backspacemsg[]     PROGMEM = "\b \b";
static const unsigned char indentmsg[]        PROGMEM = "    ";
static const unsigned char sderrormsg[]       PROGMEM = "SD card error.";
static const unsigned char sdfilemsg[]        PROGMEM = "SD file error.";
static const unsigned char sdnofilemsg[]      PROGMEM = "SD file does not exist.";
static const unsigned char dirextmsg[]        PROGMEM = "(dir)";
static const unsigned char slashmsg[]         PROGMEM = "/";
static const unsigned char spacemsg[]         PROGMEM = " ";
static const unsigned char ioerrmsg[]         PROGMEM = "IO pin out of range.";
static const unsigned char chrerrormsg[]      PROGMEM = "CHR$ must be part of PRINT statement.";
static const unsigned char prtOfflineMsg[]    PROGMEM = "Printer offline.";
static const unsigned char prtPaperMsg[]      PROGMEM = "Printer out of paper.";
static const unsigned char helpKeywordMsg[]   PROGMEM = "Keywords: ";
static const unsigned char helpFuncMsg[]      PROGMEM = "Functions: ";
static const unsigned char helpMathMsg[]      PROGMEM = "Math and Logical Operators: ";
static const unsigned char helpRelopMsg[]     PROGMEM = "Relational Operators: ";
static const unsigned char helpLogicLvlMsg[]  PROGMEM = "Pin IO Logic Levels: ";
static const unsigned char stackErrMsg[]      PROGMEM = "Internal Error: Stack is stuffed!";
static const unsigned char audioIndexErrMsg[] PROGMEM = "Unknown Audio Command Index.";
static const unsigned char audioFileErrMsg[]  PROGMEM = "Could not open Audio file.";


////////////////////////////////////////////////////////////////////////////////
// Teensy (Arduino) IO related...
#define kNumDigitalIO  9
#define kNumPwmIO      3
#define kNumAnalogIO   9

// Map the Tiny Basic Indices to our Arduino pins
static const int digitalIOpins[kNumDigitalIO] = {14, 15, 16, 17, 18, 19, 20, 21, 22};
static const int pwmIOpins[kNumPwmIO] = {20, 21, 22};
static const int analogIOpins[kNumAnalogIO] = {0, 1, 2, 3, 4, 5, 6, 7, 8};


////////////////////////////////////////////////////////////////////////////////
// Teensy Audio generation...

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;
AudioPlaySdWav           playSdWav1;
AudioSynthSimpleDrum     drum1;
AudioEffectEnvelope      envelope1;
AudioMixer4              mixer1;
AudioOutputAnalog        dac1;
AudioConnection          patchCord1(waveform1, envelope1);
AudioConnection          patchCord2(playSdWav1, 0, mixer1, 2);
AudioConnection          patchCord3(playSdWav1, 1, mixer1, 3);
AudioConnection          patchCord4(drum1, 0, mixer1, 0);
AudioConnection          patchCord5(envelope1, 0, mixer1, 1);
AudioConnection          patchCord6(mixer1, dac1);
// GUItool: end automatically generated code

// State
static unsigned int note_duration_msec;
static short note_type;
static boolean note_playing = false;

// Wave full filename
char expanded_wavfile_name[kMaxDirLevels*kMaxFilenameLen+1];


#ifdef DEMO_MODE
// Timeout variables for demo mode
const int tb_timeoutSecs = 600;
unsigned long tb_lastKeyPressT;
boolean tb_sawUserActivity;
#endif



////////////////////////////////////////////////////////////////////////////////
// Code...


/***************************************************************************/
static void ignore_blanks(void)
{
  while (*txtpos == SPACE || *txtpos == TAB)
    txtpos++;
}

/***************************************************************************/
static void scantable(unsigned const char *table)
{

  int i = 0;
  table_index = 0;
  while (1)
  {
    // Run out of table entries?
    if (pgm_read_byte( table ) == 0)
      return;

    // Do we match this character?
    if (txtpos[i] == pgm_read_byte( table ))
    {
      i++;
      table++;
    }
    else
    {
      // do we match the last character of keywork (with 0x80 added)? If so, return
      if (txtpos[i] + 0x80 == pgm_read_byte( table ))
      {
        txtpos += i + 1; // Advance the pointer to following the keyword
        ignore_blanks();
        return;
      }

      // Forward to the end of this keyword
      while ((pgm_read_byte( table ) & 0x80) == 0)
        table++;

      // Now move on to the first character of the next word, and reset the position index
      table++;
      table_index++;
      ignore_blanks();
      i = 0;
    }
  }
}

/***************************************************************************/
static void pushb(unsigned char b)
{
  sp--;
  *sp = b;
}

/***************************************************************************/
static unsigned char popb()
{
  unsigned char b;
  b = *sp;
  sp++;
  return b;
}

/***************************************************************************/
void printnum(int num)
{
  int digits = 0;

  if (num < 0)
  {
    num = -num;
    outchar('-');
  }
  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outchar(popb());
    digits--;
  }
}

void printUnum(unsigned int num)
{
  int digits = 0;

  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outchar(popb());
    digits--;
  }
}

/***************************************************************************/
static unsigned short testnum(void)
{
  unsigned short num = 0;
  ignore_blanks();

  while (*txtpos >= '0' && *txtpos <= '9' )
  {
    // Trap overflows
    if (num >= 0xFFFF / 10)
    {
      num = 0xFFFF;
      break;
    }

    num = num * 10 + *txtpos - '0';
    txtpos++;
  }
  return  num;
}

/***************************************************************************/
static unsigned char print_quoted_string(void)
{
  int i = 0;
  unsigned char delim = *txtpos;
  if (delim != '"' && delim != '\'')
    return 0;
  txtpos++;

  // Check we have a closing delimiter
  while (txtpos[i] != delim)
  {
    if (txtpos[i] == NL)
      return 0;
    i++;
  }

  // Print the characters
  while (*txtpos != delim)
  {
    outchar(*txtpos);
    txtpos++;
  }
  txtpos++; // Skip over the last delimiter

  return 1;
}

/***************************************************************************/
// Returns: -1 for malformed statement/expression
//           0 for statement not found
//           1 for success
static int print_chr(void)
{
  short int e = 0;

  if ((txtpos[0] == 'C') && (txtpos[1] == 'H') && (txtpos[2] == 'R') &&
      (txtpos[3] == '$'))
    {
      txtpos += 4;
      ignore_blanks();
      if (*txtpos++ != '(')
        return -1;

      // Get the character expression
      expression_error = 0;
      e = expression();
      if (expression_error)
        return -1;

      // Make sure we see a closing paren
      if (*txtpos++ != ')')
        return -1;

      ignore_blanks();
      
      // Finally output the character
      outchar(e & 0x7F);
      return 1;
    } else {
      return 0;
    }
}

/***************************************************************************/
void printmsgNoNL(const unsigned char *msg)
{
  while ( pgm_read_byte( msg ) != 0 ) {
    outchar( pgm_read_byte( msg++ ) );
  };
}

/***************************************************************************/
void printmsg(const unsigned char *msg)
{
  printmsgNoNL(msg);
  line_terminator();
}

/***************************************************************************/
void print_table(const unsigned char *table)
{
  while ( pgm_read_byte( table ) != 0) {
    outchar( pgm_read_byte( table ) & 0x7F );
    if ((pgm_read_byte( table++ ) & 0x80) == 0x80) {
      outchar( SPACE );
    }
  }
}

/***************************************************************************/
static void getln(char prompt)
{
  if (prompt != NUL) {
    outchar(prompt);
  }
  txtpos = program_end + sizeof(LINENUM);
  while (1)
  {
    char c = inchar();
    switch (c)
    {
      case NL:
      //break;
      case CR:
        line_terminator();
        // Terminate all strings with a NL
        txtpos[0] = NL;
        return;
      case CTRLH:
      case DEL:
        if (txtpos == program_end)
          break;
        txtpos--;

        //printmsg(backspacemsg);
        outchar(DEL);
        break;
      default:
        // We need to leave at least one space to allow us to shuffle the line into order
        if (txtpos == variables_begin - 2)
          outchar(BELL);
        else
        {
          // Update our buffer if it is a displayable character, otherwise just echo it back
          if (!IsControlChar(c)) {
            txtpos[0] = c;
            txtpos++;
          }
          outchar(c);
        }
    }
  }
}

/***************************************************************************/
static int getnum()
{
  int n = 0;
  char c;
  char buf[8];
  char* bufP = &buf[0];
  boolean neg_num = false;
  boolean num_started = false;

  while (1)
  {
    c = inchar();
    if ((c == NL) || (c == CR)) {
      // Number terminated
      line_terminator();

      // Compute value
      for (char* cP = &buf[0]; cP < bufP; cP++) {
        n = (n * 10) + (*cP - '0');
      }
      if (neg_num) {
        n = -n;
      }
      return n;
    } else if (c == '-') {
      if (!num_started) {
        neg_num = true;
        num_started = true;
        outchar(c);
      }
    } else if ((c >= '0') && (c <= '9') && (bufP <= &buf[0] + 5)) {
      *bufP++ = c;
      num_started = true;
      outchar(c);
    } else if ((c == CTRLH) || (c == DEL)) {
      if (bufP > &buf[0]) {
        bufP--;
        outchar(DEL);
      }
    } else if (c == CTRLC) {
      return n;
    }
  }
}

/***************************************************************************/
static unsigned char *findline(void)
{
  unsigned char *line = program_start;
  while (1)
  {
    if (line == program_end)
      return line;

    if (((LINENUM *)line)[0] >= linenum)
      return line;

    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
}

/***************************************************************************/
static void toUppercaseBuffer(void)
{
  unsigned char *c = program_end + sizeof(LINENUM);
  unsigned char quote = 0;

  while (*c != NL)
  {
    // Are we in a quoted string?
    if (*c == quote)
      quote = 0;
    else if (*c == '"' || *c == '\'')
      quote = *c;
    else if (quote == 0 && *c >= 'a' && *c <= 'z')
      *c = *c + 'A' - 'a';
    c++;
  }
}

/***************************************************************************/
void printline()
{
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnum(line_num);
  outchar(' ');
  while (*list_line != NL)
  {
    outchar(*list_line);
    list_line++;
  }
  list_line++;
  line_terminator();
}

/***************************************************************************/
static short int expr4(void)
{
  // fix provided by Jurg Wullschleger wullschleger@gmail.com
  // fixes whitespace and unary operations
  ignore_blanks();

  if ( *txtpos == '-' ) {
    txtpos++;
    return -expr4();
  }
  // end fix

  if (*txtpos == '0')
  {
    txtpos++;
    return 0;
  }

  if (*txtpos >= '1' && *txtpos <= '9')
  {
    short int a = 0;
    do  {
      a = a * 10 + *txtpos - '0';
      txtpos++;
    }
    while (*txtpos >= '0' && *txtpos <= '9');
    return a;
  }

  // Is it a function or variable reference?
  if (txtpos[0] >= 'A' && txtpos[0] <= 'Z')
  {
    short int a;
    // Is it a variable reference (single alpha)
    if (txtpos[1] < 'A' || txtpos[1] > 'Z')
    {
      a = ((short int *)variables_begin)[*txtpos - 'A'];
      txtpos++;
      return a;
    }

    // Is it a function with no or a single parameter
    scantable(func_tab);
    if (table_index == FUNC_UNKNOWN)
      goto expr4_error;

    unsigned char f = table_index;

    // Look for functions with no parameters
    switch (f)
    {
      case FUNC_PEND:
        return (program_end - program_start);
    }

    // Look for functions with one parameter (get the parameter first)
    if (*txtpos != '(')
      goto expr4_error;
    txtpos++;

    // Check for non-numeric arguments first
    switch (f)
    {
      case FUNC_ASC:
        if (txtpos[1] != ')')
          goto expr4_error;
        a = *txtpos;
        txtpos += 2;  // Skip past character and closen paren
        return a;
    }
    
    a = expression();
    if (*txtpos != ')')
      goto expr4_error;
    txtpos++;
    switch (f)
    {
      case FUNC_PEEK:
      if ((a < 0) || ( a >= kRamSize)) {
        return (0);
      } else {
        return program[a];
      }

      case FUNC_ABS:
        if (a < 0)
          return -a;
        return a;

      case FUNC_AREAD:
        if ((a < 0) || (a >= kNumAnalogIO)) {
          return(0);
        } else {
          pinMode( analogIOpins[a], INPUT );
          return analogRead( analogIOpins[a] );
        }
      
      case FUNC_DREAD:
        if ((a < 0) || (a >= kNumDigitalIO)) {
          return(0);
        } else {
          pinMode( digitalIOpins[a], INPUT );
          return digitalRead( digitalIOpins[a] );
        }

      case FUNC_RND:
        return ( random( a ));

      case FUNC_PLAYING:
        if (a == 0) {
          return (note_playing) ? 1 : 0;
        } else {
          return (playSdWav1.isPlaying()) ? 1 : 0;
        }
    }
  }

  if (*txtpos == '(')
  {
    short int a;
    txtpos++;
    a = expression();
    if (*txtpos != ')')
      goto expr4_error;

    txtpos++;
    return a;
  }

expr4_error:
  expression_error = 1;
  return 0;

}

/***************************************************************************/
static short int expr3(void)
{
  short int a, b;

  a = expr4();

  ignore_blanks(); // fix for eg:  100 a = a + 1

  while (1)
  {
    switch (*txtpos) {
      case '*':
        txtpos++;
        b = expr4();
        a *= b;
        break;
        
      case '/':
        txtpos++;
        b = expr4();
        if (b != 0)
          a /= b;
        else
          expression_error = 1;
        break;
        
      case '|':
        txtpos++;
        b = expr4();
        a |= b;
        break;
        
      case '&':
        txtpos++;
        b = expr4();
        a &= b;
        break;
        
      case '^':
        txtpos++;
        b = expr4();
        a ^= b;
        break;
        
      default:
        return a;
    }
  }
}

/***************************************************************************/
static short int expr2(void)
{
  short int a, b;

  if (*txtpos == '-' || *txtpos == '+')
    a = 0;
  else
    a = expr3();

  while (1)
  {
    if (*txtpos == '-')
    {
      txtpos++;
      b = expr3();
      a -= b;
    }
    else if (*txtpos == '+')
    {
      txtpos++;
      b = expr3();
      a += b;
    }
    else
      return a;
  }
}

/***************************************************************************/
static short int expression(void)
{
  short int a, b;

  a = expr2();

  // Check if we have an error
  if (expression_error)  return a;

  scantable(relop_tab);
  if (table_index == RELOP_UNKNOWN)
    return a;

  switch (table_index)
  {
    case RELOP_GE:
      b = expr2();
      if (a >= b) return 1;
      break;
    case RELOP_NE:
    case RELOP_NE_BANG:
      b = expr2();
      if (a != b) return 1;
      break;
    case RELOP_GT:
      b = expr2();
      if (a > b) return 1;
      break;
    case RELOP_EQ:
      b = expr2();
      if (a == b) return 1;
      break;
    case RELOP_LE:
      b = expr2();
      if (a <= b) return 1;
      break;
    case RELOP_LT:
      b = expr2();
      if (a < b) return 1;
      break;
  }
  return 0;
}

/***************************************************************************/
void tb_loop()
{
  unsigned char *start;
  unsigned char *newEnd;
  unsigned char linelen;
  boolean isDigital;
  int val;

  program_start = program;
  program_end = program_start;
  sp = program + sizeof(program); // Needed for printnum
  stack_limit = program + sizeof(program) - STACK_SIZE;
  variables_begin = stack_limit - 27 * VAR_SIZE;

  // memory free
  printnum(variables_begin - program_end);
  printmsg(memorymsg);

warmstart:
  // End any playing audio
  end_audio();
  
  // this signifies that it is running in 'direct' mode.
  current_line = 0;
  sp = program + sizeof(program);
  printmsg(okmsg);

prompt:
  if ( triggerRun ) {
    triggerRun = false;
    current_line = program_start;
    goto execline;
  }

#ifdef DEMO_MODE
  tb_lastKeyPressT = millis();
#endif

  getln( '>' );
  toUppercaseBuffer();

  txtpos = program_end + sizeof(unsigned short);

  // Find the end of the freshly entered line
  while (*txtpos != NL)
    txtpos++;

  // Move it to the end of program_memory
  {
    unsigned char *dest;
    dest = variables_begin - 1;
    while (1)
    {
      *dest = *txtpos;
      if (txtpos == program_end + sizeof(unsigned short))
        break;
      dest--;
      txtpos--;
    }
    txtpos = dest;
  }

  // Now see if we have a line number
  linenum = testnum();
  ignore_blanks();
  if (linenum == 0)
    goto direct;

  if (linenum == 0xFFFF)
    goto qhow;

  // Find the length of what is left, including the (yet-to-be-populated) line header
  linelen = 0;
  while (txtpos[linelen] != NL)
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short) + sizeof(char); // Add space for the line number and line length

  // Now we have the number, add the line header.
  txtpos -= 3;
  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;


  // Merge it into the rest of the program
  start = findline();

  // If a line with that number exists, then remove it
  if (start != program_end && *((LINENUM *)start) == linenum)
  {
    unsigned char *dest, *from;
    unsigned tomove;

    from = start + start[sizeof(LINENUM)];
    dest = start;

    tomove = program_end - from;
    while ( tomove > 0)
    {
      *dest = *from;
      from++;
      dest++;
      tomove--;
    }
    program_end = dest;
  }

  if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
    goto prompt;



  // Make room for the new line, either all in one hit or lots of little shuffles
  while (linelen > 0)
  {
    unsigned int tomove;
    unsigned char *from, *dest;
    unsigned int space_to_make;

    space_to_make = txtpos - program_end;

    if (space_to_make > linelen)
      space_to_make = linelen;
    newEnd = program_end + space_to_make;
    tomove = program_end - start;


    // Source and destination - as these areas may overlap we need to move bottom up
    from = program_end;
    dest = newEnd;
    while (tomove > 0)
    {
      from--;
      dest--;
      *dest = *from;
      tomove--;
    }

    // Copy over the bytes into the new space
    for (tomove = 0; tomove < space_to_make; tomove++)
    {
      *start = *txtpos;
      txtpos++;
      start++;
      linelen--;
    }
    program_end = newEnd;
  }
  goto prompt;

unimplemented:
  end_audio();
  printmsg(unimplimentedmsg);
  goto prompt;

qhow:
  end_audio();
  printmsg(howmsg);
  goto prompt;

qwhat:
  end_audio();
  printmsgNoNL(whatmsg);
  if (current_line != NULL)
  {
    unsigned char tmp = *txtpos;
    if (*txtpos != NL)
      *txtpos = '^';
    list_line = current_line;
    printline();
    *txtpos = tmp;
  }
  line_terminator();
  goto prompt;

qsorry:
  printmsg(sorrymsg);
  goto warmstart;

qioerr:
  printmsg(ioerrmsg);
  goto warmstart;

qsounderr:
  printmsg(audioIndexErrMsg);
  goto warmstart;

qsoundfileerr:
  printmsg(audioFileErrMsg);
  goto warmstart;

run_next_statement:
  while (*txtpos == ':')
    txtpos++;
  ignore_blanks();
  if (*txtpos == NL)
    goto execnextline;
  goto interperateAtTxtpos;

direct:
  txtpos = program_end + sizeof(LINENUM);
  if (*txtpos == NL)
    goto prompt;

interperateAtTxtpos:
  if (breakcheck())
  {
    printmsg(breakmsg);
    ResetPrinter();    // Clear any currently on-going printer activity
    goto warmstart;
  }

  scantable(keywords);

  switch (table_index)
  {
    case KW_DELAY:
      goto do_delay;
    case KW_FILES:
      goto files;
    case KW_LIST:
      goto list;
    case KW_HLIST:
      goto hlist;
    case KW_PLIST:
      goto plist;
    case KW_CHAIN:
      goto chain;
    case KW_LOAD:
      goto load;
    case KW_MEM:
      goto mem;
    case KW_CLS:
      goto cls;
    case KW_NEW:
      if (txtpos[0] != NL)
        goto qwhat;
      program_end = program_start;
      current_line = 0;
      goto prompt;
    case KW_REBOOT:
        ResetTeensy();
    case KW_MKDIR:
      goto mk_dir;
    case KW_RMDIR:
      goto rm_dir;
    case KW_DNDIR:
      goto dn_dir;
    case KW_UPDIR:
      goto up_dir;
    case KW_RUN:
      current_line = program_start;
      goto execline;
    case KW_SAVE:
      goto save;
    case KW_ERASE:
      goto del_file;
    case KW_NEXT:
      goto next;
    case KW_LET:
      goto assignment;
    case KW_IF:
      short int val;
      expression_error = 0;
      val = expression();
      if (expression_error || *txtpos == NL)
        goto qhow;
      if (val != 0)
        goto interperateAtTxtpos;
      goto execnextline;
    case KW_GOTO:
      expression_error = 0;
      linenum = expression();
      if (expression_error || *txtpos != NL)
        goto qhow;
      current_line = findline();
      goto execline;
    case KW_GOSUB:
      goto gosub;
    case KW_RETURN:
      goto gosub_return;
    case KW_REM:
    case KW_QUOTE:
      goto execnextline;  // Ignore line completely
    case KW_FOR:
      goto forloop;
    case KW_INPUT:
      goto input;
    case KW_PRINT:
    case KW_QMARK:
      goto print;
    case KW_CHR:
      printmsg(chrerrormsg);
      goto warmstart;
    case KW_PCHR:
      goto print_char;
    case KW_INCHR:
      goto input_char;
    case KW_CURSOR:
      goto set_cursor;
    case KW_INSRC:
      goto set_input;
    case KW_OUTDST:
      goto set_output;
    case KW_DATA:
      goto load_data;
    case KW_POKE:
      goto poke;
    case KW_END:
    case KW_STOP:
      // This is the easy way to end - set the current line to the end of program attempt to run it
      if (txtpos[0] != NL)
        goto qwhat;
      end_audio();
      current_line = program_end;
      goto execline;
    case KW_BYE:
      // Leave the basic interpreter
      return;
    case KW_AWRITE:  // AWRITE <pin>, HIGH|LOW
      isDigital = false;
      goto awrite;
    case KW_DWRITE:  // DWRITE <pin>, HIGH|LOW
      isDigital = true;
      goto dwrite;
    case KW_RSEED:
      goto rseed;
    case KW_DEFAULT:
      goto assignment;
    case KW_DRUM:
      goto drum;
    case KW_NOTE:
      goto note;
    case KW_WAV:
      goto play_wav;
    case KW_HELP:
      goto print_help;
    case KW_RENUM:
      goto unimplemented;
    default:
      break;
  }

execnextline:
  if (current_line == NULL)   // Processing direct commands?
    goto prompt;
  current_line +=  current_line[sizeof(LINENUM)];

execline:
  if (current_line == program_end) // Out of lines to run
    goto warmstart;
  txtpos = current_line + sizeof(LINENUM) + sizeof(char);
  goto interperateAtTxtpos;

do_delay:
  {
    expression_error = 0;
    val = expression();
    if (expression_error)
      goto qwhat;
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
    
    delay( val );
  }
  goto run_next_statement;
  
input:
  {
    unsigned char var;

    // Figure out variable
    ignore_blanks();
    if (*txtpos < 'A' || *txtpos > 'Z')
      goto qwhat;
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;

    // Read input stream
    ((short int *)variables_begin)[var - 'A'] = getnum();
  }
  goto run_next_statement;

forloop:
  {
    unsigned char var;
    short int initial, step, terminal;
    ignore_blanks();
    if (*txtpos < 'A' || *txtpos > 'Z')
      goto qwhat;
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if (*txtpos != '=')
      goto qwhat;
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    initial = expression();
    if (expression_error)
      goto qwhat;

    scantable(to_tab);
    if (table_index != 0)
      goto qwhat;

    terminal = expression();
    if (expression_error)
      goto qwhat;

    scantable(step_tab);
    if (table_index == 0)
    {
      step = expression();
      if (expression_error)
        goto qwhat;
    }
    else
      step = 1;

    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;

    if (!expression_error)
    {
      struct stack_for_frame *f;
      if (sp + sizeof(struct stack_for_frame) < stack_limit)
        goto qsorry;

      sp -= sizeof(struct stack_for_frame);
      f = (struct stack_for_frame *)sp;
      ((short int *)variables_begin)[var - 'A'] = initial;
      f->frame_type = STACK_FOR_FLAG;
      f->for_var = var;
      f->terminal = terminal;
      f->step     = step;
      f->txtpos   = txtpos;
      f->current_line = current_line;
      goto run_next_statement;
    }
  }
  goto qhow;

gosub:
  expression_error = 0;
  linenum = expression();
  if (!expression_error && ((*txtpos == NL) || (*txtpos == ':')))
  {
    struct stack_gosub_frame *f;
    if (sp + sizeof(struct stack_gosub_frame) < stack_limit)
      goto qsorry;

    sp -= sizeof(struct stack_gosub_frame);
    f = (struct stack_gosub_frame *)sp;
    f->frame_type = STACK_GOSUB_FLAG;
    f->txtpos = txtpos;
    f->current_line = current_line;
    current_line = findline();
    goto execline;
  }
  goto qhow;

next:
  // Fnd the variable name
  ignore_blanks();
  if (*txtpos < 'A' || *txtpos > 'Z')
    goto qhow;
  val = *txtpos;  // Save variable name
  txtpos++;
  ignore_blanks();
  if (*txtpos != ':' && *txtpos != NL)
    goto qwhat;

gosub_return:
  // Now walk up the stack frames and find the frame we want, if present
  tempsp = sp;
  while (tempsp < program + sizeof(program) - 1)
  {
    switch (tempsp[0])
    {
      case STACK_GOSUB_FLAG:
        if (table_index == KW_RETURN)
        {
          struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
          current_line  = f->current_line;
          txtpos      = f->txtpos;
          sp += sizeof(struct stack_gosub_frame);
          goto run_next_statement;
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_gosub_frame);
        break;
      case STACK_FOR_FLAG:
        // Flag, Var, Final, Step
        if (table_index == KW_NEXT)
        {
          struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
          // Is the the variable we are looking for?
          if (((char) val) == f->for_var)
          {
            short int *varaddr = ((short int *)variables_begin) + val - 'A';
            *varaddr = *varaddr + f->step;
            // Use a different test depending on the sign of the step increment
            if ((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
            {
              // We have to loop so don't pop the stack
              txtpos = f->txtpos;
              current_line = f->current_line;
              goto run_next_statement;
            }
            // We've run to the end of the loop. drop out of the loop, popping the stack
            sp = tempsp + sizeof(struct stack_for_frame);
            goto run_next_statement;
          }
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_for_frame);
        break;
      default:
        printmsg(stackErrMsg);
        goto warmstart;
    }
  }
  // Didn't find the variable we've been looking for
  goto qhow;

assignment:
  {
    short int value;
    short int *var;

    if (*txtpos < 'A' || *txtpos > 'Z')
      goto qhow;
    var = (short int *)variables_begin + *txtpos - 'A';
    txtpos++;

    ignore_blanks();

    if (*txtpos != '=')
      goto qwhat;
    txtpos++;
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto qwhat;
    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
    *var = value;
  }
  goto run_next_statement;

load_data:
  {
    short int address;
    short int value;
    boolean done = false;

    // See if we can find an address
    expression_error = 0;
    address = expression();
    if (expression_error)
      goto qwhat;

    // Process any arguments
    if (txtpos[0] == ',') {
      txtpos++;
      ignore_blanks();
      while (!done) {
        if (!((txtpos[0] == NL) || (txtpos[0] == ':') ||
             ((txtpos[0] >= '0') && (txtpos[0] <= '9')))) {
          // Unexpected character
          goto qwhat;
        } else if ((txtpos[0] == NL) || (txtpos[0] == ':')) {
          done = true;
        } else {
          value = testnum();
          program[address++] = value & 0xFF;
          ignore_blanks();
          if (txtpos[0] == ',') txtpos++;
          ignore_blanks();
        }
      }
    }
  }
  goto run_next_statement;
  
poke:
  {
    short int address;
    short int value;

    // Work out where to put it
    expression_error = 0;
    address = expression();
    if (expression_error)
      goto qwhat;

    // check for a comma
    if (*txtpos != ',')
      goto qwhat;
    txtpos++;
    ignore_blanks();

    // Now get the value to assign
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto qwhat;

    // Do a validated assignment
    if ((address >= 0) && (address < kRamSize)) {
      program[address] = value & 0xFF;
    }
    
    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
  }
  goto run_next_statement;

list:
  {
    // List with the current output mask
    saved_output_mask = output_mask;
    goto rawlist;
  }
  
hlist:
  {
    // List to the host
    saved_output_mask = output_mask;
    output_mask = OUTPUT_MASK_USB_HOST;
    goto rawlist;
  }

plist:
  {
    // List to the printer
    //
    // Save the current output mask
    saved_output_mask = output_mask;
  
    // First make sure the printer is online
    if (!PRT_ONLINE()) {
      printmsg(prtOfflineMsg);
      goto warmstart;
    }

    // Then that it has paper
    if (PRT_PAPER_OUT()) {
      printmsg(prtPaperMsg);
      goto warmstart;
    }

    // And finally execute the list
    output_mask = OUTPUT_MASK_PRINTER;
    goto rawlist;
  }

rawlist:
  {
    unsigned char *last_list_line;
    short unsigned save_linenum;
    
    linenum = testnum(); // Returns 0 if no line found.
    ignore_blanks();
    last_list_line = program_end;  // May be overwritten with new last line

    // Check for comma (for list end number) if we found a number
    if (linenum != 0) {
      if (txtpos[0] == ',') {
        // Tuck away the first line number
        save_linenum = linenum;
        
        // Try to find a second line number
        txtpos++;
        linenum = testnum();
        ignore_blanks();

        if (linenum != 0) {
          // Found a second line number, now find it's location
          last_list_line = findline();
        }

        // Restore the starting line number
        linenum = save_linenum;
      }
    }

    // Should be EOL
    if (txtpos[0] != NL) {
      output_mask = saved_output_mask;
      goto qwhat;
    }

    // Find the line
    list_line = findline();
    while (list_line != last_list_line)
      printline();
      
    output_mask = saved_output_mask;
    goto warmstart;
  }

print:
  // If we have an empty list then just put out a NL
  if (*txtpos == ':' )
  {
    line_terminator();
    txtpos++;
    goto run_next_statement;
  }
  if (*txtpos == NL)
  {
    goto execnextline;
  }

  while (1)
  {
    // Look for things we can print
    ignore_blanks();
    int r = print_chr();
    if (r == 1)
    {
      ;
    }
    else if (r == -1)
      goto qwhat;
    else if (print_quoted_string())
    {
      ;
    }
    else if (*txtpos == '"' || *txtpos == '\'')
      goto qwhat;
    else
    {
      short int e;
      expression_error = 0;
      e = expression();
      if (expression_error)
        goto qwhat;
      printnum(e);
    }

    // At this point we have three options, a comma or a new line
    if (*txtpos == ',')
      txtpos++; // Skip the comma and move onto the next
    else if (txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
    {
      txtpos++; // This has to be the end of the print - no newline
      break;
    }
    else if (*txtpos == NL || *txtpos == ':')
    {
      line_terminator();  // The end of the print statement
      break;
    }
    else
      goto qwhat;
  }
  goto run_next_statement;

print_char:
  {
    short int e = 0;

    expression_error = 0;
    e = expression();
    if (expression_error)
      goto qwhat;

    // Make sure we're at the end of the line
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
      
    outchar(e & 0x7F);
  }
  goto run_next_statement;

input_char:
  {
    unsigned char var;
    char c;
    short int e;                  // e is a timeout in mSec
    short int cur_timeout = 0;    // remaining timeout before bailing
    boolean got_char = false;

    // Figure out variable to store the character value in
    ignore_blanks();
    if (*txtpos < 'A' || *txtpos > 'Z')
      goto qwhat;
    var = *txtpos;
    txtpos++;
    ignore_blanks();

    // Try to get the timeout from a subsequent expression
    if (*txtpos == ',') {
      txtpos++;
      expression_error = 0;
      e = expression();
      cur_timeout = e;
      if (expression_error)
        goto qwhat;
    } else {
      e = 0;  // No timeout specified means wait forever
    }
    
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;

    // Read input stream
    while (!got_char) {
      if (TB_TX_AVAIL()) {
        c = PEEK_TB_TX();
        ((short int *)variables_begin)[var - 'A'] = (short int) c;
        POP_TB_TX();
        got_char = true;
      } else {
        // Wait a bit to see if anything comes in
        delay(1);
        if (e != 0) {
          if (--cur_timeout == 0) {
            // Timed out, return 0
            got_char = true;
            ((short int *)variables_begin)[var - 'A'] = 0;
          }
        }
      }
    }
    goto run_next_statement;
  }

set_cursor:
  {
    short int e1 = 0;
    short int e2 = 0;

    // Get row
    expression_error = 0;
    e1 = expression();
    if (expression_error)
      goto qwhat;

    if (!((*txtpos == NL) || (*txtpos == ':') || (*txtpos == ','))) {
      goto qwhat;
    }

    // Get column
    if (*txtpos == ',') {
      //ignore_blanks();
      txtpos++;
      e2 = expression();
      if (expression_error)
        goto qwhat;
    }

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;

    // Convert to 1-based for the escape sequence and validate
    e1 += 1;
    e2 += 1; 
    if (e1 > 99) e1 = 99;
    else if (e1 < 0) e1 = 1;
    if (e2 > 99) e2 = 99;
    else if (e2 < 0) e2 = 1;

    // Send set cursor escape sequence
    outchar(ESC);
    outchar('[');
    // Row
    if (e1 > 9) {
      // Tens digit
      outchar('0' + (e1/10));
      // Units digit
      outchar('0' + (e1%10));
    } else {
      // Units digit only
      outchar('0' + e1);
    }

    // Column/Row separator
    outchar(';');

    // Column
    if (e2 > 9) {
      // Tens digit
      outchar('0' + (e2/10));
      // Units digit
      outchar('0' + (e2%10));
    } else {
      // Units digit only
      outchar('0' + e2);
    }
  
    // Terminator
    outchar('H');
  }
  goto run_next_statement;

set_input:
  {
    short int e;

    // Get output mask
    expression_error = 0;
    e = expression();
    if (expression_error)
      goto qwhat;

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;

    input_mask = e;
  }
  goto run_next_statement;
  
set_output:
  {
    short int e;

    // Get output mask
    expression_error = 0;
    e = expression();
    if (expression_error)
      goto qwhat;

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;

    output_mask = e;
  }
  goto run_next_statement;

mem:
  // memory free
  printnum(variables_begin - program_end);
  printmsg(memorymsg);
  goto run_next_statement;

cls:
  // Make sure we're at the end of the line
  if ((*txtpos != NL) && (*txtpos != ':'))
    goto qwhat;
      
  // Clear screen (ANSI)
  outchar(ESC);
  outchar('[');
  outchar('2');
  outchar('J');
  goto run_next_statement;

awrite: // AWRITE <pin>,val
dwrite:
  {
    short int pinNo;
    short int value;

    // Get the pin number
    expression_error = 0;
    pinNo = expression();
    if (expression_error)
      goto qwhat;

    // check for a comma
    if (*txtpos != ',')
      goto qwhat;
    txtpos++;
    ignore_blanks();

    scantable(highlow_tab);
    if (table_index != HIGHLOW_UNKNOWN)
    {
      if ( table_index <= HIGHLOW_HIGH ) {
        value = 1;
      }
      else {
        value = 0;
      }
    }
    else {
      // and the value (numerical)
      expression_error = 0;
      value = expression();
      if (expression_error)
        goto qwhat;
    }

    // Validate the pin number
    if ( isDigital) {
      if ((pinNo < 0) || (pinNo >= kNumDigitalIO)) {
        goto qioerr;
      }
    } else {
      if ((pinNo < 0) || (pinNo >= kNumPwmIO)) {
        goto qioerr;
      }
    }

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;
    
    pinMode( digitalIOpins[pinNo], OUTPUT );
    if ( isDigital ) {
      digitalWrite( digitalIOpins[pinNo], value );
    }
    else {
      analogWrite( pwmIOpins[pinNo], value );
    }
  }
  goto run_next_statement;

files:
  // display a listing of files on the device.
  cmd_Files();
  goto warmstart;

chain:
  runAfterLoad = true;

load:
  // clear the program
  program_end = program_start;

  // load from a file into memory
  {
    char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto qwhat;

    build_expanded_filename(filename);
    if ( !SD.exists( expanded_file_name ))
    {
      printmsg( sdnofilemsg );
    }
    else {

      fp = SD.open( expanded_file_name );
      inStream = kStreamFile;
      inhibitOutput = true;
    }
  }
  goto warmstart;

save:
  // save from memory out to a file
  {
    char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto qwhat;

    build_expanded_filename(filename);
    // remove the old file if it exists
    if ( SD.exists( expanded_file_name )) {
      SD.remove( expanded_file_name );
    }

    // open the file, switch over to file output
    fp = SD.open( expanded_file_name, FILE_WRITE );
    outStream = kStreamFile;

    // copied from "List"
    list_line = findline();
    while (list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;

    fp.close();
    goto warmstart;
  }

del_file:
  // Delete file
  {
    char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if (expression_error)
      goto qwhat;
    
    build_expanded_filename(filename);

    // remove the old file if it exists
    if ( SD.exists( expanded_file_name )) {
      if ( !SD.remove( expanded_file_name ) ) {
        printmsg( sdfilemsg );
      }
    } else {
      printmsg( sdnofilemsg );
    }

    goto warmstart;
  }

mk_dir:
  // Make directory
  {
    char *dirname;

    // Work out the directory name
    expression_error = 0;
    dirname = filenameWord();
    if (expression_error)
      goto qwhat;
    
    build_expanded_filename(dirname);
    // Try to make the directory if it doesn't exist
    if ( !SD.exists( expanded_file_name )) {
      if ( !SD.mkdir( expanded_file_name ) ) {
        printmsg( sdfilemsg );
      }
    } else {
      printmsg( sdnofilemsg );
    }
    goto warmstart;
  }

rm_dir:
  // Remove directory
  {
    char *dirname;

    // Work out the directory name
    expression_error = 0;
    dirname = filenameWord();
    if (expression_error)
      goto qwhat;

    build_expanded_filename(dirname);
    // Try to make the directory if it doesn't exist
    if ( SD.exists( expanded_file_name )) {
      if ( !SD.rmdir( expanded_file_name ) ) {
        printmsg( sdfilemsg );
      }
    } else {
      printmsg( sdnofilemsg );
    }
    goto warmstart;
  }

dn_dir:
  // Enter directory
  {
    char *dirname;
    int i = 0;

    // Work out the directory name
    expression_error = 0;
    dirname = filenameWord();
    if (expression_error)
      goto qwhat;
    
    ignore_blanks();

    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
      
    if (cur_dir_level < (kMaxDirLevels-1)) {
      // Build a complete path to the directory we want to "enter"
      build_expanded_filename(dirname);
      // See if it exists
      if ( SD.exists( expanded_file_name )) {
        // Add this filename to our array of directory names
        while (*(dirname + i) != '\0') {
          dir_array[cur_dir_level][i] = *(dirname+ i);
          i++;
        }
        dir_array[cur_dir_level][i] = '\0';
        cur_dir_level++;
      } else {
        printmsg( sdfilemsg );
        goto warmstart;
      }
    } else {
      printmsg( sdnofilemsg );
      goto warmstart;
    }
  }
  goto run_next_statement;

up_dir:
  ignore_blanks();

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
    goto qwhat;
  
  // Move out of current directory
  if (cur_dir_level > 0) cur_dir_level--;
  goto run_next_statement;
  
rseed:
  {
    short int value;

    //Get the pin number
    expression_error = 0;
    value = expression();
    if (expression_error)
      goto qwhat;

    // Check that we are at the end of the statement
    if (*txtpos != NL && *txtpos != ':')
      goto qwhat;
    
    randomSeed( value );
  }
  goto run_next_statement;

drum:
  {
    short int e1 = 0;
    short int e2 = 0;

    // Get index
    expression_error = 0;
    e1 = expression();
    if (expression_error)
      goto qwhat;

    if (*txtpos != ',') {
      goto qwhat;
    }

    // Get argument
    txtpos++;
    e2 = expression();
    if (expression_error)
      goto qwhat;

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;

    // Execute command
    switch (e1) {
      case 0: // Trigger
        if (e2 != 0) {
          drum1.noteOn();
        }
        break;
      case 1: // Frequency
        if (e2 > 0) {
          drum1.frequency(e2);
        }
        break;
      case 2: // Amplitude
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        mixer1.gain(0, ((float) e2) / 100.0);
        break;
      case 3: // Length
        if (e2 < 0) e2 = 0;
        drum1.length(e2);
        break;
      case 4: // SecondMix
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        drum1.secondMix(((float) e2) / 100.0);
        break;
      case 5: // PitchMod
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        drum1.pitchMod(((float) e2) / 100.0);
        break;
      default:
        goto qsounderr;
    }
    
  }
  goto run_next_statement;

note:
  {
    short int e1 = 0;
    short int e2 = 0;

    // Get index
    expression_error = 0;
    e1 = expression();
    if (expression_error)
      goto qwhat;

    if (*txtpos != ',') {
      goto qwhat;
    }

    // Get argument
    txtpos++;
    e2 = expression();
    if (expression_error)
      goto qwhat;

    // Make sure we're at the end of the line
    if ((*txtpos != NL) && (*txtpos != ':'))
      goto qwhat;

    // Execute command
    switch (e1) {
      case 0: // Trigger
        if (e2 != 0) {
          waveform1.begin(note_type);
          waveform1.amplitude(1.0);
          envelope1.noteOn();
          note_playing = true;
          Timer1.setPeriod(note_duration_msec * 1000);
          Timer1.start();
        } else {
          end_note();
        }
        break;
      case 1: // Frequency
        if (e2 > 0) {
          waveform1.frequency(e2);
        }
        break;
      case 2: // Amplitude
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        mixer1.gain(1, ((float) e2) / 100.0);
        break;
      case 3: // Length
        if (e2 < 0) e2 = 0;
        note_duration_msec = e2;
        break;
      case 4: // Waveform
        switch (e2) {
          case 1: note_type = WAVEFORM_SAWTOOTH; break;
          case 2: note_type = WAVEFORM_SQUARE; break;
          case 3: note_type = WAVEFORM_TRIANGLE; break;
          case 4: note_type = WAVEFORM_PULSE; break;
          case 5: note_type = WAVEFORM_SAWTOOTH_REVERSE; break;
          default: note_type = WAVEFORM_SINE; break;
        }
      case 5: // PulseWidth
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        waveform1.pulseWidth(((float) e2) / 100.0);
        break;
      case 6: // Attack
        if (e2 < 0) e2 = 0;
        envelope1.attack(e2);
        break;
      case 7: // Hold
        if (e2 < 0) e2 = 0;
        envelope1.hold(e2);
        break;
      case 8: // Decay
        if (e2 < 0) e2 = 0;
        envelope1.decay(e2);
        break;
      case 9: // Sustain
        if (e2 < 0) e2 = 0;
        if (e2 > 100) e2 = 100;
        envelope1.sustain(((float) e2) / 100.0);
        break;
      case 10: // Release
        if (e2 < 0) e2 = 0;
        envelope1.release(e2);
        break;
      default:
        goto qsounderr;
    }
  }
  goto run_next_statement;

play_wav:
  {
    short int e1 = 0;
    short int e2 = 0;
    char *filename;

    // Get index
    expression_error = 0;
    e1 = expression();
    if (expression_error)
      goto qwhat;
    
    if (*txtpos != ',') {
      goto qwhat;
    }
    txtpos++;

    // Get Argument
    if (e1 == 1) {
      // Work out the filename
      ignore_blanks();
      filename = filenameWord();
      if (expression_error)
        goto qwhat;

      // Make sure we're at the end of the line
      ignore_blanks();
      if ((*txtpos != NL) && (*txtpos != ':'))
        goto qwhat;

      // Create the full directory path in expanded_file_name
      build_expanded_filename(filename);

      // Save the the complete filename to our expanded_wavfile_name variable if the file exists
      if ( SD.exists( expanded_file_name )) {
        memcpy(expanded_wavfile_name, expanded_file_name, strlen(expanded_file_name)+1);
      } else {
        goto qsoundfileerr;
      }
    } else {
      // Get numeric argument
      expression_error = 0;
      e2 = expression();
      if (expression_error)
        goto qwhat;

      // Make sure we're at the end of the line
      if ((*txtpos != NL) && (*txtpos != ':'))
        goto qwhat;

      // Execute command
      switch (e1) {
        case 0: // Trigger
          if (e2 != 0) {
            if ( SD.exists( expanded_wavfile_name )) {
              if (!playSdWav1.play(expanded_wavfile_name)) {
                goto qsoundfileerr;
              }
            }
          } else {
            playSdWav1.stop();
          }
          break;
        case 2: // Amplitude
          if (e2 < 0) e2 = 0;
          if (e2 > 100) e2 = 100;
          mixer1.gain(2, ((float) e2) / 200.0);
          mixer1.gain(3, ((float) e2) / 200.0);
          break;
        default:
         goto qsounderr;
      }
    }
  }
  goto run_next_statement;


print_help:
  {
    // Print keywords
    printmsgNoNL(helpKeywordMsg);
    print_table(keywords);
    line_terminator();
    printmsgNoNL(helpFuncMsg);
    print_table(func_tab);
    line_terminator();
    printmsgNoNL(helpMathMsg);
    print_table(mathop_tab);
    line_terminator();
    printmsgNoNL(helpRelopMsg);
    print_table(relop_tab);
    line_terminator();
    printmsgNoNL(helpLogicLvlMsg);
    print_table(highlow_tab);
    line_terminator();
  }
  goto run_next_statement;
}

/***************************************************************************/
static void line_terminator(void)
{
  outchar(NL);
  outchar(CR);
}

/***************************************************************************/
void tb_setup()
{
  output_mask = OUTPUT_MASK_SCREEN;
  input_mask = TB_INPUT_MASK_KEYBOARD;
  printmsg(initmsg);
#ifdef DEMO_MODE
  printmsg(demomsg);
#endif

  initSD();
  cur_dir_level = 0;

  init_audio();

#ifdef ENABLE_AUTORUN
  if ( SD.exists( kAutorunFilename )) {
    program_end = program_start;
    fp = SD.open( kAutorunFilename );
    inStream = kStreamFile;
    inhibitOutput = true;
    runAfterLoad = true;
  }
#endif /* ENABLE_AUTORUN */

#ifdef DEMO_MODE
  tb_sawUserActivity = false;
#endif
}

void tb_setup_for_virtual_host()
{
  output_mask = OUTPUT_MASK_SCREEN;
  input_mask = TB_INPUT_MASK_KEYBOARD;
}


/***************************************************************************/
static unsigned char breakcheck(void)
{
  char c;

  if (TB_TX_AVAIL()) {
    c = PEEK_TB_TX();
    POP_TB_TX();
    return (c == CTRLC);
  }
  return 0;
}

/***************************************************************************/
static int inchar()
{
  int v;
  char c;

  switch ( inStream ) {
    case ( kStreamFile ):
      v = fp.read();
      if ( v == NL ) v = CR; // file translate
      if ( !fp.available() ) {
        fp.close();
        goto inchar_loadfinish;
      }
      return v;
      break;

    case ( kStreamSerial ):
    default:
      while (1) {
        if (TB_TX_AVAIL()) {
          c = PEEK_TB_TX();
          POP_TB_TX();
#ifdef DEMO_MODE
          tb_lastKeyPressT = millis();
          tb_sawUserActivity = true;
#endif
          return (c);
        }
#ifdef DEMO_MODE
        else {
          if (tb_sawUserActivity) {
            if (InactivityTimeoutMsec(tb_lastKeyPressT, tb_timeoutSecs * 1000)) {
              ResetTeensy();
            }
          }
        }
#endif

        delay(1);  // necessary or this code will never execute (optimized away somehow?)
      }
  }

inchar_loadfinish:
  inStream = kStreamSerial;
  inhibitOutput = false;

  if ( runAfterLoad ) {
    runAfterLoad = false;
    triggerRun = true;
  }
  return NL; // trigger a prompt.
}

/***********************************************************/
static void outchar(unsigned char c)
{
  if ( inhibitOutput ) return;

  if ( outStream == kStreamFile ) {
    // output to a file
    fp.write( c );
  }

  if ( outStream == kStreamSerial) {
    // Only push when there's room
    if ((output_mask & OUTPUT_MASK_SCREEN) == OUTPUT_MASK_SCREEN) {
      while (TB_RX_FULL()) {
        delay(1);
      }
      PUSH_TB_RX(c);
    }
    if ((output_mask & OUTPUT_MASK_USB_HOST) == OUTPUT_MASK_USB_HOST) {
      while (HOST_TX1_FULL()) {
        delay(1);
      }
      PUSH_HOST_TX1(c);
    }
    if ((output_mask & OUTPUT_MASK_SER_HOST) == OUTPUT_MASK_SER_HOST) {
      while (HOST_TX2_FULL()) {
        delay(1);
      }
      PUSH_HOST_TX2(c);
    }
    if ((output_mask & OUTPUT_MASK_PRINTER) == OUTPUT_MASK_PRINTER) {
      while (PRT_TX_FULL()) {
        delay(1);
      }
      PUSH_PRT_TX(c);
    }
  }
}

/***************************************************************************/
// returns input_mask for the terminal program
static int GetInputMask() {
  return input_mask;
}

/***************************************************************************/
// returns 1 if the character is valid in a filename
static int isValidFnChar( char c )
{
  if ( c >= '0' && c <= '9' ) return 1; // number
  if ( c >= 'A' && c <= 'Z' ) return 1; // LETTER
  if ( c >= 'a' && c <= 'z' ) return 1; // letter (for completeness)
  if ( c == '_' ) return 1;
  if ( c == '+' ) return 1;
  if ( c == '.' ) return 1;
  if ( c == '~' ) return 1; // Window~1.txt

  return 0;
}

char * filenameWord(void)
{
  int i, j;
  char c;
  int cstep;   // 0 = start, 1 = collecting filename chars, 2 = ignoring extra filename chars,
               // 3 = collecting filetype characters, 4 = ignoring extra filetype characters

  expression_error = 0;

  // Search for valid filename characters
  cstep = 0;
  i = 0;
  j = 0;
  while (isValidFnChar(c = *txtpos)) {
    switch (cstep) {
      case 0: // Start building filename chars
        cur_file_name[i++] = c;
        cstep = 1;
        break;
      case 1: // Building filename chars
        if (c == '.') {
          cur_file_name[i++] = c;
          cstep = 3;
        } else {
          if (i < 8) {
            cur_file_name[i++] = c;
          } else {
            cstep = 2;
          }
        }
        break;
      case 2: // Skipping filename chars
        if (c == '.') {
          cur_file_name[i++] = c;
          cstep = 3;
        }
        break;
      case 3: // Collecting filetype chars
        if (j++ < 3) {
          cur_file_name[i++] = c;
        } else {
          cstep = 4;
        }
        break;
      case 4: // ignoring filetype chars
        break;
    }

    // Move to next character
    txtpos++;
  }
  
  if (cstep == 0) {
    cur_file_name[0] = '\0';
    expression_error = 1;
  } else {
    // Add a final nul to terminate the string
    cur_file_name[i] = '\0';
  }

  return cur_file_name;
}

/***************************************************************************/
void build_expanded_filename(char * fn)
{
  int i;
  char *cp;
  char *efnp = expanded_file_name;

  // Copy directory names into the expanded file name
  for (i = 0; i<cur_dir_level; i++) {
    cp = dir_array[i];
    while (*cp != '\0') {
      *efnp++ = *cp++;
    }
    *efnp++ = '/';
  }

  // Copy filename into the expanded file name
  while (*fn != '\0') {
    *efnp++ = *fn++;
  }
  *efnp = '\0';   // Final null
}

/***************************************************************************/
static int initSD( void )
{
  // if the card is already initialized, we just go with it.
  // there is no support (yet?) for hot-swap of SD Cards. if you need to
  // swap, pop the card, reset the arduino.)

  if ( sd_is_initialized == true ) return kSD_OK;

  // due to the way the SD Library works, pin 10 always needs to be
  // an output, even when your shield uses another line for CS
  //pinMode(10, OUTPUT); // change this to 53 on a mega

  if ( !SD.begin( kSD_CS )) {
    // failed
    printmsg( sderrormsg );
    return kSD_Fail;
  }
  // success - quietly return 0
  sd_is_initialized = true;

  // and our file redirection flags
  outStream = kStreamSerial;
  inStream = kStreamSerial;
  inhibitOutput = false;

  return kSD_OK;
}

void cmd_Files( void )
{
  //File dir = SD.open( "/" );
  build_expanded_filename((char *) "\0");   // Get the current directory
  File dir = SD.open( expanded_file_name );
  dir.seek(0);

  // Print directory
  printmsgNoNL( (const unsigned char *) expanded_file_name );
  line_terminator();
  
  while ( true ) {
    File entry = dir.openNextFile();
    if ( !entry ) {
      entry.close();
      break;
    }

    // common header
    printmsgNoNL( indentmsg );
    printmsgNoNL( (const unsigned char *)entry.name() );

    if ( entry.isDirectory() ) {
      printmsgNoNL( slashmsg );
    }

    if ( entry.isDirectory() ) {
      // directory ending
      for ( int i = strlen( entry.name()) ; i < 16 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printmsgNoNL( dirextmsg );
    }
    else {
      // file ending
      for ( int i = strlen( entry.name()) ; i < 17 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printUnum( entry.size() );
    }
    line_terminator();
    entry.close();
  }
  dir.close();
}


/***************************************************************************/
// Audio Support
//
void init_audio()
{
  // Stop any ongoing audio in case we are be called for a reboot
  end_audio();

  AudioNoInterrupts();

  AudioMemory(6);
  
  // Initialize the audio objects with default values
  mixer1.gain(0, 1.0);
  mixer1.gain(1, 1.0);
  mixer1.gain(2, 0.5);  // sd playback stereo signals each contribute half
  mixer1.gain(3, 0.5);

  drum1.frequency(550);
  drum1.length(750);
  drum1.secondMix(0.0);
  drum1.pitchMod(0.5);

  note_type = WAVEFORM_SINE;
  waveform1.frequency(440);
  waveform1.amplitude(0.0);
  waveform1.pulseWidth(0.5);

  envelope1.attack(10);
  envelope1.hold(1);
  envelope1.decay(15);
  envelope1.sustain(0.5);
  envelope1.release(30);

  note_duration_msec = 750;

  expanded_wavfile_name[0] = '\0';

  AudioInterrupts();

  Timer1.stop();
  Timer1.attachInterrupt(timer1_isr);
}

void end_audio()
{
  playSdWav1.stop();
  envelope1.noteOff();
  waveform1.amplitude(0.0);
  Timer1.stop();
}

void end_note()
{
  // Stop note playback
  envelope1.noteOff();
  note_playing = false;

  // Disable timer
  Timer1.stop();
}

void timer1_isr()
{
  end_note();
}

void print_audio_statistics()
{
  Serial.print("Audio Statistics: ");
  Serial.print(AudioProcessorUsageMax());
  Serial.print(" ");
  Serial.println(AudioMemoryUsageMax());
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
}

