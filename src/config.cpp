#include "config.h"
#include "controller.h"

#include "../chunkwm/src/api/plugin_api.h"

#include "../chunkwm/src/common/config/tokenize.h"
#include "../chunkwm/src/common/config/cvar.h"
// #include "../chunkwm/src/common/misc/assert.h"

#include "constants.h"
#include "misc.h"

#include <CoreFoundation/CFString.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define local_persist static

inline char **
BuildArguments(const char *Message, int *Count)
{
    char **Args = (char **) malloc(16 * sizeof(char *));
    *Count = 1;

    while (*Message) {
        token ArgToken = GetToken(&Message);
        char *Arg = TokenToString(ArgToken);
        Args[(*Count)++] = Arg;
    }

    return Args;
}

inline void
FreeArguments(int Count, char **Args)
{
    for (int Index = 1; Index < Count; ++Index) {
        free(Args[Index]);
    }
    free(Args);
}

struct command
{
    char Flag;
    char *Arg;
    struct command *Next;
};

inline void
FreeCommandChain(command *Chain)
{
    command *Command = Chain->Next;
    while (Command) {
        command *Next = Command->Next;
        free(Command->Arg);
        free(Command);
        Command = Next;
    }
}

inline command *
ConstructCommand(char Flag, char *Arg)
{
    command *Command = (command *) malloc(sizeof(command));

    Command->Flag = Flag;
    Command->Arg = Arg ? strdup(Arg) : NULL;
    Command->Next = NULL;

    return Command;
}

typedef void (*query_func)(char *, int);
typedef void (*command_func)(char *);

command_func WindowCommandDispatch(char Flag)
{
    switch (Flag) {
    //case 'a': return AllCycle;          break;
    //case 'f': return FloatCycle;        break;
    case 'm': return MoveWindow;     break;
    case 'i': return IncWindow;      break;
    case 'd': return DecWindow;      break;
    case 's': return TemporaryStep;     break;
    //case 'p': return PresetWindow;      break;

    // NOTE(koekeishiya): silence compiler warning.
    default: return 0; break;
    }
}

inline bool
ParseWindowCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "d:i:s:m:";

    struct option Long[] = {
        { "inc", required_argument, NULL, 'i' },
        { "dec", required_argument, NULL, 'd' },
        { "move", required_argument, NULL, 'm' },
        { "step", required_argument, NULL, 's' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
            case 'd':
            case 'i':
            case 'm': {
                if ((StringEquals(optarg, "west")) ||
                    (StringEquals(optarg, "east")) ||
                    (StringEquals(optarg, "north")) ||
                    (StringEquals(optarg, "south"))) {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                } else {
                    c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 's': {
                float Step;
                if (sscanf(optarg, "%f", &Step) == 1) {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                } else {
                    c_log(C_LOG_LEVEL_WARN, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?': {
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            } break;
        }
    }

End:
    // reset getopt
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

query_func QueryCommandDispatch(char Flag)
{
    switch (Flag) {
    case 'p': return QueryWindowCoord;   break;

    // maybe should make these %screen res-based measurements?
    // include an "index" of floating windows also

    default: return 0; break;
    }
}

inline bool
ParseQueryCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "p:";

    struct option Long[] = {
        { "position", required_argument, NULL, 'p' },
        { NULL, 0, NULL, 0 }
    };

    command *Command = Chain;
    while ((Option = getopt_long(Count, Args, Short, Long, NULL)) != -1) {
        switch (Option) {
            case 'p': {
                command *Entry = ConstructCommand(Option, optarg);
                Command->Next = Entry;
                Command = Entry;
            } break;
            case '?': {
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            } break;
        }
    }

End:
    // reset getopt
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

void CommandCallback(int SockFD, const char *Type, const char *Message)
{
    if (StringEquals(Type, "query")) {
        command Chain = {};
        bool Success = ParseQueryCommand(Message, &Chain);
        if (Success) {
            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_WARN, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*QueryCommandDispatch(Command->Flag))(Command->Arg, SockFD);
            }

            FreeCommandChain(&Chain);
        }
    } else if (StringEquals(Type, "window")) {
        command Chain = {};
        bool Success = ParseWindowCommand(Message, &Chain);
        if (Success) {
            // restore original step sizes
            float move_step = CVarFloatingPointValue(CVAR_FLOAT_MOVE);
            float resize_step = CVarFloatingPointValue(CVAR_FLOAT_RESIZE);

            command *Command = &Chain;
            while ((Command = Command->Next)) {
                c_log(C_LOG_LEVEL_WARN, "    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);
                (*WindowCommandDispatch(Command->Flag))(Command->Arg);
            }

            if (move_step != CVarFloatingPointValue(CVAR_FLOAT_MOVE)) {
                UpdateCVar(CVAR_FLOAT_MOVE, move_step);
            }

            if (resize_step != CVarFloatingPointValue(CVAR_FLOAT_RESIZE)) {
                UpdateCVar(CVAR_FLOAT_RESIZE, resize_step);
            }
        }
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-float: no match for '%s %s'\n", Type, Message);
    }
}
