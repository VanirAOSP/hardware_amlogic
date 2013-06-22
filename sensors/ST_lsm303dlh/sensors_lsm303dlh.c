/******************** (C) COPYRIGHT 2009 STMicroelectronics ********************
* File Name          : sensors_lsm303dlh.c
* Department         : STMicroelectronics APM MEMS G.C. FAE TEAM 
* Author             : Travis Tu <travis.tu@st.com>
* Date First Issued  : 2010.01.01
********************************************************************************
* History:
* 2010.7.10  V1.0
********************************************************************************
* THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR REFERENCE OR 
* EDUCATION. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY 
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING 
* FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE 
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* this implements a sensors hardware library for the Android emulator.
 * the following code should be built as a shared library that will be
 * placed into /system/lib/hw/sensors.goldfish.so
 *
 * it will be loaded by the code in hardware/libhardware/hardware.c
 * which is itself called from com_android_server_SensorService.cpp
 */


#include <linux/ioctl.h>  /* For IOCTL macros */

#define LSM303DLH_ACC_IOCTL_BASE 'a'
/* The following define the IOCTL command values via the ioctl macros */
#define LSM303DLH_ACC_IOCTL_SET_DELAY   _IOW(LSM303DLH_ACC_IOCTL_BASE, 0, int)
#define LSM303DLH_ACC_IOCTL_GET_DELAY   _IOR(LSM303DLH_ACC_IOCTL_BASE, 1, int)
#define LSM303DLH_ACC_IOCTL_SET_ENABLE  _IOW(LSM303DLH_ACC_IOCTL_BASE, 2, int)
#define LSM303DLH_ACC_IOCTL_GET_ENABLE  _IOR(LSM303DLH_ACC_IOCTL_BASE, 3, int)
#define LSM303DLH_ACC_IOCTL_SET_G_RANGE _IOW(LSM303DLH_ACC_IOCTL_BASE, 4, int)

#define LSM303DLH_MAG_IOCTL_BASE 'm'
/* The following define the IOCTL command values via the ioctl macros */
#define LSM303DLH_MAG_IOCTL_SET_DELAY   _IOW(LSM303DLH_MAG_IOCTL_BASE, 0, int)
#define LSM303DLH_MAG_IOCTL_GET_DELAY   _IOR(LSM303DLH_MAG_IOCTL_BASE, 1, int)
#define LSM303DLH_MAG_IOCTL_SET_ENABLE  _IOW(LSM303DLH_MAG_IOCTL_BASE, 2, int)
#define LSM303DLH_MAG_IOCTL_GET_ENABLE  _IOR(LSM303DLH_MAG_IOCTL_BASE, 3, int)
#define LSM303DLH_MAG_IOCTL_SET_H_RANGE _IOW(LSM303DLH_MAG_IOCTL_BASE, 4, int)

#define  SENSORS_SERVICE_NAME "sensors"

#define LOG_TAG "LSM303DLH"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <cutils/sockets.h>
#include <hardware/sensors.h>
#include <linux/input.h>
#include <pthread.h>
#include "MEMSAlgLib_eCompass.h"

#if 1
#define  D(...)  LOGD(__VA_ARGS__)
#else
#define  D(...)  ((void)0)
#endif

#define  E(...)  LOGE(__VA_ARGS__)

/** SENSOR IDS AND NAMES
 **/

#define MAX_NUM_SENSORS 4

#define SUPPORTED_SENSORS  ((1<<MAX_NUM_SENSORS)-1)

#define  ID_BASE           SENSORS_HANDLE_BASE
#define  ID_ACCELERATION   (ID_BASE+0)
#define  ID_MAGNETIC_FIELD (ID_BASE+1)
#define  ID_ORIENTATION    (ID_BASE+2)
#define  ID_TEMPERATURE    (ID_BASE+3)

#define  SENSORS_ACCELERATION   (1 << ID_ACCELERATION)
#define  SENSORS_MAGNETIC_FIELD  (1 << ID_MAGNETIC_FIELD)
#define  SENSORS_ORIENTATION     (1 << ID_ORIENTATION)
#define  SENSORS_TEMPERATURE     (1 << ID_TEMPERATURE)

#define  ID_CHECK(x)  ((unsigned)((x)-ID_BASE) < 4)

#define  SENSORS_LIST  \
    SENSOR_(ACCELERATION,"acceleration") \
    SENSOR_(MAGNETIC_FIELD,"magnetic-field") \
    SENSOR_(ORIENTATION,"orientation") \
	SENSOR_(TEMPERATURE,"temperature") \

/* return the current time in nanoseconds */
static int64_t
data__now_ns(void)
{
    struct timespec  ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static const struct {
    const char*  name;
    int          id; } _sensorIds[MAX_NUM_SENSORS] =
{
#define SENSOR_(x,y)  { y, ID_##x },
    SENSORS_LIST
#undef  SENSOR_
};

static const char*
_sensorIdToName( int  id )
{
    int  nn;

    for (nn = 0; nn < MAX_NUM_SENSORS; nn++)
        if (id == _sensorIds[nn].id)
            return _sensorIds[nn].name;
    return "<UNKNOWN>";
}

static int acc_event_fd, mag_event_fd;
static uint32_t active_sensors;

static int
_sensorIdFromName( const char*  name )
{
    int  nn;

    if (name == NULL)
        return -1;

    for (nn = 0; nn < MAX_NUM_SENSORS; nn++)
        if (!strcmp(name, _sensorIds[nn].name))
            return _sensorIds[nn].id;

    return -1;
}

/** SENSORS CONTROL DEVICE
 **
 ** This one is used to send commands to the sensors drivers.
 ** We implement this by sending ioctl to accel/magne misc
 ** devices
 **/

typedef struct SensorControl {
	struct sensors_control_device_t  device;
	uint32_t                         active_sensors;
} SensorControl;

/*
 * read/write calibration data in /data/system/sensors.dat
 */
static int read_sensor_calibration_data(int *mx, int *my, int *mz)
{
	unsigned long fd = fopen("/data/system/sensors.dat1", "r");
	if (fd > 0) 
  {
		if(fscanf(fd, "%d\n%d\n%d\n", mx, my, mz)==3)
    {
			D("%s: valid calibration data, %d %d %d", __func__, *mx, *my, *mz);
      fclose(fd);
      return 0;
    }
    fclose(fd);
	}
  D("%s: without valid calibration data file", __func__);
  return -1;
}

static int write_sensor_calibration_data(int mx, int my, int mz)
{
	unsigned long fd = fopen("/data/system/sensors.dat1", "w");
	fprintf(fd, "%d\n%d\n%d\n", mx, my, mz);
	fclose(fd);
  D("%s: valid calibration data, %d %d %d", __func__, mx, my, mz);
	return 0;
}

static int set_delay(int ms)
{
	int acc_fd, mag_fd;
	static int old_ms = 0;

	if (old_ms == ms)
		return 0;

	acc_fd = open("/dev/lsm303dlh_acc", O_RDONLY);
	mag_fd = open("/dev/lsm303dlh_mag", O_RDONLY);

	if ((acc_fd > 0) && (mag_fd > 0)) {
		LOGD_IF(1, "%s: ms=%d", __FUNCTION__, ms);
		ioctl(acc_fd, LSM303DLH_ACC_IOCTL_SET_DELAY, &ms);
		ioctl(mag_fd, LSM303DLH_MAG_IOCTL_SET_DELAY, &ms);
		close(acc_fd);
		close(mag_fd);
		old_ms = ms;
		return 0;
	} else {
		return -1;
	}
}




static int open_inputs(int *acc_event_fd, int *mag_event_fd)
{
	int acc_event, mag_event;
	acc_event = open("/dev/input/event2", O_RDWR);
	mag_event = open("/dev/input/event3", O_RDWR);
	if (acc_event < 0 || mag_event < 0)
		return -1;
	else {
			*acc_event_fd = acc_event;
			*mag_event_fd = mag_event;
			return 0;
	}
}

//#define UNIX_DOMAIN "unix.domain"
static native_handle_t*
control__open_data_source(struct sensors_control_device_t *dev)
{
	native_handle_t* handle;
	int acc_event_fd, mag_event_fd;
	D("%s", __FUNCTION__);

	if (open_inputs(&acc_event_fd, &mag_event_fd) < 0)
		return NULL;


	handle = native_handle_create(2, 0);
	handle->data[0] = acc_event_fd;
	handle->data[1] = mag_event_fd;

	return handle;
}

static int
control__activate(struct sensors_control_device_t *dev,
	int handle,
	int enabled)
{
	D("%s", __FUNCTION__);
	SensorControl*  ctl = (void*)dev;
    uint32_t        mask, sensors, active, new_sensors, changed;
    int             ret;

    D("%s: handle=%s (%d) enabled=%d", __FUNCTION__,
        _sensorIdToName(handle), handle, enabled);

    if (!ID_CHECK(handle)) {
        E("%s: bad handle ID", __FUNCTION__);
        return -1;
    }

    mask    = (1<<handle);
    sensors = enabled ? mask : 0;

    active      = active_sensors;
    new_sensors = (active & ~mask) | (sensors & mask);
    changed     = active ^ new_sensors;

    if (!changed)
        return 0;

    active_sensors = new_sensors;

	return 0;
}

static int
control__set_delay(struct sensors_control_device_t *dev, int32_t ms)
{
	D("%s", __FUNCTION__);
	return 0;
}

/* this function is used to force-stop the blocking read() in
 * data__poll. In order to keep the implementation as simple
 * as possible here, we send a command to the emulator which
 * shall send back an appropriate data block to the system.
 */
static int
control__wake(struct sensors_control_device_t *dev)
{
	D("%s", __FUNCTION__);
    return 0;
}

static int
control__close(struct hw_device_t *dev)
{
	D("%s", __FUNCTION__);
	return 0;
}

/** SENSORS DATA DEVICE
 **
 ** This one is used to read sensor data from the hardware.
 **/

typedef struct SensorData {
    struct sensors_data_device_t  device;
    sensors_data_t                sensors[MAX_NUM_SENSORS];
	uint32_t                      pendingSensors;
} SensorData;

static int
data__data_open(struct sensors_data_device_t *dev, native_handle_t* handle)
{
	SensorData*  data = (void*)dev;
	int i;
	D("%s: dev=%p", __FUNCTION__, dev);
	memset(&data->sensors, 0, sizeof(data->sensors));

	for (i=0 ; i<MAX_NUM_SENSORS ; i++) {
		data->sensors[i].vector.status = SENSOR_STATUS_ACCURACY_HIGH;
	}

  
	if (acc_event_fd < 0 || mag_event_fd < 0) {
		acc_event_fd = dup(handle->data[0]);
		mag_event_fd = dup(handle->data[1]);;
		if ((acc_event_fd < 0) || (mag_event_fd < 0)) {
			LOGE("%s fail to open input devices",__FUNCTION__);
			return -1;
		} else {
			int mx, my, mz;
			MEMSAlgLib_eCompass_Init(1000,1000); 

			//if find the last record of calibration data
			if(!read_sensor_calibration_data(&mx, &my, &mz)) //platform related API
				MEMSAlgLib_eCompass_SetCalibration(mx, my, mz);

		}
	}

	data->pendingSensors = 0;
	native_handle_close(handle);
	native_handle_delete(handle);
	return 0;
}

static int
data__data_close(struct sensors_data_device_t *dev)
{
	SensorData*  data = (void*)dev;
    D("%s: dev=%p", __FUNCTION__, dev);
	if (acc_event_fd > 0){
		close(acc_event_fd);
		close(mag_event_fd);
		acc_event_fd = -1;
	}

	return 0;
}

int get_events_val(ECOM_ValueTypeDef *data)
{
	struct input_event acc_event;
	struct input_event mag_event;
	int ret;

	while(1) {
		ret = read(acc_event_fd, &acc_event, sizeof(struct input_event));
		if (ret != sizeof(struct input_event))
			continue;

		if(acc_event.type == EV_ABS) {
			if(acc_event.code == ABS_X)
				data->ax = acc_event.value; 
			if(acc_event.code == ABS_Y)
				data->ay = acc_event.value;   
			if(acc_event.code == ABS_Z)
				data->az = acc_event.value;  
		}

		if(acc_event.type == 0)
			break;
	}

	while(1) {
		ret = read(mag_event_fd, &mag_event, sizeof(struct input_event));
		if (ret != sizeof(struct input_event))
			continue;

		if(mag_event.type == EV_ABS) {
			if(mag_event.code == ABS_X)
				data->mx = mag_event.value;  
			if(mag_event.code == ABS_Y)
				data->my = mag_event.value;   
			if(mag_event.code == ABS_Z)
				data->mz = mag_event.value;  
		}
		if(mag_event.type == 0)
			break;
	}

	data->time = data__now_ns()/1000000;

	LOGD_IF(0, "%s %d %d %d %d %d %d %d\n", __FUNCTION__, data->ax, data->ay, data->az, data->mx, data->my, data->mz, data->time);

	return 0;
}

static int
pick_sensor(SensorData*      data,
	sensors_data_t*  values)
{
	uint32_t mask = SUPPORTED_SENSORS;
	while (mask) {
		uint32_t i = 31 - __builtin_clz(mask);
		mask &= ~(1<<i);
		if (data->pendingSensors & (1<<i)) {
			data->pendingSensors &= ~(1<<i);
			*values = data->sensors[i];
			values->sensor = (1<<i);
			LOGD_IF(0, "%s: %d [%f, %f, %f]", __FUNCTION__,
				(1<<i),
				values->vector.x,
				values->vector.y,
				values->vector.z);
			return i;
		}
	}
	LOGE("No sensor to return!!! pendingSensors=%08x", data->pendingSensors);
	return -1;
}

static int
data__poll(struct sensors_data_device_t *dev, sensors_data_t* values)
{
	SensorData*  data = (void*)dev;
	static ECOM_ValueTypeDef event_val;
  
  //D("%s: dev=%p", __FUNCTION__, dev);
  
	// wait until we get a complete event for an enabled sensor
	uint32_t new_sensors = 0;
	if (acc_event_fd < 0 || mag_event_fd < 0)
		return -1;

	// there are pending sensors, returns them now...
	if (data->pendingSensors) {
		return pick_sensor(data, values);
	}

	get_events_val(&event_val); 

	new_sensors |= SENSORS_ACCELERATION;
	data->sensors[ID_ACCELERATION].acceleration.x = event_val.ay*GRAVITY_EARTH/1000.0f;  
	data->sensors[ID_ACCELERATION].acceleration.y = -event_val.ax*GRAVITY_EARTH/1000.0f;  
	data->sensors[ID_ACCELERATION].acceleration.z = -event_val.az*GRAVITY_EARTH/1000.0f;  

	/*
	 * For SystemServer process(active_sensors != 0, it only cares about acceleration
	 */
	if (active_sensors == 0) {   
    if(MEMSAlgLib_eCompass_IsCalibrated())
    {
      short MAGOFFX1,MAGOFFY1,MAGOFFZ1;
      MEMSAlgLib_eCompass_GetCalibration(&MAGOFFX1,&MAGOFFY1,&MAGOFFZ1); 
      new_sensors |= SENSORS_MAGNETIC_FIELD;
      data->sensors[ID_MAGNETIC_FIELD].magnetic.x = (event_val.my-MAGOFFY1)*(1.0f/1000.0f)*100.0f;          
      data->sensors[ID_MAGNETIC_FIELD].magnetic.y = -(event_val.mx-MAGOFFX1)*(1.0f/1000.0f)*100.0f;          
      data->sensors[ID_MAGNETIC_FIELD].magnetic.z = -(event_val.mz-MAGOFFZ1)*(1.0f/1000.0f)*100.0f;          
      LOGD_IF(0, "%s cal: %d %d %d\n", __FUNCTION__, MAGOFFX1, MAGOFFY1, MAGOFFZ1);
    }
	}

	if (active_sensors == 0) {
		short MAGOFFX1,MAGOFFY1,MAGOFFZ1;
		short MAGOFFX2,MAGOFFY2,MAGOFFZ2;

		//get old calibration data
		MEMSAlgLib_eCompass_GetCalibration(&MAGOFFX1,&MAGOFFY1,&MAGOFFZ1);

    LOGD_IF(1, "mmx=%d,mmy=%d,mmz=%d,ax=%d,ay=%d,az=%d,time=%d,cx=%d,cy=%d,cz=%d\n\r", 	event_val.mx,event_val.my,event_val.mz,event_val.ax,event_val.ay,event_val.az,event_val.time,MAGOFFX1,MAGOFFY1,MAGOFFZ1);
    
		//Azimuth
		MEMSAlgLib_eCompass_Update(&event_val);

		if(MEMSAlgLib_eCompass_IsCalibrated()) {
			new_sensors |= SENSORS_ORIENTATION;
			data->sensors[ID_ORIENTATION].orientation.azimuth = MEMSAlgLib_eCompass_Get_Azimuth();  
			MEMSAlgLib_eCompass_GetCalibration(&MAGOFFX2,&MAGOFFY2,&MAGOFFZ2);

			if( (MAGOFFX1!=MAGOFFX2) || (MAGOFFY1!=MAGOFFY2) || (MAGOFFZ1!=MAGOFFZ2) ) 
      {
        LOGD_IF(1, "CALIBRATION: %d %d %d %d %d %d\n", MAGOFFX1, MAGOFFY1, MAGOFFZ1,MAGOFFX2, MAGOFFY2, MAGOFFZ2);
				write_sensor_calibration_data(MAGOFFX2, MAGOFFY2, MAGOFFZ2);//platform related API
			}
		}

		data->sensors[ID_ORIENTATION].orientation.pitch = MEMSAlgLib_eCompass_Get_Pitch();  
		data->sensors[ID_ORIENTATION].orientation.roll = -MEMSAlgLib_eCompass_Get_Roll();   

		if(MEMSAlgLib_eCompass_IsCalibrated())
			set_delay(25);  
		else
			set_delay(100);
	}

	if (new_sensors) {
		data->pendingSensors = new_sensors;
		return pick_sensor(data, values);
	}

	return -1;
}

static int
data__close(struct hw_device_t *dev)
{
	SensorData*  data = (void*)dev;
    D("%s: dev=%p", __FUNCTION__, dev);

	if(data) {
		if (acc_event_fd > 0){
			close(acc_event_fd);
			close(mag_event_fd);
			acc_event_fd = -1;
		}

		free(data);
	}
	return 0;
}

/** MODULE REGISTRATION SUPPORT
 **
 ** This is required so that hardware/libhardware/hardware.c
 ** will dlopen() this library appropriately.
 **/

/*
 * the following is the list of all supported sensors.
 * this table is used to build sSensorList declared below
 * according to which hardware sensors are reported as
 * available from the emulator (see get_sensors_list below)
 */
static const struct sensor_t sSensorListInit[] = {
        { .name       = "LSM303DLH 3-axis Accelerometer",
          .vendor     = "STMicroelectronics",
          .version    = 1,
          .handle     = ID_ACCELERATION,
          .type       = SENSOR_TYPE_ACCELEROMETER,
          .maxRange   = GRAVITY_EARTH * 4.0f,           
          .resolution = GRAVITY_EARTH * 4.0f / 4000.0f, 
          .power      = 0.25f,
          .reserved   = {}
        },

        { .name       = "LSM303DLH 3-axis Magnetic sensor",
          .vendor     = "STMicroelectronics",
          .version    = 1,
          .handle     = ID_MAGNETIC_FIELD,
          .type       = SENSOR_TYPE_MAGNETIC_FIELD,
          .maxRange   = 8.1f*2.0f*100.0f,    
          .resolution = (1.0f/1000.0f)*100.0f,   
          .power      = 0.6f,
          .reserved   = {}
        },

        { .name       = "LSM303DLH Orientation sensor",
          .vendor     = "STMicelectronics GC&SA APM travis.tu@st.com",
          .version    = 1,
          .handle     = ID_ORIENTATION,
          .type       = SENSOR_TYPE_ORIENTATION,
          .maxRange   = 360.0f,
          .resolution = 1.0f,
          .power      = 0.001f,
          .reserved   = {}
        },

        { .name       = "LDD6410 Temperature sensor",
          .vendor     = "The Android Open Source Project",
          .version    = 1,
          .handle     = ID_TEMPERATURE,
          .type       = SENSOR_TYPE_TEMPERATURE,
          .maxRange   = 80.0f,
          .resolution = 1.0f,
          .power      = 0.0f,
          .reserved   = {}
        },

};

static struct sensor_t  sSensorList[MAX_NUM_SENSORS];

static int sensors__get_sensors_list(struct sensors_module_t* module,
	struct sensor_t const** list)
{
	int nn;
	int mask = 0x7;
	int count = 0;

	for (nn = 0; nn < MAX_NUM_SENSORS; nn++) {
		if (mask & (1 << nn))
			sSensorList[count++] = sSensorListInit[nn];
	}
	D("%s: returned %d sensors", __FUNCTION__, count);
	*list = sSensorList;
	return count;
}

static int
open_sensors(const struct hw_module_t* module,
             const char*               name,
             struct hw_device_t*      *device)
{
    int  status = -EINVAL;

    D("%s: name=%s", __FUNCTION__, name);

    if (!strcmp(name, SENSORS_HARDWARE_CONTROL))
    {
        SensorControl *dev = malloc(sizeof(*dev));

        memset(dev, 0, sizeof(*dev));

        dev->device.common.tag       = HARDWARE_DEVICE_TAG;
        dev->device.common.version   = 0;
        dev->device.common.module    = (struct hw_module_t*) module;
        dev->device.common.close     = control__close;
        dev->device.open_data_source = control__open_data_source;
        dev->device.activate         = control__activate;
        dev->device.set_delay        = control__set_delay;
		dev->device.wake             = control__wake;

		*device = &dev->device.common;
        status  = 0;
    }
    else if (!strcmp(name, SENSORS_HARDWARE_DATA)) {
        SensorData *dev = malloc(sizeof(*dev));

        memset(dev, 0, sizeof(*dev));

        dev->device.common.tag     = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module  = (struct hw_module_t*) module;
        dev->device.common.close   = data__close;
        dev->device.data_open      = data__data_open;
        dev->device.data_close     = data__data_close;
        dev->device.poll           = data__poll;
		acc_event_fd = -1;
		mag_event_fd = -1;

        *device = &dev->device.common;
        status  = 0;
    }
    return status;
}


static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

const struct sensors_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.version_major = 1,
		.version_minor = 0,
		.id = SENSORS_HARDWARE_MODULE_ID,
		.name = "LSM303DLH SENSORS Module",
		.author = "Travis Tu <travis.tu@st.com>",
		.methods = &sensors_module_methods,
	},
	.get_sensors_list = sensors__get_sensors_list
};
