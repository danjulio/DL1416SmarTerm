/* ******************************************************************************************
 * Menu - Code for handling configuration menu display and character processing.  This code
 * operates as a stand-alone function (terminal code is not running) and exits with a soft
 * reset to restart normal operation.  It can change persistent storage configuration items
 * which are re-read when the system reboots.
 * 
 * Display Format
 * 
 * 
 *                                          ChangedCol
 *    SelectorCol      ItemCol              | ValueCol
 *       v             v                    vv
 *       >             "ItemText1"          *ItemVal1
 *                     "ItemText2"           ItemVal2
 *                     ...
 *                     "ItemTextN"           ItemValN
 * 
 * Key Handling
 *   SO - Save any changed configuration items and reboot
 *   FF - Move selector down one position (wrap)
 *   VT - Move selector up one position (wrap)
 *   SPACE - Change selected item to next val (wrap through list)
 *   
 *   All other keys are ignored.
 *    
 * ******************************************************************************************/

// -----------------------------------------------------------------------------------
// MODULE CONSTANTS

// Column positions - make sure that there is enough room between kItemCol and kValueCol
// for the longest ItemText string
#define kHeaderCol       4
#define kSelectorCol     4
#define kItemCol         8
#define kValueCol        32
#define kItemChangedCol  kValueCol-1

// Row positions
#define kHeaderRow       1
#define kMenuStartRow    3

// Row selection indicator
#define kSelector        '>'

// Changed selection indicator
#define kChanged         '*'


static const char headerText[] PROGMEM = "TERMINAL CONFIGURATION MENU    FW: " kTermVersion;



// -----------------------------------------------------------------------------------
// MENU CONTENTS

#define kNumMenuItems  8

static const char itemText1[] PROGMEM = "HOST IF BAUD";
static const char itemText2[] PROGMEM = "PRINTER BAUD";
static const char itemText3[] PROGMEM = "LOCAL ECHO";
static const char itemText4[] PROGMEM = "FLOW CONTROL";
static const char itemText5[] PROGMEM = "CURSOR TYPE";
static const char itemText6[] PROGMEM = "DEFAULT TAB WIDTH";
static const char itemText7[] PROGMEM = "CR/LF HANDLING";
static const char itemText8[] PROGMEM = "LINE WRAP";

static const char* itemTextList[] PROGMEM = {
  itemText1, itemText2, itemText3, itemText4, itemText5, itemText6, itemText7, itemText8
};


// Note: itemValText strings must all be the same length to overwrite each other correctly (use trailing spaces)
// Menu Item 1 value
#define kNumMenu1Items 4
static const char item1ValText1[] PROGMEM = "1200 ";
static const char item1ValText2[] PROGMEM = "2400 ";
static const char item1ValText3[] PROGMEM = "9600 ";
static const char item1ValText4[] PROGMEM = "19200";

static const char* item1ValTextList[] PROGMEM = {
  item1ValText1, item1ValText2, item1ValText3, item1ValText4
};

static const int item1ValList[] PROGMEM = {1200, 2400, 9600, 19200};


// Menu Item 2 value
#define kNumMenu2Items 4
static const char item2ValText1[] PROGMEM = "1200 ";
static const char item2ValText2[] PROGMEM = "2400 ";
static const char item2ValText3[] PROGMEM = "9600 ";
static const char item2ValText4[] PROGMEM = "19200";

static const char* item2ValTextList[] PROGMEM = {
  item2ValText1, item2ValText2, item2ValText3, item2ValText4
};

static const int item2ValList[] PROGMEM = {1200, 2400, 9600, 19200};


// Menu Item 3 value
#define kNumMenu3Items 2
static const char item3ValText1[] PROGMEM = "ON ";
static const char item3ValText2[] PROGMEM = "OFF";

static const char* item3ValTextList[] PROGMEM = {
  item3ValText1, item3ValText2
};

static const int item3ValList[] = {1, 0};


// Menu Item 4 value
#define kNumMenu4Items 2
static const char item4ValText1[] PROGMEM = "ON ";
static const char item4ValText2[] PROGMEM = "OFF";

static const char* item4ValTextList[] PROGMEM = {
  item4ValText1, item4ValText2
};

static const int item4ValList[] = {1, 0};


// Menu Item 5 value
#define kNumMenu5Items 2
static const char item5ValText1[] PROGMEM = "BLINK";
static const char item5ValText2[] PROGMEM = "SOLID";

static const char* item5ValTextList[] PROGMEM = {
  item5ValText1, item5ValText2
};

static const int item5ValList[] = {1, 0};


// Menu Item 6 value
#define kNumMenu6Items 4
static const char item6ValText1[] PROGMEM = "1";
static const char item6ValText2[] PROGMEM = "2";
static const char item6ValText3[] PROGMEM = "4";
static const char item6ValText4[] PROGMEM = "8";

static const char* item6ValTextList[] PROGMEM = {
  item6ValText1, item6ValText2, item6ValText3, item6ValText4
};

static const int item6ValList[] = {1, 2, 4, 8};


// Menu Item 7 value
#define kNumMenu7Items 2
static const char item7ValText1[] PROGMEM = "CR+LF     ";
static const char item7ValText2[] PROGMEM = "INDIVIDUAL";

static const char* item7ValTextList[] PROGMEM = {
  item7ValText1, item7ValText2
};

static const int item7ValList[] = {1, 0};


// Menu Item 8 value
#define kNumMenu8Items 2
static const char item8ValText1[] PROGMEM = "ON ";
static const char item8ValText2[] PROGMEM = "OFF";

static const char* item8ValTextList[] PROGMEM = {
  item8ValText1, item8ValText2
};

static const int item8ValList[] = {1, 0};



struct menuEntry {
  int numItems;
  const char** itemTextAPtr;
  const int* itemValAPtr;
  int curSel;
  int curVal;
  int newVal;
  boolean changed;
};



// -----------------------------------------------------------------------------------
// MODULE VARIABLES

struct menuEntry menuEntryArray[kNumMenuItems];
int curItem;


// -----------------------------------------------------------------------------------
// MODULE API ROUTINES

void MenuInit() {
  // Initialize our data structure
  MenuInitEntryArray();
  curItem = 0;
  
  // Clear the display and disable the cursor
  DL1416_DIS_CUR();
  DL1416_CLS();

  // Draw the menu
  MenuDraw();
}


void MenuEval() {
  char c;
  
  // Process incoming keys
  if (Serial2.available()) {
    c = Serial2.read();

    switch (c) {
      case SI:
        MenuUpdatePersistentStorage();
        delay(100); // Allow for any pending eeprom writes to finish
        ResetTeensy();
        break;
      case FF:
        MenuIncSelector();
        break;
      case VT:
        MenuDecSelector();
        break;
      case SPACE:
        MenuIncSelection();
        break;
    }
  }

  // Process output to the display
  if (dl1416Num != 0) {
    if (InactivityTimeout(dl1416PrevT, kDL1416delay)) {
      c = PEEK_DL1416();
      POP_DL1416();
      Serial2.write(c);
      dl1416PrevT = micros();
    }
  }
}


// -----------------------------------------------------------------------------------
// MODULE INTERNAL ROUTINES

// This routine must be customized to specific menu items
void MenuInitEntryArray() {
  int n, t;

  // Item 1: Host IF Baud
  t = PsGetHostBaud();
  n = MenuFindItemFromValue(t, kNumMenu1Items, item1ValList);
  menuEntryArray[0].numItems = kNumMenu1Items;
  menuEntryArray[0].itemTextAPtr = item1ValTextList;
  menuEntryArray[0].itemValAPtr = item1ValList;
  if (n >= 0) {
    menuEntryArray[0].curSel = n;
  } else {
    menuEntryArray[0].curSel = kNumMenu1Items-1;  // Force first SPACE to change value to first legal value
  }
  menuEntryArray[0].curVal = t;
  menuEntryArray[0].newVal = t;
  menuEntryArray[0].changed = false;

  
  // Item 2: Printer Baud
  t = PsGetPrtBaud();
  n = MenuFindItemFromValue(t, kNumMenu2Items, item2ValList);
  menuEntryArray[1].numItems = kNumMenu2Items;
  menuEntryArray[1].itemTextAPtr = item2ValTextList;
  menuEntryArray[1].itemValAPtr = item2ValList;
  if (n >= 0) {
    menuEntryArray[1].curSel = n;
  } else {
    menuEntryArray[1].curSel = kNumMenu2Items-1;
  }
  menuEntryArray[1].curVal = t;
  menuEntryArray[1].newVal = t;
  menuEntryArray[1].changed = false;
  
  // Item 3: Local Echo
  t = PsGetTermLocalEcho() ? 1 : 0;
  n = MenuFindItemFromValue(t, kNumMenu3Items, item3ValList);
  menuEntryArray[2].numItems = kNumMenu3Items;
  menuEntryArray[2].itemTextAPtr = item3ValTextList;
  menuEntryArray[2].itemValAPtr = item3ValList;
  if (n >= 0) {
    menuEntryArray[2].curSel = n;
  } else {
    menuEntryArray[2].curSel = kNumMenu3Items-1;
  }
  menuEntryArray[2].curVal = t;
  menuEntryArray[2].newVal = t;
  menuEntryArray[2].changed = false;
  
  // Item 4: Flow Control
  t = PsGetHostFlowControl() ? 1 : 0;
  n = MenuFindItemFromValue(t, kNumMenu4Items, item4ValList);
  menuEntryArray[3].numItems = kNumMenu4Items;
  menuEntryArray[3].itemTextAPtr = item4ValTextList;
  menuEntryArray[3].itemValAPtr = item4ValList;
  if (n >= 0) {
    menuEntryArray[3].curSel = n;
  } else {
    menuEntryArray[3].curSel = kNumMenu4Items-1;
  }
  menuEntryArray[3].curVal = t;
  menuEntryArray[3].newVal = t;
  menuEntryArray[3].changed = false;
  
  // Item 5: Cursor Type
  t = PsGetCursorBlink() ? 1 : 0;
  n = MenuFindItemFromValue(t, kNumMenu5Items, item5ValList);
  menuEntryArray[4].numItems = kNumMenu5Items;
  menuEntryArray[4].itemTextAPtr = item5ValTextList;
  menuEntryArray[4].itemValAPtr = item5ValList;
  if (n >= 0) {
    menuEntryArray[4].curSel = n;
  } else {
    menuEntryArray[4].curSel = kNumMenu5Items-1;
  }
  menuEntryArray[4].curVal = t;
  menuEntryArray[4].newVal = t;
  menuEntryArray[4].changed = false;
  
  // Item 6: Tab Width
  t = PsGetTabWidth();
  n = MenuFindItemFromValue(t, kNumMenu6Items, item6ValList);
  menuEntryArray[5].numItems = kNumMenu6Items;
  menuEntryArray[5].itemTextAPtr = item6ValTextList;
  menuEntryArray[5].itemValAPtr = item6ValList;
  if (n >= 0) {
    menuEntryArray[5].curSel = n;
  } else {
    menuEntryArray[5].curSel = kNumMenu6Items-1;
  }
  menuEntryArray[5].curVal = t;
  menuEntryArray[5].newVal = t;
  menuEntryArray[5].changed = false;
  
  // Item 7: CR/LF Handling
  t = PsGetConvertCRLF() ? 1 : 0;
  n = MenuFindItemFromValue(t, kNumMenu7Items, item7ValList);
  menuEntryArray[6].numItems = kNumMenu7Items;
  menuEntryArray[6].itemTextAPtr = item7ValTextList;
  menuEntryArray[6].itemValAPtr = item7ValList;
  if (n >= 0) {
    menuEntryArray[6].curSel = n;
  } else {
    menuEntryArray[6].curSel = kNumMenu7Items-1;
  }
  menuEntryArray[6].curVal = t;
  menuEntryArray[6].newVal = t;
  menuEntryArray[6].changed = false;
  
  // Item 8: Line Wrap
  t = PsGetLinewrap() ? 1 : 0;
  n = MenuFindItemFromValue(t, kNumMenu8Items, item8ValList);
  menuEntryArray[7].numItems = kNumMenu8Items;
  menuEntryArray[7].itemTextAPtr = item8ValTextList;
  menuEntryArray[7].itemValAPtr = item8ValList;
  if (n >= 0) {
    menuEntryArray[7].curSel = n;
  } else {
    menuEntryArray[7].curSel = kNumMenu8Items-1;
  }
  menuEntryArray[7].curVal = t;
  menuEntryArray[7].newVal = t;
  menuEntryArray[7].changed = false;
}


void MenuDraw() {
  int i;
  
  // Menu Page Title
  SetCursorPosition(kHeaderRow, kHeaderCol);
  MenuLoadString(headerText);

  // Menu Contents
  for (i=0; i<kNumMenuItems; i++) {
    // Menu Item
    SetCursorPosition(i + kMenuStartRow, kItemCol);
    MenuLoadString(itemTextList[i]);

    // Currently selected Item Value
    SetCursorPosition(i + kMenuStartRow, kValueCol);
    MenuLoadString(*(menuEntryArray[i].itemTextAPtr + menuEntryArray[i].curSel));
  }

  // First selector
  SetCursorPosition(kMenuStartRow + curItem, kSelectorCol);
  PUSH_DL1416(kSelector);
}


// This routine must be customized to specific menu items
void MenuUpdatePersistentStorage() {
  int i;

  for (i=0; i<kNumMenuItems; i++) {
    if (menuEntryArray[i].changed) {
      switch (i) {
        case 0:
          PsSetHostBaud(menuEntryArray[i].newVal);
          break;
        case 1:
          PsSetPrtBaud(menuEntryArray[i].newVal);
          break;
        case 2:
          PsSetTermLocalEcho(menuEntryArray[i].newVal == 1 ? true : false);
          break;
        case 3:
          PsSetHostFlowControl(menuEntryArray[i].newVal == 1 ? true : false);
          break;
        case 4:
          PsSetCursorBlink(menuEntryArray[i].newVal == 1 ? true : false);
          break;
        case 5:
          PsSetTabWidth(menuEntryArray[i].newVal);
          break;
        case 6:
          PsSetConvertCRLF(menuEntryArray[i].newVal == 1 ? true : false);
          break;
        case 7:
          PsSetLinewrap(menuEntryArray[i].newVal == 1 ? true : false);
          break;
      }
    }
  }
}


void MenuIncSelector() {
  // Erase the current selector indicator
  SetCursorPosition(kMenuStartRow + curItem, kSelectorCol);
  PUSH_DL1416(SPACE);

  // Increment
  if (++curItem == kNumMenuItems) curItem = 0;

  // Draw new selector indicator
  SetCursorPosition(kMenuStartRow + curItem, kSelectorCol);
  PUSH_DL1416(kSelector);
}


void MenuDecSelector() {
// Erase the current selector indicator
  SetCursorPosition(kMenuStartRow + curItem, kSelectorCol);
  PUSH_DL1416(SPACE);

  // Decrement
  if (curItem == 0) {
    curItem = kNumMenuItems-1;
  } else {
    --curItem;
  }

  // Draw new selector indicator
  SetCursorPosition(kMenuStartRow + curItem, kSelectorCol);
  PUSH_DL1416(kSelector);
}


void MenuIncSelection() {
  // Increment new value selection index
  menuEntryArray[curItem].curSel += 1;
  if (menuEntryArray[curItem].curSel == menuEntryArray[curItem].numItems) {
    menuEntryArray[curItem].curSel = 0;
  }

  // Update location with next value string
  SetCursorPosition(kMenuStartRow + curItem, kValueCol);
  MenuLoadString(*(menuEntryArray[curItem].itemTextAPtr + menuEntryArray[curItem].curSel));

  // Update the new value
  menuEntryArray[curItem].newVal = *(menuEntryArray[curItem].itemValAPtr + menuEntryArray[curItem].curSel);

  // Note any changes
  SetCursorPosition(kMenuStartRow + curItem, kItemChangedCol);
  if (menuEntryArray[curItem].curVal != menuEntryArray[curItem].newVal) {
    menuEntryArray[curItem].changed = true;
    PUSH_DL1416(kChanged);
  } else {
    menuEntryArray[curItem].changed = false;
    PUSH_DL1416(SPACE);
  }
}


void MenuLoadString(const char* cP) {
  while (*cP != NUL) {
    PUSH_DL1416(*cP++);
  }
}


int MenuFindItemFromValue(int val, int numItems, const int* itemList) {
  while (numItems-- > 0) {
    if (*(itemList+numItems) == val) {
      return(numItems);
    }
  }
  return(-1);
}

