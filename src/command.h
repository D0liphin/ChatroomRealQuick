#pragma once

#include "main.h"

#define COMMAND_SAY 0
#define COMMAND_SETUSER 1

/** 
 * Get the command type for a given msg, and put the tail of the command 
 * (the args) in `args` 
 * 
 * # Returns 
 * - the command type
 */
int select_command(char const *command, char const **args);

/** -1 for errors */
void command_setuser(struct sockclient *client, char const *args);

void command_say(struct sockclient const *client, char const *args);