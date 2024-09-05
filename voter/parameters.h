//DANIELE FAZZARI N46004470

#define TICK_TIME 250000000 //250ms
#define STACK_SIZE 10000

#define ALTITUDE_SIZE 5
#define SPEED_SIZE 3
#define TEMP_SIZE 3

#define RAW_SEN_SHM 121111
#define PROC_SEN_SHM 121112
#define STOP_MSG_SHM 121113

struct raw_sensors_data {
    unsigned int altitudes[ALTITUDE_SIZE]; //uptated every 250ms
    unsigned int speeds[SPEED_SIZE];	   //updated every 500ms
    int temperatures[TEMP_SIZE];	   //uptaded every second
};

struct processed_sensors_data {
    unsigned int altitude;
    unsigned int speed;
    int temperature;
};

struct stop_messages {
	int stop_altitude;
	int stop_speed;
	int stop_temperature;
};

//sem sinc per shm raw_sensor (Sensor - Voter)
#define ALT_SEM "alt_sem"
#define SPEED_SEM "speed_sem"
#define TEMP_SEM "temp_sem"

//sem richieste aperiodiche per shm raw_sensor (Sensor - Monitor)
#define RAW_ALT_SEM_APE "raw_alt_sem_ape"
#define RAW_SPEED_SEM_APE "raw_speed_sem_ape"
#define RAW_TEMP_SEM_APE "raw_temp_sem_ape"

//sem sinc accesso a stop_messages (tra Voter e Monitor)
#define STOP_ALT_SEM "stop_alt_sem"
#define STOP_TEMP_SEM "stop_temp_sem"
#define STOP_SPEED_SEM "stop_speed_sem"

//sem richieste aperiodiche per shm proc_sensor (Voter - Monitor)
#define PROC_ALT_SEM_APE "proc_alt_sem_ape"
#define PROC_SPEED_SEM_APE "proc_speed_sem_ape"
#define PROC_TEMP_SEM_APE "proc_temp_sem_ape"

//mailbox id
#define ERR_MBX_ID "err_mbx_id"
#define MAILBOX_SIZE 256

//WCET tasks e TBS bandwidth
#define WCET_ALT 200000000 
#define WCET_SPEED 100000000 
#define WCET_TEMP 100000000 
#define TBS_BANDWIDTH 0.5 

//id errori
#define TEMP_ERR_ID 1
#define SPEED_ERR_ID 2
#define ALT_ERR_ID 3




