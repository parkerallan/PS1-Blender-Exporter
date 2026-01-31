#ifndef INPUT_H
#define INPUT_H

#include <sys/types.h>
#include <libetc.h>

// Controller state
extern u_long padState;
extern u_long padStateOld;

// Initialize controller
void initController(void);

// Handle input (updates camera and animation)
void handleInput(void);

#endif // INPUT_H
