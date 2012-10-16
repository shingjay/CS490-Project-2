#include <stdio.h>
#include <stdlib.h>

void initializeFiles();
int runSample();
void runSampleNTimes(int n);

int main() 
{
	// initialize files
	initializeFiles();
	
	// run sample the first time to get the total synchronization points
	int n = runSample();
	//printf("n is %d\n",n);
	runSampleNTimes(n);
}

void initializeFiles() 
{
	FILE *f1 = fopen("syncpoints.txt","w");
	FILE *f2 = fopen("nth-thread.txt","w");
	
	fprintf(f1,"%d\n",0);
	fprintf(f2,"%d\n",1);
	
	fclose(f1);
	fclose(f2);
}

// first execution	
// TODO change the sample file name here
int runSample()
{
	system("./run.sh ./sample1");
	int temp;
	FILE *f1 = fopen("syncpoints.txt","r");
	fscanf(f1,"%d",&temp);
	fclose(f1);
	return temp;
}

// runs sample file n amount of times
// TODO change the sample file name here
void runSampleNTimes(int n)
{
	int i;
	for (i = 1 ; i <= n ; i++) 
	{
		FILE *f = fopen("nth-thread.txt","w");
		fprintf(f,"%d",i);
		fclose(f);
		system("./run.sh ./sample1");
	//	printf("@i = %d\n\n",i);
	//	system("cat nth-thread.txt");
	}
}
