/* ******************************************************************************************
 * Switch module - Set initial operating state, debounce switch input and detect operating
 * mode changes and initiate a reset when seen.
 * ******************************************************************************************/

// -----------------------------------------------------------------------------------
// MODULE CONSTANTS

// Debounce time in uSec
#define kSwitchDebounce   20000


// -----------------------------------------------------------------------------------
// MODULE VARIABLES

// Last debounce sample
unsigned long switchPrevT;

// Switch state
int prevSwitchSmpl;
int curSwitchVal;


// -----------------------------------------------------------------------------------
// MODULE API ROUTINES

void SwitchInit() {
  // Configure our pin
  pinMode(SWITCH_IN, INPUT);

  // Get an initial value
  curSwitchVal = (digitalRead(SWITCH_IN) == HIGH) ? 1 : 0;
  prevSwitchSmpl = curSwitchVal;

  // Get an initial time stamp
  switchPrevT = micros();
}


void SwitchEval() {
  int smpl;
  
  if (InactivityTimeout(switchPrevT, kSwitchDebounce)) {
    switchPrevT = micros();
    smpl = (digitalRead(SWITCH_IN) == HIGH) ? 1 : 0;

    // Need at least two identical switch samples before checking
    if (smpl == prevSwitchSmpl) {
      if (smpl != curSwitchVal) {
        // Detected a change - reboot into new mode
        ResetTeensy();
      }
    }
    prevSwitchSmpl = smpl;
  }
}


boolean SwitchIsTerminalMode() {
  return(curSwitchVal == 1);
}

