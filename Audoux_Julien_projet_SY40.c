#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/shm.h>


#define MINCARSEC 1
#define MAXCARSSEC 6

#define REFRESH_DISPLAY 2

#define SEMPERM 0600

#define IFLAGS (SEMPERM | IPC_CREAT)


struct sembuf sem_oper_P;
struct sembuf sem_oper_V;

time_t start_time;

int semid, semid_waitings;

int shmid;

pid_t main_process;

int status;

int displayMode;

typedef struct vehicule {
  int distance;
  int type;
  int payment;
} vehicule;

void title() {
  printf("\n------------------------- TOLL ROAD SIMULATION v1.6 -------------------------\n\n");
  printf("\t[*]started on: %s", ctime(&start_time));
}

void display() {
  int* data = shmat(shmid, NULL, 0);
  title();
  printf("\t[*]execution time: %lds\n\n", time(NULL) - start_time);
  printf("\t[*][T-TOLL] lanes: %d, queue: %d, earnings: %d€, treated cars: %d\n", data[3], data[6], data[0], data[9]);
  printf("\t[*][CARD-TOLL] lanes: %d, queue: %d, earnings: %d€, treated cars: %d\n", data[4], data[7], data[1], data[10]);
  printf("\t[*][TRUCK-TOLL] lanes: %d, queue: %d, earnings: %d€, treated cars: %d\n", data[5], data[8], data[2], data[11]);
  printf("\t[*]car generation: %d/s\n\n", data[12]);
  shmdt(data);
}

void end(char message[]) {
  if(getpid() == main_process) {
    while (wait(&status) > 0);
    if(displayMode) printf("\e[1;1H\e[2J");
    display();
    printf("\t%s\n", message);
    if(semctl(semid, 0, IPC_RMID) == -1) {
      perror("error");
      exit(EXIT_FAILURE);
    }
    if(semctl(semid_waitings, 0, IPC_RMID) == -1) {
      perror("error");
      exit(EXIT_FAILURE);
    }
    if(shmctl(shmid,IPC_RMID,0) == -1) {
      perror("error");
      exit(EXIT_FAILURE);
    }
  }
  exit(0);
}

void sigHandler(int sig) {
  switch(sig) {
    case SIGINT:
      end("------------------------- Ctrl-C -------------------------");
    case SIGUSR1:
      displayMode = 0;
      end("------------------- [-]flux fork error -------------------");
    default:
      printf("[-]signal %d not recognized", sig);
  }
}

int initsem() {
  key_t key = 111;
  int status = 0;
  union semun {
    int val;
    struct semid_ds *stat;
    short * array;
  } ctl_arg;

  if ((semid = semget(key, 3, IFLAGS)) > 0) {
    short array[] = {1, 1, 1};
    ctl_arg.array = array;
    status = semctl(semid, 0, SETALL, ctl_arg);
  }
  if (status == -1) {
    perror("error initsem status");
    return (-1);
  } else return (semid);
}

int initsemWait() {
  key_t key = 123;
  int status = 0;
  union semun {
    int val;
    struct semid_ds *stat;
    short * array;
  } ctl_arg;

  if ((semid_waitings = semget(key, 3, IFLAGS)) > 0) {
    short array[] = {10, 10, 10};
    ctl_arg.array = array;
    status = semctl(semid_waitings, 0, SETALL, ctl_arg);
  }
  if (status == -1) {
    perror("error initsemWait status");
    return (-1);
  } else return (semid_waitings);
}

vehicule randomVehicle() {
  srand(time(NULL) ^(getpid()<<16));
  vehicule vehicule;
  vehicule.distance = rand() % 200;
  vehicule.type = rand() % 5;
  vehicule.payment = rand() % 2;
  return vehicule; 
}

int P(int semnum) {
  sem_oper_P.sem_num = semnum;
  sem_oper_P.sem_op = -1 ;
  sem_oper_P.sem_flg = 0 ;
  return semop(semid, &sem_oper_P, 1);
}

int V(int semnum) {
  sem_oper_V.sem_num = semnum;
  sem_oper_V.sem_op = 1 ;
  sem_oper_V.sem_flg = 0 ;
  return semop(semid, &sem_oper_V, 1);
}

int Pwait(int semnum) {
  sem_oper_P.sem_num = semnum;
  sem_oper_P.sem_op = -1;
  sem_oper_P.sem_flg = 0;
  return semop(semid_waitings, &sem_oper_P, 1);
}

int Vwait(int semnum) {
  sem_oper_V.sem_num = semnum;
  sem_oper_V.sem_op = 1;
  sem_oper_V.sem_flg = 0;
  return semop(semid_waitings, &sem_oper_V, 1);
}

//toll
void toll(vehicule vehicule, int index){
  int sem;
  int price = vehicule.distance * 0.1 * (vehicule.type + 1);
  int wait = rand() % (vehicule.type + 1) * 2 + (vehicule.type + 1); 
  int* data = shmat(shmid, NULL, 0);
  if (vehicule.type == 2 || vehicule.type == 3) {
    /* TRUCK */
    sem = 2;
    Pwait(sem);
    data[6 + sem]++;
    P(sem);
    Vwait(sem);
    data[6 + sem]--;
    printf("[%d][TRUCK-TOLL]", index);
  } else if(vehicule.payment == 0) {
    /* CARD PAYMENT */
    sem = 1;
    Pwait(sem);
    data[6 + sem]++;
    P(sem);
    Vwait(sem);
    data[6 + sem]--;
    printf("[%d][CARD-TOLL]", index);
  } else {
    /* T PAYMENT */
    sem = 0;
    Pwait(sem);
    data[6 + sem]++;
    P(sem);
    Vwait(sem);
    data[6 + sem]--;
    printf("[%d][T-TOLL]", index);
  }
  data[sem] += price;
  data[9 + sem]++;
  printf("vehicle: {%dkm, payment:%d, type:%d, price:%d€, time:%ds}\n", vehicule.distance, vehicule.payment, vehicule.type, price, wait);
  fflush(stdout);
  sleep(wait);
  V(sem);
  shmdt(data);
}

//add or remove a lane
void manageLanes() {
  int* data;
  while(1) {
    data = shmat(shmid, NULL, 0);
    for (int i = 0; i < 3; ++i) {
      if (semctl(semid_waitings, i, GETVAL, NULL) == 0) {
        V(i);
        for (int a = 0; a < 10; ++a) {
          Vwait(i);
        }
        data[3 + i]++;
        printf("\t[*]lane opened on toll: %d\n", i);
      } else if ((semctl(semid_waitings, i, GETVAL, NULL) > 20)) {
        switch(fork()) {
          case 0:
            P(i);
            exit(0);
          default:;
        }
        for (int a = 0; a < 10; ++a) {
          Pwait(i);
        }
        data[3 + i]--;
        printf("\t[*]lane closed on toll: %d\n", i);
      }
    }
    shmdt(data);
    usleep(100000);
  }
}

//cars trafic
int flux() {
  int* data;
  int i = 1, sec = 1;
  int carsecond = MINCARSEC;
  int up = 1;
  while(1) {
    data  = shmat(shmid, NULL, 0);
    data[12] = carsecond;
    switch(fork()) {
      case -1:
        perror("[-]flux fork error");
        shmdt(data);
        return 1;
      case 0:
        toll(randomVehicle(), i);
        exit(0);
      default:;
    }
    i++;
    if (i % carsecond == 0) {
      sec++;
      sleep(1);
      if (sec % 20 == 0) {
        //carsecond = (rand() % MAXCARSSEC) + 1;
        if (up) {
          carsecond++;
        } else {
          carsecond--;
        }
        if (carsecond >= MAXCARSSEC) {
          up = 0;
        } else if (carsecond <= MINCARSEC) {
          up = 1;
        }
        printf("\t[*]flux: %d cars per second\n", carsecond);
        fflush(stdout);
      }
    }
    shmdt(data);
  }
}

int main(int argc, char const *argv[]) {

  //initialisation

  main_process = getpid();

  displayMode = 1;

  if (argc != 1) {
    displayMode = atoi(argv[1]);
  }

  int status = 0;

  signal(SIGINT,sigHandler);
  signal(SIGUSR1, sigHandler);

  start_time = time(NULL);

  if((semid = initsem()) < 0) {
    perror("seminit error semid");
    return(1);
  }
  if ((semid_waitings = initsemWait()) < 0) {
    perror("seminit waitings error semid");
    return(1);
  }

  if ((shmid = shmget(111111,60,IFLAGS))==-1) {
    perror("initshm error");
    exit(EXIT_FAILURE);
  }
  int* data = shmat(shmid, NULL, 0);
  for (int i = 0; i < 13; ++i) {
    data[i] = 0;
  }

  //number of lane on each path
  data[3] = semctl(semid, 0, GETVAL, NULL);
  data[4] = semctl(semid, 1, GETVAL, NULL);
  data[5] = semctl(semid, 2, GETVAL, NULL);

  //menu display
  if (displayMode){
    switch(fork()) {
      case -1:
        perror("display fork error");
        exit(1);
        break;
      case 0:
        while(displayMode) {
          printf("\e[1;1H\e[2J");
          display();
          sleep(REFRESH_DISPLAY);
        }
        exit(0);
        default:;
    }
  } else {
    title();
    printf("\n");
  }

  //management of lanes
  switch(fork()) {
    case -1:
      perror("manageLane fork error");
      exit(1);
      break;
    case 0:
      manageLanes();
      exit(0);
      break;
    default:;
  }

  //starting vehicle flux
  if (flux() == 1) {
    kill(0, SIGUSR1);
  }
  

  return 0;
}
