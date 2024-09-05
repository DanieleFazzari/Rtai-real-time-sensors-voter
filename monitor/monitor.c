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
#define CPUMAP 0x1

#include <pthread.h> //libreria pthread linux
#include <rtai_sem.h> //libreria semafori RTAI  
#include <rtai_mbx.h> //libreria mailbox RTAI
#include <rtai_msg.h> //libreria messaggi RTAI

static RT_TASK * TBS; 
static RT_TASK * ALT_APE; 
static RT_TASK * TEMP_APE;
static RT_TASK * SPEED_APE;
static RT_TASK * MAIN;

static pthread_t TBS_thread;
static pthread_t altitude_thread;
static pthread_t temperature_thread;
static pthread_t speed_thread;

RTIME sampl_interv;
static RTIME deadline = 0; 
static int counter_alt = 0; 
static int counter_temp = 0; 
static int counter_speed = 0; 

MBX* error_mbx;  

//semafori sinc su processed_sensors_data
SEM* proc_alt_sem_ape;
SEM* proc_speed_sem_ape;
SEM* proc_temp_sem_ape;

//semafori sinc su shm raw_sensors_data
SEM* raw_alt_sem_ape;
SEM* raw_speed_sem_ape;
SEM* raw_temp_sem_ape;

//semafori sinc su shm stop_messages
SEM* stop_alt_sem;
SEM* stop_temp_sem;
SEM* stop_speed_sem;

//per far terminare il tbs
static int keep_on_running = 1;

//puntatori alle shared memory
static struct raw_sensors_data * raw_sensors_data_ptr;
static struct processed_sensors_data * processed_sensors_data_ptr;
static struct stop_messages * stop_messages_ptr;

void * Total_bandwidth_server(void * param)
{
	if (!(TBS = rt_task_init_schmod(nam2num("Total_bandwidth_server"), 1, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT TOTAL BANDWIDTH SERVER TASK\n");
		exit(1);
	}
	rt_make_hard_real_time();

	int cod_err;
	while(keep_on_running)
	{
		if(!rt_mbx_receive(error_mbx, &cod_err, sizeof(int)))
		{
			switch(cod_err) 
			{
				case 1:
				/*RT_TASK* rt_send (RT_TASK* task, unsigned int msg)*/
				rt_send(TEMP_APE, cod_err);
				break;

				case 2:
				rt_send(SPEED_APE, cod_err);
				break;
			
				case 3:
				rt_send(ALT_APE, cod_err);				
				break;		
			}
		}
	}
	rt_task_delete(TBS);
	return 0;
}

void * temperature_aperiodic_request(void * param)
{
	if (!(TEMP_APE = rt_task_init_schmod(nam2num("TEMP_APE"), 1, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT TEMP ERROR TASK\n");
		exit(1);
	}
	rt_make_hard_real_time();

	int i = 0;
	int* cod_err;
	int v[TEMP_SIZE];
	int processed_temperature = 0;
	
	while(counter_temp < 10)
	{
		rt_receive(TBS, &cod_err); //attende TBS
		counter_temp++; //conta che è arrivato un alert di errore
		RTIME now = rt_get_time();
		//calcola la propria deadline (assoluta) con la tecnica del tbs
		if(now > deadline){
		deadline = now + nano2count(WCET_ALT / TBS_BANDWIDTH);
		}
		else deadline = deadline + nano2count(WCET_ALT / TBS_BANDWIDTH);
		/*SIGNATURE: void rt_task_set_resume_end_times(RTIME resume_time, RTIME end_time);*/
		rt_task_set_resume_end_times(rt_get_time(), deadline);

		rt_sem_wait(proc_alt_sem_ape);
		rt_sem_wait(raw_alt_sem_ape);
		for(i = 0; i<TEMP_SIZE; i++) {
		v[i]=raw_sensors_data_ptr->temperatures[i];
		}
		processed_temperature = processed_sensors_data_ptr->temperature;
		rt_sem_signal(raw_alt_sem_ape);
		rt_sem_signal(proc_alt_sem_ape);

		for(i=0; i<TEMP_SIZE; i++)		
			rt_send(MAIN, v[i]);
		rt_send(MAIN, processed_temperature);

	}
	//ricevute 10 segnalazioni di errore
	rt_sem_wait(stop_temp_sem);
	stop_messages_ptr -> stop_temperature = 1;
	rt_sem_signal(stop_temp_sem);


	rt_task_delete(TEMP_APE);
	return 0;
}

void * speed_aperiodic_request(void * param)
{
	if (!(SPEED_APE = rt_task_init_schmod(nam2num("SPEED_APE"), 1, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT SPEED ERROR TASK\n");
		exit(1);
	}
	rt_make_hard_real_time();

	int cod_err;
	int i = 0;
	unsigned int v[SPEED_SIZE];
	unsigned int processed_speed = 0;

	while(counter_speed<10)
	{
		rt_receive(TBS, &cod_err); //attende TBS
		counter_speed++; //conta che è arrivato un alert di errore
		RTIME now = rt_get_time();
		//calcola la propria deadline (assoluta) con la tecnica del tbs
		if(now > deadline){
			deadline = now + nano2count(WCET_ALT / TBS_BANDWIDTH);
		}
		else deadline = deadline + nano2count(WCET_ALT / TBS_BANDWIDTH);
		/*SIGNATURE: void rt_task_set_resume_end_times(RTIME resume_time, RTIME end_time);*/
		rt_task_set_resume_end_times(rt_get_time(), deadline);
		rt_sem_wait(proc_alt_sem_ape);
		rt_sem_wait(raw_alt_sem_ape);
		for(i = 0; i<SPEED_SIZE; i++) {
			v[i]=raw_sensors_data_ptr->speeds[i];
		}
		processed_speed = processed_sensors_data_ptr->speed;
		rt_sem_signal(raw_alt_sem_ape);
		rt_sem_signal(proc_alt_sem_ape);

		for(i=0; i<SPEED_SIZE; i++)		
			rt_send(MAIN, v[i]); 
		rt_send(MAIN, processed_speed); 
	}
	//ricevute 20 segnalazioni
	rt_sem_wait(stop_speed_sem);
	stop_messages_ptr -> stop_speed = 1;
	rt_sem_signal(stop_speed_sem);
	rt_task_delete(SPEED_APE);
	return 0;	
}

void * altitude_aperiodic_request(void * param)
{
	if (!(ALT_APE = rt_task_init_schmod(nam2num("ALT_APE"), 1, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT ALT ERROR TASK\n");
		exit(1);
	}
	rt_make_hard_real_time();

	int i = 0;
	int cod_err;
	unsigned int v[ALTITUDE_SIZE];
	unsigned int processed_altitude = 0;

	while(counter_alt<20)
	{
		rt_receive(TBS, &cod_err); //attende TBS
		counter_alt++; //conta che è arrivato un alert di errore
		RTIME now = rt_get_time();
		//calcola la propria deadline (assoluta) con la tecnica del tbs
		if(now > deadline){
			deadline = now + nano2count(WCET_ALT / TBS_BANDWIDTH);
		}
		else deadline = deadline + nano2count(WCET_ALT / TBS_BANDWIDTH);
		/*SIGNATURE: void rt_task_set_resume_end_times(RTIME resume_time, RTIME end_time);*/
		rt_task_set_resume_end_times(rt_get_time(), deadline);
	
		rt_sem_wait(proc_alt_sem_ape);
		rt_sem_wait(raw_alt_sem_ape);
		for(i = 0; i<ALTITUDE_SIZE; i++) {
			v[i]=raw_sensors_data_ptr->altitudes[i];
		}
		processed_altitude = processed_sensors_data_ptr->altitude;
		rt_sem_signal(raw_alt_sem_ape);
		rt_sem_signal(proc_alt_sem_ape);

		for(i=0; i<ALTITUDE_SIZE; i++)		
			rt_send(MAIN, v[i]); 
		rt_send(MAIN, processed_altitude); 
	}
	rt_sem_wait(stop_alt_sem);
	stop_messages_ptr -> stop_altitude = 1;
	rt_sem_signal(stop_alt_sem);
	rt_task_delete(ALT_APE);	
	return 0;
}

int main(void)
{
	printf("Monitor task started!");
	sampl_interv = nano2count(TICK_TIME); 

	if (!(MAIN = rt_task_init_schmod(nam2num("MAIN"), 0, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("Failed init total bandwidth server\n");
		exit(1);
	}
	error_mbx = rt_typed_named_mbx_init(ERR_MBX_ID, MAILBOX_SIZE, PRIO_Q);

	//init semafori sincronizzazione su processed_sensors_data
	proc_alt_sem_ape = rt_typed_named_sem_init(PROC_ALT_SEM_APE, 1, BIN_SEM|FIFO_Q);
	proc_speed_sem_ape = rt_typed_named_sem_init(PROC_SPEED_SEM_APE, 1, BIN_SEM|FIFO_Q);
	proc_temp_sem_ape = rt_typed_named_sem_init(PROC_TEMP_SEM_APE, 1, BIN_SEM|FIFO_Q);

	//init semafori sincronizzazione su raw_sensors_data
	raw_alt_sem_ape = rt_typed_named_sem_init(RAW_ALT_SEM_APE, 1, BIN_SEM|FIFO_Q);
	raw_speed_sem_ape = rt_typed_named_sem_init(RAW_SPEED_SEM_APE, 1, BIN_SEM|FIFO_Q);
	raw_temp_sem_ape = rt_typed_named_sem_init(RAW_TEMP_SEM_APE, 1, BIN_SEM|FIFO_Q);
		
	//init semafori sinc su stop_messages shm
	stop_alt_sem = rt_typed_named_sem_init(STOP_ALT_SEM, 1, BIN_SEM|FIFO_Q);
	stop_temp_sem = rt_typed_named_sem_init(STOP_TEMP_SEM, 1, BIN_SEM|FIFO_Q);
	stop_speed_sem = rt_typed_named_sem_init(STOP_SPEED_SEM, 1, BIN_SEM|FIFO_Q);

	//riferimenti alle shared memory processed_sensors_data, raw_sensors_data, stop_message
	raw_sensors_data_ptr = rtai_malloc(RAW_SEN_SHM, sizeof(struct raw_sensors_data)); 
	processed_sensors_data_ptr = rtai_malloc(PROC_SEN_SHM, sizeof(struct processed_sensors_data)); 
	stop_messages_ptr = rtai_malloc(STOP_MSG_SHM, sizeof(struct stop_messages));

	pthread_create(&TBS_thread,NULL , Total_bandwidth_server, NULL);
	pthread_create(&altitude_thread,NULL ,altitude_aperiodic_request , NULL);
	pthread_create(&speed_thread,NULL ,speed_aperiodic_request , NULL);
	pthread_create(&temperature_thread,NULL ,temperature_aperiodic_request , NULL);
	
	int temp_rec = 0;
	unsigned int alt_rec = 0;
	unsigned int speed_rec = 0;
	int count = 0;
	int i = 0;

	while(keep_on_running)
	{
		if(rt_receive_if(TEMP_APE, &temp_rec)){
			count++;
			printf("Warning task TEMP: %d ", temp_rec);
			for(i=1; i<TEMP_SIZE-2; i++){
				rt_receive_if(TEMP_APE, &temp_rec);	
				printf("%d ", temp_rec);	
			}
			if(rt_receive_if(TEMP_APE, &temp_rec)) printf("-> %d\n", temp_rec);
		}

		if(rt_receive_if(ALT_APE, &alt_rec)){
			count++;
			printf("Warning task ALT: %u ", alt_rec);
			for(i=1; i<ALTITUDE_SIZE-2; i++){
				rt_receive_if(ALT_APE, &alt_rec);			
				printf("%u ", alt_rec);	
			}
			if(rt_receive_if(ALT_APE, &alt_rec)) printf("-> %u\n", alt_rec);
		}

		if(rt_receive_if(SPEED_APE, &speed_rec)){
			count++;
			printf("Warning task SPEED: %u ", speed_rec);
			for(i=1; i<SPEED_SIZE-2; i++){
				rt_receive_if(SPEED_APE, &speed_rec);			
				printf("%u ", speed_rec);	
			}
			if(rt_receive_if(SPEED_APE, &speed_rec)) printf("-> %u\n", speed_rec);
		}

		if(count==30) keep_on_running=0;	
	}
	
	rt_mbx_delete(error_mbx);
	rt_named_sem_delete(proc_alt_sem_ape);	
	rt_named_sem_delete(proc_speed_sem_ape);
	rt_named_sem_delete(proc_temp_sem_ape);
	rt_named_sem_delete(raw_alt_sem_ape);	
	rt_named_sem_delete(raw_speed_sem_ape);
	rt_named_sem_delete(raw_temp_sem_ape);
	rt_named_sem_delete(stop_alt_sem);
	rt_named_sem_delete(stop_temp_sem);
	rt_named_sem_delete(stop_speed_sem);

	//elimino riferimenti alle shm
	rtai_free(RAW_SEN_SHM, raw_sensors_data_ptr);
	rtai_free(PROC_SEN_SHM, processed_sensors_data_ptr);
	rtai_free(STOP_MSG_SHM, stop_messages_ptr);	
}