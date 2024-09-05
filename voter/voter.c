//DANIELE FAZZARI N46004470

#include <linux/module.h>
#include <asm/io.h>
#include <asm/rtai.h>
#include "parameters.h"
#include <linux/module.h>
#include <rtai_shm.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_mbx.h> //libreria mailbox RTAI

#define NTASKS 3
static RT_TASK tasks[NTASKS];
static struct raw_sensors_data * raw_sensors_data_ptr;
static struct processed_sensors_data * processed_sensors_data_ptr;
static RTIME now;
static RTIME tick_period;

#include <linux/moduleparam.h> 
static int altitude_fun_activator = 1;
static int speed_fun_activator = 1;
static int temperature_fun_activator = 1;
module_param(altitude_fun_activator, int, 0664);
module_param(speed_fun_activator, int, 0664);
module_param(temperature_fun_activator, int, 0664);
MODULE_PARM_DESC(altitude_fun_activator, "activation parameter altitude_fun");
MODULE_PARM_DESC(speed_fun_activator, "activation parameter speed_fun");
MODULE_PARM_DESC(temperature_fun_activator, "activation parameter temperature_fun");
 
SEM* temp_sem;
SEM* speed_sem;
SEM* alt_sem;
SEM* stop_alt_sem;
SEM* stop_temp_sem;
SEM* stop_speed_sem;
SEM* proc_alt_sem_ape;
SEM* proc_temp_sem_ape;
SEM* proc_speed_sem_ape;

//mailbox per messaggio di errore
MBX* error_mbx; 
static struct stop_messages * stop_messages_ptr;

static void altitude_fun(long p)
{
	int i,k;
	int count, found;
	int v[ALTITUDE_SIZE];
	int overrun = 0;
	long long slack = 0;
	int send = 0; //send=1 se rilevata anomalia
	int keep_alt_processing = 1; //keep_alt_processing = 0 se rilevati 20 errori
	
	while(keep_alt_processing){
		rt_printk(" ***** last SLACK altitude_task = %d *****\n", slack);

		rt_sem_wait(alt_sem);
		for(i = 0; i<ALTITUDE_SIZE; i++) {
		v[i]=raw_sensors_data_ptr->altitudes[i];
		}
		rt_sem_signal(alt_sem);
		
		i = 0;
		found = 0;
		while(i<ALTITUDE_SIZE && !found){
			k = 0;
			count = 0;
			if (v[i]!=0) {
				while(k < ALTITUDE_SIZE && count<2){	
					if(v[i]==v[k] && i!=k){
						count++; 
					}
					else if(v[i]!=v[k]){
						//errore di misurazione
						send = 1;
					}
					k++;
				}
				if(count>=2){  
					found =1;
				}
			}
			else send = 1; //errore stuck_at_zero
			i++;
		} 

		rt_sem_wait(proc_alt_sem_ape);
		processed_sensors_data_ptr->altitude = v[i-1];
		rt_sem_signal(proc_alt_sem_ape);
		
		if(send){ //invio messaggio di errore 3
			rt_mbx_send(error_mbx, (void*)ALT_ERR_ID, sizeof(int));	
		}
	
		rt_sem_wait(stop_alt_sem);
		if(stop_messages_ptr -> stop_altitude == 1){
			keep_alt_processing = 0;
			rt_sem_wait(alt_sem);
			processed_sensors_data_ptr -> altitude = -1;
			rt_sem_signal(alt_sem);	
		}
		rt_sem_signal(stop_alt_sem);

		slack = count2nano(next_period()-rt_get_time());
		overrun = rt_task_wait_period();
		if(overrun == RTE_TMROVRN) {
		rt_printk(" ***** OVERRUN altitude_task *****\n" );
		}
	}
}

static void speed_fun(long p)
{
	int j = 0;
	int v[SPEED_SIZE];
	int overrun = 0;
	long long slack = 0;
	int keep_speed_processing = 1; //keep_speed_processing=0 se rilevati 10 errori
	int pos = 0;

	while(keep_speed_processing)
	{	
		rt_printk(" ***** last SLACK speed_task = %d *****\n", slack);

		//inizio sezione critica
		rt_sem_wait(speed_sem);
		for(j = 0; j<SPEED_SIZE; j++) {
		v[j]=raw_sensors_data_ptr->speeds[j];
		}
		//fine sezione critica
		rt_sem_signal(speed_sem);

		if(v[0]==v[1] || v[0]==v[2]){
			pos = 0;		
		}
		else pos = 2;

		rt_sem_wait(proc_speed_sem_ape);
		processed_sensors_data_ptr->speed = v[pos];
		rt_sem_signal(proc_speed_sem_ape);

		//se una delle misurazioni è errata
		if(v[0]!=v[1] || v[1]!=v[2] || v[0]!=v[2]) {
			rt_mbx_send(error_mbx, (void*)SPEED_ERR_ID, sizeof(int));
		}

		rt_sem_wait(stop_speed_sem);
		if(stop_messages_ptr -> stop_speed == 1){
			keep_speed_processing = 0;
			rt_sem_wait(speed_sem);
			processed_sensors_data_ptr -> speed = -1;
			rt_sem_signal(speed_sem);	
		}
		rt_sem_signal(stop_speed_sem);	

		slack = count2nano(next_period()-rt_get_time());
		overrun = rt_task_wait_period();
		
		if(overrun == RTE_TMROVRN) {
		rt_printk(" ***** OVERRUN speed_task *****\n" );
		}	
	}
}

static void temperature_fun(long p)
{
	int i,k;
	int found = 0;
	int v[TEMP_SIZE];
	int overrun = 0;
	long long slack = 0;
	int keep_temperature_processing = 1; //keep_temperature_processing=0 se rilevati 10 errori
	
	while(keep_temperature_processing){
		rt_printk(" ***** last SLACK temperature_task = %d *****\n", slack);

		//inizio sezione critica
		rt_sem_wait(temp_sem);
		for(i = 0; i<TEMP_SIZE; i++) {
		v[i]=raw_sensors_data_ptr->temperatures[i];
		}
		//fine sezione critica
		rt_sem_signal(temp_sem);

		i = 0;
		found = 0;

		while( i<TEMP_SIZE && !found){
			k = 0;
			if (v[i]!=0) {
				while(k < TEMP_SIZE){	
					if(v[i]==v[k] && i!=k){ 
						found = 1;
					}
					k++;
				}
			}
			//send direttamente nell'else perchè entro al massimo una volta nell'else
			else rt_mbx_send(error_mbx, (void*)TEMP_ERR_ID, sizeof(int));
			i++;
		} 

		rt_sem_wait(proc_temp_sem_ape);
		processed_sensors_data_ptr->temperature = v[i-1];
		rt_sem_signal(proc_temp_sem_ape);

		rt_sem_wait(stop_temp_sem);
		if(stop_messages_ptr -> stop_temperature == 1){
			keep_temperature_processing = 0;
			rt_sem_wait(temp_sem);
			processed_sensors_data_ptr -> temperature = -1;
			rt_sem_signal(temp_sem);	
		}
		rt_sem_signal(stop_temp_sem);		

		slack = count2nano(next_period()-rt_get_time());
		overrun = rt_task_wait_period();
		if(overrun == RTE_TMROVRN) {
		rt_printk(" ***** OVERRUN temperature_task *****\n" );
		}	
	}
}

int init_module(void)
{
	printk("\n***** VOTER MODULE INIT ***** \n");

	rt_task_init_cpuid(&tasks[0], (void *)altitude_fun, 1, STACK_SIZE, 1, 0, 0, 0);
	rt_task_init_cpuid(&tasks[1], (void *)speed_fun, 1, STACK_SIZE, 2, 0, 0, 0);
	rt_task_init_cpuid(&tasks[2], (void *)temperature_fun, 1, STACK_SIZE, 3, 0, 0, 0);

	//init semafori sinc su raw_sensors_data
	alt_sem = rt_typed_named_sem_init(ALT_SEM, 1, BIN_SEM|FIFO_Q);
	speed_sem = rt_typed_named_sem_init(SPEED_SEM, 1, BIN_SEM|FIFO_Q);
	temp_sem = rt_typed_named_sem_init(TEMP_SEM, 1, BIN_SEM|FIFO_Q);
	
	//init mailbox messaggi di errore trovato
	error_mbx = rt_typed_named_mbx_init(ERR_MBX_ID, MAILBOX_SIZE, PRIO_Q); 
	
	//init semafori sinc su stop_messages shm
	stop_alt_sem = rt_typed_named_sem_init(STOP_ALT_SEM, 1, BIN_SEM|FIFO_Q);
	stop_temp_sem = rt_typed_named_sem_init(STOP_TEMP_SEM, 1, BIN_SEM|FIFO_Q);
	stop_speed_sem = rt_typed_named_sem_init(STOP_SPEED_SEM, 1, BIN_SEM|FIFO_Q);

	//init 
	proc_alt_sem_ape = rt_typed_named_sem_init(PROC_ALT_SEM_APE, 1, BIN_SEM|FIFO_Q);
	proc_temp_sem_ape = rt_typed_named_sem_init(PROC_TEMP_SEM_APE, 1, BIN_SEM|FIFO_Q);
	proc_speed_sem_ape = rt_typed_named_sem_init(PROC_SPEED_SEM_APE, 1, BIN_SEM|FIFO_Q);

	//init shm 	
 	raw_sensors_data_ptr = rtai_kmalloc(RAW_SEN_SHM, 1); 
	processed_sensors_data_ptr = rtai_kmalloc(PROC_SEN_SHM, 1); 
	stop_messages_ptr = rtai_kmalloc(STOP_MSG_SHM, sizeof(struct stop_messages));
	stop_messages_ptr -> stop_altitude = 0;
	stop_messages_ptr -> stop_speed = 0;
	stop_messages_ptr -> stop_temperature = 0;

	tick_period = nano2count(TICK_TIME);
	now = rt_get_time()+ NTASKS*tick_period;
	if(altitude_fun_activator==1){
		rt_task_make_periodic(&tasks[0], now, tick_period); 
	}
	if(speed_fun_activator==1){
		rt_task_make_periodic(&tasks[1], now, 2*tick_period); 
	}
	if(temperature_fun_activator==1){
		rt_task_make_periodic(&tasks[2], now, 4*tick_period);
	}
	rt_spv_RMS(0);
	return 0;
}


void cleanup_module(void)
{	
	int i = 0;
	for (i = 0; i < NTASKS; i++) {
		rt_task_delete(&tasks[i]);
	}

	rtai_kfree(RAW_SEN_SHM);
	rtai_kfree(PROC_SEN_SHM);
	rtai_kfree(STOP_MSG_SHM);		

	//delete riferimenti semafori, mbx 
	rt_named_sem_delete(temp_sem);
	rt_named_sem_delete(speed_sem);
	rt_named_sem_delete(alt_sem);
	rt_named_mbx_delete(error_mbx);
	rt_named_sem_delete(stop_alt_sem);
	rt_named_sem_delete(stop_temp_sem);
	rt_named_sem_delete(stop_speed_sem);

	printk("\n***** CLEANUP MODULE INIT ***** \n");
}
