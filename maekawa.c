#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <unistd.h> 
#include<string.h> 
#include<sys/wait.h> 
// higher thread has higher priority
#define P 4
#define Psq 2
#define P1 2
#define P2 1
#define P3 1
#define REQUEST 1
#define REPLY 2
#define RELEASE 3
#define GRANT 4
#define INQUIRE 5
#define FAILED 6
#define YIELD 7
float fabsm(float a){
	if(a<0)
	return -1*a;
return a;
}
int main(int argc, char *argv[])
{
    int i,j;
    //every pipe is incoming
    int ** fdpipes = (int**)malloc(sizeof(int*)*(P));
    float messagebuffer;
    int sender;
	for(i=0;i<P;i++){
		fdpipes[i] = (int *)malloc(sizeof(int)*2);
	}
	for(i=0;i<P;i++){
		if (pipe(fdpipes[i])==-1) 
		{ 
			fprintf(stderr, "Pipe Failed" ); 
			return 1; 
    	} 
	}
    int gridhelper[Psq][Psq];
    for(i=0;i<Psq;i++){
        for(j=0;j<Psq;j++){
            gridhelper[i][j] = Psq*i+j;
        }
    }
    //-1 in request queue means absence 1 means presence
    int requestqueue[P];
    int topper;
    for(i=0;i<P;i++){
        requestqueue[i]=-1;
    }
    int grantsecuredarr[P];
    int numgrantssecured=0;
    int numfailedreq=0;
    for(i=0;i<P;i++){
        grantsecuredarr[i]=-1;
    }
    //1 means I have the lock -1 means I don't have the lock or grant
    int grantlock=1;
    //-1 grantgiven to means grant given to none
    int grantgivento=-1;
    int tid;
    int masterpid = getpid();
    int childpid;
    int childpidarr[P];
    int ptype;
    for(i=0;i<P;i++){
        childpid=fork();
        if(childpid<0){
            printf("Fork failed");
        }else if(childpid>0){
            childpidarr[i]=childpid;
        }else{
            tid=i;
            if(i<P1){
                ptype=1;
            }else if(i<P1+P2){
                ptype=2;
            }else{
                ptype=3;
            }
            break;
        }
    }
    if(childpid==0){
        if(ptype!=1){
            // depending on ptype 2 or 3 i either sleep or not after acquring lock
            //acquire the lock
            messagebuffer = 1.0*REQUEST + 10.0*tid;
            int numrow = tid/Psq;
            int numcol = tid%Psq;
            for(i=0;i<Psq;i++){
                if(i==tid){
                    continue;
                }
                // if(i>tid){
                //     write(fdpipes[2 * (gridhelper[i][tid])][1],&messagebuffer,sizeof(float));
                //     write(fdpipes[2 * (gridhelper[tid][i])][1],&messagebuffer,sizeof(float));

                // }else{
                //     write(fdpipes[(2 * (gridhelper[i][tid]))+1][1],&messagebuffer,sizeof(float));
                //     write(fdpipes[(2 * (gridhelper[tid][i]))+1][1],&messagebuffer,sizeof(float));
                // }
                write(fdpipes[(gridhelper[i][numcol])][1],&messagebuffer,sizeof(float));
                write(fdpipes[(gridhelper[numrow][i])][1],&messagebuffer,sizeof(float));
            }
            write(fdpipes[tid][1],&messagebuffer,sizeof(float));
            //sent the request to acquire lock

        }
        

        for(;;){
            read(fdpipes[tid][0],&messagebuffer,sizeof(float));
            int typemessage = ((int) messagebuffer)%10;
            switch (typemessage)
            {
                case REQUEST:
                    sender = (int) (messagebuffer/10);
                    if(grantlock==1){
                        //I have the lock I can grant the lock
                        grantlock=-1;
                        messagebuffer=1.0*GRANT + 10.0*tid;
                        grantgivento=sender;
                        requestqueue[sender]=1;
                        write(fdpipes[sender][1],&messagebuffer,sizeof(float));

                    }else{
                        //if i dont have the lock
                        //queue the sender
                        requestqueue[sender]=1;
                        //priority based case now
                        if(grantgivento>=sender){
                            //send failed
                            messagebuffer = 1.0*FAILED + 10.0*tid;
                            write(fdpipes[sender][1],&messagebuffer,sizeof(float));

                        }else if(grantgivento<sender)
                        {
                            //inquire the grantgiven
                            messagebuffer = 1.0*INQUIRE + 10.0*tid;
                            write(fdpipes[grantgivento][1],&messagebuffer,sizeof(float));
                        }else{
                            printf("can't happen");
                        }
                        
                    }
                    break;
                case RELEASE:
                    sender = (int) (messagebuffer/10);
                    topper=-1;
                    for(i=P-1;i>=0;i--){
                        if(requestqueue[i]==-1){
                            continue;
                        }else if(requestqueue[i]==1){
                            topper=i;
                            break;
                        }
                    }
                    if(topper==-1){
                        printf("fishy behaviour in release");
                    }
                    grantgivento=topper;
                    grantlock = -1;
                    requestqueue[sender]=-1;
                    messagebuffer=1.0*GRANT+10.0*tid;
                    write(fdpipes[topper][1],&messagebuffer,sizeof(float));
                    break;
                case GRANT:
                    sender = (int) (messagebuffer/10);
                    grantsecuredarr[sender]=1;
                    numgrantssecured+=1;
                    break;
                case INQUIRE:
                    if(numfailedreq>0){
                        // I should send yield
                        sender = (int) (messagebuffer/10);
                        numgrantssecured-=1;
                        grantsecuredarr[sender] = -1;
                        messagebuffer = 1.0*YIELD + 10.0*tid;
                        write(fdpipes[sender][1],&messagebuffer, sizeof(float));
                    }
                    break;
                case FAILED:
                    sender = (int) (messagebuffer/10);
                    numfailedreq+=1;
                    grantsecuredarr[sender] = 0;
                    break;
                case YIELD:
                    sender = (int) (messagebuffer/10);
                    topper=-1;
                    for(i=P-1;i>=0;i--){
                        if(requestqueue[i]==-1){
                            continue;
                        }else if(requestqueue[i]==1){
                            topper=i;
                            break;
                        }
                    }
                    if(topper==-1){
                        printf("fishy behaviour in release");
                    }
                    grantgivento=topper;
                    grantlock = -1;
                    messagebuffer=1.0*GRANT+10.0*tid;
                    requestqueue[sender] = 1;
                    write(fdpipes[topper][1],&messagebuffer,sizeof(float));

                    break;
            
                default:
                    printf("unknown message pls help");
                    break;
            }
            //check grants
            if(numgrantssecured=2*Psq-1){
                //got all grants 
                int numrow = tid/Psq;
                int numcol = tid%Psq;
                int mybool=1;//true
                for(i=0;i<Psq;i++){
                    if(grantsecuredarr[gridhelper[i][numcol]]!=1){
                        mybool=0;
                        break;
                    }
                    if(grantsecuredarr[gridhelper[numrow][i]]!=1){
                        mybool=0;
                        break;
                    }
                }
                if(mybool==1){
                    //I have all the grants
                    printf("%d acquired the lock\n",tid);
                    if(ptype==2){
                        sleep(2);
                    }
                    if(ptype==1){
                        printf("fishy yes it is as type 1 has lock lol\n");
                    }
                    numgrantssecured=0;
                    for(i=0;i<P;i++){
                        grantsecuredarr[i]=-1;
                    }
                    for(i=0;i<Psq;i++){
                        if(i==tid){
                            continue;
                        }
                        messagebuffer=1.0*RELEASE+10.0*tid;
                        write(fdpipes[gridhelper[i][numcol]][1], &messagebuffer,sizeof(float));
                        write(fdpipes[gridhelper[numrow][i]][1], &messagebuffer,sizeof(float));

                    }
                    messagebuffer=1.0*RELEASE+10.0*tid;
                    write(fdpipes[tid][1],&messagebuffer, sizeof(messagebuffer));
                    printf("%d released the lock\n",tid);
                }

            }
        }

        
    }else{

    }



}