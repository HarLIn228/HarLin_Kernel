#ifndef SHELL_H
#define SHELL_H

struct shell_command {
    const char* name;
    int (*handler)(int argc, char** argv);
};

void shell_run(void);
int  Harlin_ShellRegister(const struct shell_command* cmd);

#endif
