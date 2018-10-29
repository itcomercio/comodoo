#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/**
 * Executes the command "grep Villanova < scores > out".
 *
 * @author Jim Glenn
 * @version 0.1 9/23/2004
 */

int main(int argc, char **argv)
{
  int in, out, pid, status;

  /*
  char *dialog_args[] = {"python",
      "-c", 
      "from dialog import Dialog; d = Dialog(dialog='dialog');d.msgbox('HOLA')"};
  */

  char *dialog_args[] = {"python", "/bin/check.py"};

  // open input and output files
  //in = open("/dev/tty", O_RDONLY);
  //out = open("/dev/tty", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);

  // replace standard input with input file
  //dup2(in, 0);

  // replace standard output with output file
  //dup2(out, 1);

  // close unused file descriptors
  //close(in);
  //close(out);

  if (!(pid = fork())) {
    execvp("python", dialog_args);
  }

  waitpid(pid, &status, 0);
}
