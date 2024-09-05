//DANIELE FAZZARI N46004470

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <rtai_lxrt.h>
#include <rtai_shm.h>
#include <sys/io.h>
#include <signal.h>
#include "parameters.h"
#define CPUMAP 0x0
#include <rtai_sem.h>

static RT_TASK *main_Task;
static RT_TASK *temp_Task;
static RT_TASK *alt_Task;
static RT_TASK *speed_Task;
static int keep_on_running = 1;
static pthread_t temp;
static pthread_t alt;
static pthread_t speed;
static RTIME expected;
static RTIME base_period;
struct raw_sensors_data* raw_sensor;
struct processed_sensors_data* proc_sensor;
static void endme(int dummy) {keep_on_running = 0;}

SEM* temp_sem;
SEM* speed_sem;
SEM* alt_sem;

static void * temp_loop(void * par) 
{	
	if (!(temp_Task = rt_task_init_schmod(nam2num("TEMP"), 22, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT PERIODIC TASK\n");
		exit(1);
	}

	unsigned int iseed = (unsigned int)rt_get_time();
  	srand (iseed);

	expected = rt_get_time() + base_period;
	rt_task_make_periodic(temp_Task, expected, 4*base_period);
	rt_make_hard_real_time();

	int value = -50;
	int count = 1;
	int broken_sensor = (int)(((float)TEMP_SIZE) * rand() / ( RAND_MAX + 1.0 ));
	int when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));

	while (keep_on_running)
	{
		value += -1 + (int)(3.0 * rand() / ( RAND_MAX + 1.0 ));
		int i;

		//inizio sezione critica
		rt_sem_wait(temp_sem);
		for (i=0; i<TEMP_SIZE; i++) raw_sensor->temperatures[i]=value;

		if (count%when_to_break==0) {
			raw_sensor->temperatures[broken_sensor]=0; // random stuck at zero...
			when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
			count = when_to_break;
		}
		//fine sezione critica
		rt_sem_signal(temp_sem);

		count++;
		rt_task_wait_period();
	}
	rt_task_delete(temp_Task);
	return 0;
}

static void * alt_loop(void * par) {
	
	if (!(alt_Task = rt_task_init_schmod(nam2num("ALT"), 20, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT PERIODIC TASK\n");
		exit(1);
	}

	unsigned int iseed = (unsigned int)rt_get_time();
  	srand (iseed);

	expected = rt_get_time() + base_period;
	rt_task_make_periodic(alt_Task, expected, base_period);
	rt_make_hard_real_time();

	int value = 11000;
	int count = 1;
	int broken_sensor_1 = (int)(((float)ALTITUDE_SIZE) * rand() / ( RAND_MAX + 1.0 ));
	int broken_sensor_2 = (int)(((float)ALTITUDE_SIZE) * rand() / ( RAND_MAX + 1.0 ));
	int when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
	while (keep_on_running)
	{
		value += -1 + (int)(3.0 * rand() / ( RAND_MAX + 1.0 ));
		int i;

		//inizio sezione critica
		rt_sem_wait(alt_sem);
		for (i=0; i<ALTITUDE_SIZE; i++) raw_sensor->altitudes[i]=value;
		if (count%when_to_break==0) {
			raw_sensor->altitudes[broken_sensor_1]
				+= -3 + (int)(7.0 * rand() / ( RAND_MAX + 1.0 )); // random value failure
			raw_sensor->altitudes[broken_sensor_2] = 0; // random stuck at zero
			when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
			count = when_to_break;
		}
		//fine sezione critica
		rt_sem_signal(alt_sem);

		count++;
		rt_task_wait_period();
	}
	rt_task_delete(alt_Task);
	return 0;
}

static void * speed_loop(void * par) {
	
	if (!(speed_Task = rt_task_init_schmod(nam2num("SPEED"), 21, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT PERIODIC TASK\n");
		exit(1);
	}

	unsigned int iseed = (unsigned int)rt_get_time();
  	srand (iseed);

	expected = rt_get_time() + base_period;
	rt_task_make_periodic(speed_Task, expected, 2*base_period);
	rt_make_hard_real_time();

	int value = 800;
	int count = 1;
	int broken_sensor = (int)(((float)SPEED_SIZE) * rand() / ( RAND_MAX + 1.0 ));
	int when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
	while (keep_on_running)
	{
		value += -2 + (int)(5.0 * rand() / ( RAND_MAX + 1.0 ));
		int i;

		//inizio sezione critica
		rt_sem_wait(speed_sem);
		for (i=0; i<SPEED_SIZE; i++) raw_sensor->speeds[i]=value;


		if (count%when_to_break==0) {
			raw_sensor->speeds[broken_sensor]
				+= -2 + (int)(5.0 * rand() / ( RAND_MAX + 1.0 )); // random value failure
			when_to_break = 1 + (int)( 10.0 * rand() / ( RAND_MAX + 1.0 ));
			count = when_to_break;
		}
		//fine sezione critica
		rt_sem_signal(speed_sem);

		count++;
		rt_task_wait_period();
	}
	rt_task_delete(speed_Task);
	return 0;
}

int main(void)
{
	printf("Sensor STARTED!\n");
 	signal(SIGINT, endme);

	if (!(main_Task = rt_task_init_schmod(nam2num("MNTSK"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MAIN TASK\n");
		exit(1);
	}

	//attach to data shared with the voter
	raw_sensor = rtai_malloc(RAW_SEN_SHM, sizeof(struct raw_sensors_data));
	proc_sensor = rtai_malloc(PROC_SEN_SHM, sizeof(struct processed_sensors_data));

	//inizializzazione semafori 
	temp_sem = rt_typed_named_sem_init(TEMP_SEM, 1, BIN_SEM|FIFO_Q);
	speed_sem = rt_typed_named_sem_init(SPEED_SEM, 1, BIN_SEM|FIFO_Q);
	alt_sem = rt_typed_named_sem_init(ALT_SEM, 1, BIN_SEM|FIFO_Q);

	base_period = nano2count(TICK_TIME);
	pthread_create(&temp, NULL, temp_loop, NULL);
	pthread_create(&alt, NULL, alt_loop, NULL);
	pthread_create(&speed, NULL, speed_loop, NULL);

	int i;
	while (keep_on_running) 
	{
		printf("Raw Temp:\t");
		for (i=0; i<TEMP_SIZE-1; i++)
		printf(" %d,",raw_sensor->temperatures[i]);
		printf(" %d\n",	raw_sensor->temperatures[TEMP_SIZE-1]);

		printf("Raw Speed:\t");
		for (i=0; i<SPEED_SIZE-1; i++)
			printf(" %d,",raw_sensor->speeds[i]);
		printf(" %d\n",	raw_sensor->speeds[SPEED_SIZE-1]);

		printf("Raw Alt:\t");
		for (i=0; i<ALTITUDE_SIZE-1; i++)
			printf(" %d,",raw_sensor->altitudes[i]);
		printf(" %d\n",	raw_sensor->altitudes[ALTITUDE_SIZE-1]);
		
		printf("    Temp:\t %d\n",proc_sensor->temperature);
		printf("    Speed:\t %d\n",proc_sensor->speed);
		printf("    Alt:\t %d\n",proc_sensor->altitude);
		rt_sleep(250000000);
	}

	rt_shm_free(RAW_SEN_SHM);
	rt_shm_free(PROC_SEN_SHM);
	rt_task_delete(main_Task);
	rt_named_sem_delete(temp_sem);
	rt_named_sem_delete(speed_sem);
	rt_named_sem_delete(alt_sem);

 	printf("Sensor STOPPED\n");
	return 0;
}




