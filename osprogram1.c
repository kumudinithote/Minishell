#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>

#define MAXNUMARGS 10

struct command {
  int type;
  char *argv[MAXNUMARGS];
};

struct suppCommand {
  int type;
  struct command *cmd;
  char *file;
  //can be use for pipe as required
  struct command *leftcmd;
  struct command *rightcmd;
};

struct command *parseCommand(char*);
struct command *parseSuppExecutable(char**, char*);
struct command *parseExecutable(char**, char*);

void runCommand(struct command *cmd)
{
  int p[2];                                       // p[0] and p[1] are the child process of the parents
  int redir;
  int pipeId;                                     // pipeID is the parent process
  struct command *ecmd;
  struct suppCommand *rcmd;

  if(cmd == 0)
    exit(0);

  switch(cmd->type){
  default:
    fprintf(stderr, "Command not found\n");
    exit(-1);

  case ' ':
    ecmd = (struct command*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);
    execvp(ecmd->argv[0], ecmd->argv);
    break;

  case '>':
    rcmd = (struct suppCommand*)cmd;
    freopen(rcmd->file, "w+", stdout);
    runCommand(rcmd->cmd);
    break;

  case '<':
    rcmd = (struct suppCommand*)cmd;
    freopen(rcmd->file, "r", stdin);
    runCommand(rcmd->cmd);
    break;

  case '|':
    rcmd = (struct suppCommand*)cmd;

    if (pipe(p) < 0)
    {
      fprintf(stderr, "pipe error, it has failed\n");
      exit(0);
    }
    int pid;

  	case 0: /* child */
      close(p[0]);
      dup2(p[1], STDOUT_FILENO);                    //duplicate the file
      runCommand(rcmd->leftcmd);                           //run the command to left
      close(p[1]);
      break;

  	default: /* parent */
      close(p[1]);
      dup2(p[0], STDIN_FILENO);                     //duplicate the file
      runCommand(rcmd->rightcmd);                          //run the command to right
      close(p[0]);
      break;
    }
    wait(&pipeId);
    break;
  }
  exit(0);
}

int checkType(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(" \t\r\n\v", *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

int isPipe(char* str)
{
    if (strchr(str, '|')){
			return 1; // returns zero if no pipe is found.
		}
    else {
      return 0;
    }
}

int tokenize(char **ps, char *es, char **q, char **eq)
{
  char spaces[] = " \t\r\n\v";
  char *temp;
  int ret;

  temp = *ps;
  while(temp < es && strchr(spaces, *temp))
    temp++;
  if(q)
    *q = temp;
  ret = *temp;

  if(*temp == 0){
    //do nothing
  }
  else if(*temp == '|' || *temp == '>' || *temp == '<')
    temp++;
  else{
    ret = '#';
    while(temp < es && !strchr(spaces, *temp) && !strchr("<|>", *temp))
      temp++;
  }

  if(eq)
    *eq = temp;

  while(temp < es && strchr(spaces, *temp))
    temp++;
  *ps = temp;
  return ret;
}

char *getCopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct command* parseCommand(char *s)
{
  char *es;
  struct command *cmd;
  es = s + strlen(s);
  cmd = parseSuppExecutable(&s, es);
  checkType(&s, es, "");
  return cmd;
}

struct command* parseSuppExecutable(char **ps, char *es)
{
  struct command *cmd;
  struct suppCommand *pipecmd;

  cmd = parseExecutable(ps, es);
  if(isPipe(*ps)){
    tokenize(ps, es, 0, 0);
    pipecmd = malloc(sizeof(*cmd));
    pipecmd->type = '|';
    pipecmd->leftcmd = cmd;
    pipecmd->rightcmd = parseSuppExecutable(ps, es);
    return (struct command*)pipecmd;
  }
  return cmd;
}

void changeDir(char *destination) // Function to change directory and check if its valid directory before changing
{
	DIR *mydir;
	char cwd[1024];
	strcpy(cwd, destination);
	mydir = opendir(destination);
	if(!mydir){
		perror("Not a file or directory");
		return;
	}
	chdir(destination);
	return;
}


int main(void)
{
  static char buf[100];
  int cycle;

  do{
    if (isatty(fileno(stdin)))
      fprintf(stdout, "myshell> ");
    fgets(buf, sizeof(buf), stdin);

    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      buf[strlen(buf)-1] = 0;
      changeDir(buf+3);
      continue;
    }
    if(buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't'){
      printf("GoodBye\n");
      return 0;
    }
    pid_t  pid;
    if ((pid = fork()) < 0) {
         printf("*** ERROR: forking child process failed\n");
         exit(1);
    }
    else if(pid == 0){
      runCommand(parseCommand(buf));
      exit(0);
    }
    wait(&cycle);

  }while(buf[0] != 0);

  exit(0);
}

struct command* parseredirs(struct command *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;
  struct suppCommand *suppCommand;

  while(checkType(ps, es, "<>")){
    tok = tokenize(ps, es, 0, 0);
    if(tokenize(ps, es, &q, &eq) != '#') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      suppCommand = malloc(sizeof(*cmd));
      suppCommand->type = '<';
      suppCommand->cmd = cmd;
      suppCommand->file = getCopy(q, eq);
      cmd = (struct command*)suppCommand;
      break;
    case '>':
      suppCommand = malloc(sizeof(*cmd));
      suppCommand->type = '>';
      suppCommand->cmd = cmd;
      suppCommand->file = getCopy(q, eq);
      cmd = (struct command*)suppCommand;
      break;
    }
  }
  return cmd;
}

struct command* parseExecutable(char **ps, char *es)
{
  char *q, *eq;
  int token, count;
  struct command *cmd;
  struct command *tempcmd;

  tempcmd = malloc(sizeof(*cmd));
  tempcmd->type = ' ';

  cmd = tempcmd;
  count = 0;
  tempcmd = parseredirs(tempcmd, ps, es);
  while(!checkType(ps, es, "|")){
    if((token=tokenize(ps, es, &q, &eq)) == 0)
      break;
    if(token != '#') {
      fprintf(stderr, "Not correct syntax\n");
      exit(-1);
    }
    cmd->argv[count] = getCopy(q, eq);
    count++;

    tempcmd = parseredirs(tempcmd, ps, es);
  }
  cmd->argv[count] = 0;
  return tempcmd;
}
