#include <stdlib.h>	
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define _CRT_SECURE_NO_WARNINGS 


#define SEMPERM 0600
#define TRUE 1
#define FALSE 0
 
typedef union semun {  
             int val; 
             struct semid_ds *buf;  
             ushort *array;
             } semun;

int initsem (key_t semkey, int n)  
{
   int status = 0;
   int semid;
   if ((semid = semget (semkey, 1, SEMPERM | IPC_CREAT | IPC_EXCL)) == -1)
   {
       if (errno == EEXIST)
                semid = semget (semkey, 1, 0);
   }
   else
   {
       semun arg;
       arg.val = n;
       status = semctl(semid, 0, SETVAL, arg);
   }
   if (semid == -1 || status == -1)
   {
       perror("initsem failed");
       return (-1);
   }
   return (semid);
}

int p (int semid)
{
   struct sembuf p_buf;
   p_buf.sem_num = 0;
   p_buf.sem_op = -1;
   p_buf.sem_flg = SEM_UNDO;
   if (semop(semid, &p_buf, 1) == -1)
   {
	perror ("p(semid) failed");
	exit(1);
   }
   return (0);
}

int v (int semid)
{
   struct sembuf v_buf;
   v_buf.sem_num = 0;
   v_buf.sem_op = 1;
   v_buf.sem_flg = SEM_UNDO;
   if (semop(semid, &v_buf, 1) == -1)
   {
	perror ("v(semid) failed");
	exit(1);
   }
   return (0);
}


// Class Lock 
typedef struct _lock { 
   int semid;
} Lock;

void initLock(Lock *l, key_t semkey) {
   if ((l->semid = initsem(semkey,1)) < 0)    
   // 세마포를 연결한다.(없으면 초기값을 1로 주면서 새로 만들어서 연결한다.)
      exit(1);
}

void Acquire(Lock *l) {
   p(l->semid);
   printf("\n writer %d lock acquire \n", getpid());
}

void Release(Lock *l) {
   v(l->semid);
   printf("\n writer %d lock release \n", getpid());
}



typedef struct _file { 
   char fName[20]; 
} File;


// Shared variable by file

void reset(char *fileVar) {
// fileVar라는 이름의 텍스트 '화일이 없으면' 새로 만들고 0값을 기록한다.
	int nResult = access( fileVar, 0 );
	if( nResult == -1 ){
		time_t t;
		FILE *iof=fopen(fileVar,"w");
		fprintf(iof,"%d %s 0", getpid(), ctime(&t));
		fclose(iof);
	}
}

void Store(char *fileVar,int i) {
// fileVar 화일 끝에 i 값을 append한다. 
	time_t t;
	FILE *iofile=fopen(fileVar,"w");
	fprintf(iofile,"%d %s %d", getpid(), ctime(&t) , i);
	fclose(iofile);	
}
 
int Load(char *fileVar) {
// fileVar 화일의 마지막 값을 읽어 온다. 
	FILE *iofile=fopen(fileVar,"r");
	int cnt;
	char getval[20];
	while(!feof(iofile)){
		fgets(getval, 20,iofile);
	}
	cnt=atoi(getval);
	fclose(iofile);
	return cnt;
}

void add(char *fileVar,int i) {
// fileVar 화일의 마지막 값을 읽어서 i를 더한 후에 이를 끝에 append 한다. 
	int lastNum;
	lastNum=Load(fileVar);
	lastNum+=i;
	Store(fileVar,lastNum);
}

void sub(char *fileVar,int i) {
// fileVar 화일의 마지막 값을 읽어서 i를 뺀 후에 이를 끝에 append 한다. 
	int lastNum;
	lastNum=Load(fileVar);
	lastNum-=i;
	Store(fileVar,lastNum);	
}


// Class CondVar
typedef struct _cond {
   int semid;
   char *queueLength;
} CondVar;

void initCondVar(CondVar *c, key_t semkey, char *queueLength) {
   c->queueLength = queueLength;
   reset(c->queueLength); // queueLength=0
   if ((c->semid = initsem(semkey,0)) < 0)    
   // 세마포를 연결한다.(없으면 초기값을 0로 주면서 새로 만들어서 연결한다.)
      exit(1); 
}

 
void Wait(CondVar *c, Lock *lock) {  
   add(c->queueLength, 1); 
   Release(lock);
   p(c->semid);
   Acquire(lock);
}

void Signal(CondVar *c) { 
 
   if(Load(c->queueLength) > 0) {
     v(c->semid);
     sub(c->queueLength, 1);
   }
}

void Broadcast(CondVar *c) {
 
   while(Load(c->queueLength) > 0) {
     v(c->semid);
     sub(c->queueLength, 1);
   }
}




void main(int argc, char *argv[])
{
   key_t semkey = 0x100; 
   key_t reader = semkey + 100;
   key_t writer = semkey + 200;

   Lock lock;
   CondVar R;
   CondVar W; 
   
   pid_t pid;
   pid = getpid();

   initLock(&lock,semkey);
   initCondVar(&R, reader, "okToRead.txt");
   initCondVar(&W, writer, "okToWrite.txt");

   reset("AR.txt");
   reset("WR.txt");
   reset("AW.txt");
   reset("WW.txt");
 
   sleep(atoi(argv[1]));
 


   //Writer Check In
   Acquire(&lock);

   int AR = Load("AR.txt");  
   int WR = Load("WR.txt");
   int AW = Load("AW.txt");
   int WW = Load("WW.txt");

   while((AR + AW) > 0) {    
     add("WW.txt", 1);    
     Wait(&W,&lock);       
     sub("WW.txt", 1);      
     AR = Load("AR.txt");    
     AW = Load("AW.txt");
   }
   add("AW.txt", 1);         
 
   Release(&lock);
   


   //Critical Section
   printf("process %d in critical section \n",pid);
   sleep(atoi(argv[2])); 



   //Writer Check Out
   Acquire(&lock);

   sub("AW.txt", 1);    

   WW = Load("WW.txt");
   WR = Load("WR.txt");

   if(WW > 0) {
    Signal(&W);
   }
   else if(WR > 0) {
    Broadcast(&R);
   }
    
   Release(&lock);  



   return;
}
