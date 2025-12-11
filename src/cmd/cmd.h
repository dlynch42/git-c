#ifndef CMD_H
#define CMD_H

int init(void);
int catFile(int argc, char *argv[]);
int hashObject(int argc, char *argv[]);
int LSTree(int argc, char *argv[]);
char* writeTree(char *dirname);
int commitTree(int argc, char *argv[]);
int clone(int argc, char *argv[]);

#endif // CMD_H