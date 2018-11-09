
#include <stdio.h>
#include <signal.h>
void bonjour(int sig)
{
  printf("J'ai recu le signal %d\n", sig);
}
int main()
{
  struct sigaction traitant = {};
  traitant.sa_handler = bonjour;
  sigemptyset ( & traitant.sa_mask );
  traitant.sa_flags = 0 ;
  if ( sigaction( SIGTSTP, &traitant, 0) == -1 )
    perror("sigaction");
  printf("presser Ctrl-Z\n");
  /* le terminal envoie le signal SIGTSTP
     lors d'un Ctrl-Z */
  while(1);
}
