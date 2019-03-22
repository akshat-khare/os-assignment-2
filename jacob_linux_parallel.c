// #include "types.h"
// #include "stat.h"
// #include "user.h"

#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <unistd.h> 
#include<string.h> 
#include<sys/wait.h> 

#define N 20
#define E 0.00001
#define T 100.0
#define P 4
#define L 20000


float fabsm(float a){
	if(a<0)
	return -1*a;
return a;
}
int main(int argc, char *argv[])
{
	float diff, tempdiff;
	// float temp;
	int i,j;
	int start,end;
	float mean;
	float u[N][N];
	float w[N][N];
	float * floatarrbuffer = (float *)malloc((sizeof(float)*(N-2)));
    // fork();
	int count=0;
	mean = 0.0;
	for (i = 0; i < N; i++){
		u[i][0] = u[i][N-1] = u[0][i] = T;
		u[N-1][i] = 0.0;
		mean += u[i][0] + u[i][N-1] + u[0][i] + u[N-1][i];
	}
	mean /= (4.0 * N);
	for (i = 1; i < N-1; i++ )
		for ( j= 1; j < N-1; j++) u[i][j] = mean;
	// 2 (p-1 ) pipes will be used for child <-> child comm
	// even pipes will be unidirectional from top to bottom
	// odd pipes will be unidirectional from bottom to top
	int ** fdpipes = (int**)malloc(sizeof(int*)*(2*(P-1)));
	for(i=0;i<2*(P-1);i++){
		fdpipes[i] = (int *)malloc(sizeof(int)*2);
	}
	for(i=0;i<2*(P-1);i++){
		if (pipe(fdpipes[i])==-1) 
		{ 
			fprintf(stderr, "Pipe Failed" ); 
			return 1; 
    	} 
	}
	// 2* p pipes for master <-> child comm
	//even for master -> child
	//odd for child -> master
	int ** fdmasterpipes = (int**)malloc(sizeof(int*)*(2*(P)));
	for(i=0;i<2*(P);i++){
		fdmasterpipes[i] = (int *)malloc(sizeof(int)*2);
	}
	for(i=0;i<2*(P);i++){
		if (pipe(fdmasterpipes[i])==-1) 
		{ 
			fprintf(stderr, "Pipe Failed" ); 
			return 1; 
    	} 
	}
	// printf("Pipes made\n");
	int masterpid = getpid();
	int * childpidarr= (int *)malloc(sizeof(int)*P);
	int childpid;
	int tid;
	for(i=0;i<P;i++){		
		childpid = fork();
		if(childpid<0){
			// printf("Fork failed\n");
			fprintf(stderr, "Fork failed");
		}else if(childpid>0){
			childpidarr[i] = childpid;			
		}else{
			// printf("setting child tid\n");
			tid=i;
			break;
		}
	}
	// printf("all childs forked %d\n",childpid);
	if(childpid==0){
		start = 1 + tid*((N-2)/P);
		if(tid==P-1){
			end = N-2;
		}else{
			end = 1 + (tid+1)*((N-2)/P) -1;
		}
		// printf("tid, start, end are %d %d %d\n",tid,start,end);
		for(;;){
			// no ipc collect for iteration 1
			// printf("count is %d\n",count);
			if(count!=0){
				//get diff from master
				// printf("%d expecting diff from master on pipe %d\n",tid, 2*tid);
				read(fdmasterpipes[2*tid][0],&diff, sizeof(diff));
				// printf("%d got diff %.6f from master on pipe %d\n",tid,diff, 2*tid);
				
				// close the iterations 
				if(diff<0){
					// send u's to master
					float temp;
					for(i=start;i<=end;i++){
						for(j=1;j<N-1;j++){
							floatarrbuffer[j-1]=u[i][j];
						}
						write(fdmasterpipes[2*tid+1][1],floatarrbuffer,(sizeof(float))*(N-2));
					}
					break;				
				}


				// //get u values from neighbours
				// if(tid>0){
				// 	//receive from upper
				// 	float temp;
				// 	int temppipe = tid-1;
				// 	int rownum =1+ tid*((N-2)/P) -1;
				// 	for(i=1;i<N-1;i++){
				// 		read(fdpipes[2*(temppipe)][0], &temp, sizeof(temp));
				// 		u[rownum][i] = temp;
				// 	}

				// }
				//get u values from neighbours
				if(tid>0){
					//receive from upper
					float temp;
					int temppipe = tid-1;
					int rownum =1+ tid*((N-2)/P) -1;
					read(fdpipes[2*(temppipe)][0], floatarrbuffer, sizeof(float)*(N-2));
					for(i=1;i<N-1;i++){
						u[rownum][i] = floatarrbuffer[i-1];
					}

				}
				// if(tid<P-1){
				// 	//receive from down
				// 	float temp;
				// 	int temppipe = tid;
				// 	int rownum= 1 + (tid+1)*((N-2)/P);
				// 	for(i=1;i<N-1;i++){
				// 		read(fdpipes[2*(temppipe)+1][0], &temp, sizeof(temp));
				// 		u[rownum][i] = temp;
				// 	}
				// }
				if(tid<P-1){
					//receive from down
					float temp;
					int temppipe = tid;
					int rownum= 1 + (tid+1)*((N-2)/P);
					read(fdpipes[2*(temppipe)+1][0], floatarrbuffer, (sizeof(float))*(N-2));
					for(i=1;i<N-1;i++){
						u[rownum][i] = floatarrbuffer[i-1];
					}
				}

			}
			diff = 0.0;
			for(i =start ; i <= end; i++){
				for(j =1 ; j < N-1; j++){
					w[i][j] = ( u[i-1][j] + u[i+1][j]+
							u[i][j-1] + u[i][j+1])/4.0;
					if( fabsm(w[i][j] - u[i][j]) > diff )
						diff = fabsm(w[i][j]- u[i][j]);	
				}
			}
			count++;
			
			// if(diff<= E || count > L){ 
			// 	break;
			// }
			// if(count > L){ 
			// 	exit(0);
			// 	//send the values to master

			// 	break;
			// }
		
			for (i =start; i<= end; i++)	
				for (j =1; j< N-1; j++) u[i][j] = w[i][j];
			
			
			//send u values to neighbours
			// printf("%d count %d computation is done\n",tid,count);
			// if(tid>0){
			// 	//send to upper
			// 	float temp;
			// 	int temppipe = tid-1;
			// 	int rownum =1+ tid*((N-2)/P);
			// 	// printf("%d sending row %d to upper neighbours\n",tid,rownum);

			// 	for(i=1;i<N-1;i++){
			// 		temp = u[rownum][i];
			// 		write(fdpipes[2*(temppipe)+1][1], &temp, sizeof(temp));
			// 	}

			// 	// printf("%d sent row %d to upper neighbours\n",tid,rownum);
			// }
			if(tid>0){
				//send to upper
				float temp;
				int temppipe = tid-1;
				int rownum =1+ tid*((N-2)/P);
				// printf("%d sending row %d to upper neighbours\n",tid,rownum);

				for(i=1;i<N-1;i++){
					floatarrbuffer[i-1] = u[rownum][i];
				}
				write(fdpipes[2*(temppipe)+1][1], floatarrbuffer, sizeof(float)*(N-2));

				// printf("%d sent row %d to upper neighbours\n",tid,rownum);
			}
			// if(tid<P-1){
			// 	//send to down
			// 	float temp;
			// 	int temppipe = tid;
			// 	int rownum= 1 + (tid+1)*((N-2)/P) -1;
			// 	// printf("%d sending row %d to down neighbours\n",tid,rownum);

			// 	for(i=1;i<N-1;i++){
			// 		temp = u[rownum][i];
			// 		write(fdpipes[2*(temppipe)][1], &temp, sizeof(temp));
			// 	}
			// 	// printf("%d sent row %d to down neighbours\n",tid,rownum);
			// }
			if(tid<P-1){
				//send to down
				float temp;
				int temppipe = tid;
				int rownum= 1 + (tid+1)*((N-2)/P) -1;
				// printf("%d sending row %d to down neighbours\n",tid,rownum);

				for(i=1;i<N-1;i++){
					floatarrbuffer[i-1] = u[rownum][i];
				}
				write(fdpipes[2*(temppipe)][1], floatarrbuffer, (sizeof(float))* (N-2));
				// printf("%d sent row %d to down neighbours\n",tid,rownum);
			}
			//send diff to master
			// printf("%d sending diff %.6f to master on pipe %d\n",tid,diff, 2*tid+1);
			write(fdmasterpipes[2*tid+1][1],&diff,sizeof(diff));


		}
		//over
		exit(0);
	}else{
		for(;;){
			diff=0.0;
			count++;
			for(i=0;i<P;i++){
				// printf("master receiving diff from %d on pipe %d\n",i, 2*i+1);

				read(fdmasterpipes[2*i+1][0],&tempdiff,sizeof(tempdiff));
				// printf("master received diff %.6f from %d on pipe %d\n",tempdiff, i, 2*i+1);
				if(tempdiff>diff){
					diff=tempdiff;
				}
			}
			if(diff<= E || count > L){ 
				// printf("------------------CONVERGED----------\n");
				diff=-1.0;
				for(i=0;i<P;i++){

					write(fdmasterpipes[2*i][1],&diff,sizeof(diff));
				}
				//gather the u's
				for(tid=0;tid<P;tid++){
					start = 1 + tid*((N-2)/P);
					if(tid==P-1){
						end = N-2;
					}else{
						end = 1 + (tid+1)*((N-2)/P) -1;
					}
					for(i=start;i<=end;i++){
						read(fdmasterpipes[2*tid+1][0],floatarrbuffer,(sizeof(float))* (N-2));
						for(j=1;j<N-1;j++){
							float temp;
							u[i][j]=floatarrbuffer[j-1];
						}
					}
				}
				break;
			}else{
				for(i=0;i<P;i++){
					// printf("master writing diff %.6f on tid %d on pipe %d\n",diff,i,2*i);
					write(fdmasterpipes[2*i][1],&diff,sizeof(diff));
					// printf("master written diff %.6f on tid %d on pipe %d\n",diff,i,2*i);

				}
			}
		}


	}
	printf("count is %d\n",count);
	for(i =0; i <N; i++){
		for(j = 0; j<N; j++)
			printf("%d ",((int)u[i][j]));
		printf("\n");
	}
	// for(i =0; i <N; i++){
	// 	for(j = 0; j<N; j++)
	// 		printf("%.6f ",(u[i][j]));
	// 	printf("\n");
	// }
	exit(0);

}
