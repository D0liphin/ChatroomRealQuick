#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "command.h"
#include "main.h"
#include "../include/panic.h"

#define STRCOMMAND_SETUSER ".setuser"

bool streq_withoutnul(char const *str1, char const *str2)
{
        for (size_t i = 0; str1[i] != '\0' && str2[i] != '\0'; ++i) {
                if (str1[i] != str2[i]) return false;
        }
        return true;
}

int select_command(char const *command, char const **args)
{
        if (streq_withoutnul(command, STRCOMMAND_SETUSER)) {
                *args = command + strlen(STRCOMMAND_SETUSER);
                return COMMAND_SETUSER;
        }
        *args = command;
        return COMMAND_SAY;
}

bool is_ascii_whitespace(char ch)
{
        return ch == ' ' || ch == '\t';
}

char const *skip_whitespace(char const *args)
{
        size_t i = 0;
        while (is_ascii_whitespace(args[i])) {
                i++;
        }
        return &args[i];
}

void command_say(struct sockclient const *client, char const *args)
{
        if (!client->name) {
                return;
        }
        printf(BLUE("%s:") " %s\n", client->name, args);
}

void command_setuser(struct sockclient *client, char const *args)
{
        free(client->name);
        char const *name = skip_whitespace(args);
        size_t name_buflen = strlen(name) + 1;
        client->name = (char *)malloc(name_buflen);
        memcpy(client->name, name, name_buflen);
}