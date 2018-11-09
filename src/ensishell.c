/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include "variante.h"
#include "readcmd.h"
#include <errno.h>
#include <fcntl.h>


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

/*  Structure permettant de stocker la liste
 *  de processus qui ont été exécuté en Background
 */
struct CellulePid {
  int pid;
  int status;
  int state;
  char *commande;
  struct CellulePid *next;
};

typedef struct CellulePid CellulePid;
typedef CellulePid *ListePid;

ListePid ajouter(ListePid lp, int pid, char *commande) {
  CellulePid *nouveauPid =
      (struct CellulePid *)malloc(sizeof(struct CellulePid));
  char *instruction = malloc(sizeof(char));
  strcpy(instruction, commande);
  nouveauPid->commande = instruction;
  nouveauPid->pid = pid;
  nouveauPid->next = lp;
  lp = nouveauPid;
  return lp;
}

void afficher(ListePid liste) {
  if (liste == NULL) {
    printf("Liste vide\n");
  } else {
    ListePid temp = liste;
    while (temp != NULL) {
      pid_t etat;
      etat = waitpid(temp->pid, NULL, WNOHANG);
      if (etat) {
        printf("Pid: %d    Commande: %s    Statut: Terminé\n", temp->pid,
               temp->commande);
      } else {
        printf("Pid: %d    Commande: %s    Statut: En cours\n", temp->pid,
               temp->commande);
      }
      temp = temp->next;
    }
  }
}

int question6_executer(char *line) {
  /* Question 6: Insert your code to execute the command line
   * identically to the standard execution scheme:
   * parsecmd, then fork+execvp, for a single command.
   * pipe and i/o redirection are not required.
   */
  struct cmdline *l;
  l = parsecmd(&line);

  pid_t pid;
  switch (pid = fork()) {
  case -1:
    perror("fork:");
    break;
  case 0: {
    char **cmd = l->seq[0];
    execvp(cmd[0], cmd);
    break;
  }
  default: {
    if (!l->bg) {
      int status;
      waitpid(pid, &status, 0);
    }
    break;
  }
  }
  /* Remove this line when using parsecmd as it will free it */
  free(line);
  return 0;
}

SCM executer_wrapper(SCM x) {
  return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif

void terminate(char *line) {
#if USE_GNU_READLINE == 1
  /* rl_clear_history() does not exist yet in centOS 6 */
  clear_history();
#endif
  if (line)
    free(line);
  printf("exit\n");
  exit(0);
}

int compare(const char *chaine1, const char *chaine2) {
  for (int i = 0; chaine1[i] != '\0'; i++) {
    if (chaine1[i] != chaine2[i])
      return 1;
  }
  return 0;
}

int main() {
  printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);
  ListePid lp = NULL;

#if USE_GUILE == 1
  scm_init_guile();
  /* register "executer" function in scheme */
  scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

  while (1) {
    struct cmdline *l;
    char *line = 0;
    // int i, j;
    char *prompt = "ensishell>";

    /* Readline use some internal memory structure that
       can not be cleaned at the end of the program. Thus
       one memory leak per command seems unavoidable yet */
    line = readline(prompt);
    if (line == 0 || !strncmp(line, "exit", 4)) {
      terminate(line);
    }

#if USE_GNU_READLINE == 1
    add_history(line);
#endif

#if USE_GUILE == 1
    /* The line is a scheme command */
    if (line[0] == '(') {
      char catchligne[strlen(line) + 256];
      sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) "
                          "(display \"mauvaise expression/bug en scheme\n\")))",
              line);
      scm_eval_string(scm_from_locale_string(catchligne));
      free(line);
      continue;
    }
#endif

    /* parsecmd free line and set it up to 0 */
    l = parsecmd(&line);

    /* If input stream closed, normal termination */
    if (!l) {

      terminate(0);
    }

    if (l->err) {
      /* Syntax error, read another command */
      printf("error: %s\n", l->err);
      continue;
    }

    if (l->in)
      printf("in: %s\n", l->in);
    if (l->out)
      printf("out: %s\n", l->out);
    if (l->bg)
      printf("background (&)\n");

    /* Display each command of the pipe */
    /*for (i=0; l->seq[i]!=0; i++) {
            char **cmd = l->seq[i];
            printf("seq[%d]: ", i);
            for (j=0; cmd[j]!=0; j++) {
                    printf("'%s' ", cmd[j]);
            }
            printf("\n");
    }*/


    if (l->seq[0] != 0) {
			//Executer la commande jobs dans le père, et non dans un fils
      if (compare(l->seq[0][0], "jobs") == 0) {
        afficher(lp);
      } else {
        pid_t pid;
        switch (pid = fork()) {
        case -1:
          perror("fork:");
          break;
        case 0: {
          char **cmd = l->seq[0];

					//Entrée dans un fichier
          if (l->in != NULL) {
            char *nom = l->in;
            int STDIN = open(nom, O_RDWR, NULL);
            if (STDIN == -1) {
              printf("ne peut pas ouvrir le fichier \"%s\" (%s)\n", nom,
                     strerror(errno));
              exit(1);
            }
            dup2(STDIN, 0);
            close(STDIN);
          }

					//Sortie vers un fichier
          if (l->out != NULL) {
            char *nom = l->out;
            mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            int STDOUT = open(nom, O_RDWR | O_CREAT, mode);
            if (STDOUT == -1) {
              printf("ne peut pas créer le fichier \"%s\" (%s)\n", nom,
                     strerror(errno));
              exit(1);
            }
            dup2(STDOUT, 1);
            close(STDOUT);
          }

					//Création d'un pipe (multiple si nécessaire)
          int i = 1;
          while (l->seq[i] != 0) {
            char **cmd2 = l->seq[i];
            int pid2;
            int tuyau[2];
            pipe(tuyau);
            if ((pid2 = fork()) == 0) {
              dup2(tuyau[0], 0);
              close(tuyau[1]);
              close(tuyau[0]);
              execvp(cmd2[0], cmd2);
            }
            dup2(tuyau[1], 1);
            close(tuyau[0]);
            close(tuyau[1]);
            i++;
            // execvp(cmd[0],cmd);
          }

          if (!compare(cmd[0], "jobs") == 0) {
            execvp(cmd[0], cmd);
          }
          break;
        }
        default: {
          if (!l->bg) {
            int status;
            waitpid(pid, &status, 0);
          } else {
            lp = ajouter(lp, pid, l->seq[0][0]);
          }
          break;
        }
        }
      }
    }
  }
}
