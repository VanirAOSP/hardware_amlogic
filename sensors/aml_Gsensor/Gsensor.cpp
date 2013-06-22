/*
* Copyright (C) Bosch Sensortec GmbH 2011
* Copyright (C) 2008 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*	   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <linux/input.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <stdlib.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "amlogic_sensor"
#endif
#include <cutils/log.h>

#include "Gsensor.h"

//#define DEBUG_SENSOR            0

#if defined(ACCELEROMETER_SENSOR_MMA8451)
  #define LSG                         (4096.0f) // 4096 LSG = 1G for MMA8451
#elif defined(ACCELEROMETER_SENSOR_MMA8450)
  #define LSG                         (1024.0f) // 1024 LSG = 1G for MMA8450
#elif defined(ACCELEROMETER_SENSOR_MMA8452)
  #define LSG                         (720.0f) //(1024.0f) // 1024 LSG = 1G for MMA8452
#elif defined(ACCELEROMETER_SENSOR_BMA250)
  #define LSG                         (256.0f)
#endif

#ifndef SENSOR_NAME
#error "Must define SENSOR_NAME in aml_Gsensor HAL !!"
#endif

#define CONVERT                     (GRAVITY_EARTH / LSG)
#define CONVERT_X                   -(CONVERT)
#define CONVERT_Y                   -(CONVERT)
#define CONVERT_Z                   (CONVERT)
#define INPUT_DIR               "/dev/input"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
/*
gspos:
0:x =  x, y =  y, z = z
1:x = -y, y =  x, z = z
2:x = -x, y = -y, z = z
3:x =  y, y = -x, z = z
4:x = -x, y =  y, z = -z
5:x = -y, y = -x, z = -z
6:x =  x, y = -y, z = -z
7:x =  y, y =  x, z = -z
*/
static int acc_aml_get_install_dir(void)
{
	char rotationproperty[PROPERTY_VALUE_MAX],gsposproperty[PROPERTY_VALUE_MAX];
	int rotation,gspos;
	property_get("ro.sf.hwrotation", rotationproperty, "0");
	property_get("ro.sf.gsensorposition", gsposproperty, "0");
	rotation = atoi(rotationproperty);
	gspos = atoi(gsposproperty);
    	switch (rotation)
	{
		case 270:
			return gspos;
		default://0	90	180
			if(gspos <= 3)
			{
				gspos -= rotation/90 +1;
				if(gspos < 0)
					gspos+=4;
			} 
			else if(gspos >=4)
			{
				gspos -= rotation/90 +1;
				if(gspos < 4)
					gspos+=4;
			}
			return gspos;
   	}
}

static int set_sysfs_input_attr(char *class_path,
				const char *attr, char *value, int len)
{
	char path[256];
	int fd;

	if (class_path == NULL || *class_path == '\0'
	    || attr == NULL || value == NULL || len < 1) {
		return -EINVAL;
	}
	snprintf(path, sizeof(path), "%s/%s", class_path, attr);
	path[sizeof(path) - 1] = '\0';
	fd = open(path, O_RDWR);
	if (fd < 0) {
		return -errno;
	}
	if (write(fd, value, len) < 0) {
		close(fd);
		return -errno;
	}
	close(fd);

	return 0;
}


static int sensor_get_class_path(char *des)
{
    char *dirname = "/sys/class/input";
    char buf[256];
    char class_path[PATH_MAX];
    
    int res;
    DIR *dir;
    struct dirent *de;
    int fd = -1;
    int found = 0;

    dir = opendir(dirname);
    if (dir == NULL)
    	return -1;

	while((de = readdir(dir))) {
		if (strncmp(de->d_name, "input", strlen("input")) != 0) {
		    continue;
        	}

		sprintf(class_path, "%s/%s", dirname, de->d_name);
		snprintf(buf, sizeof(buf), "%s/name", class_path);

		fd = open(buf, O_RDONLY);
		if (fd < 0) {
		    continue;
		}
		if ((res = read(fd, buf, sizeof(buf))) < 0) {
		    close(fd);
		    continue;
		}
		buf[res - 1] = '\0';
		if (strcmp(buf, SENSOR_NAME) == 0) {
		    found = 1;
		    close(fd);
		    break;
		}

		close(fd);
		fd = -1;
	}
	closedir(dir);

    if (found) {
        snprintf(des, PATH_MAX, "%s", class_path);
        return 0;
    }else {
        return -1;
    }

}

GSensor::GSensor() :
    SensorBase(NULL, SENSOR_NAME),
    mEnabled(0)
{
    m_gspos = acc_aml_get_install_dir();
    if(0> sensor_get_class_path(class_path) ){
        LOGE("failed to sensor_get_class_path!!");
        return ;
    }

    LOGD("dgt GSensor:m_gspos=%d,class_path is %s \n", m_gspos, class_path);
}

GSensor:: ~GSensor()
{
    
}

int GSensor::getFd() const
{
    LOGV("GSensor::getFd returning %d", data_fd);
    return data_fd;
}


 int GSensor:: enable(int handle, int enabled) {
	char buffer[20];

	int bytes = sprintf(buffer, "%d\n", enabled);

	return set_sysfs_input_attr(class_path,"enable",buffer,bytes);
}

int GSensor:: setDelay(int handle, int64_t ns) {
	char buffer[20];
	int ms=ns/1000000;
	int bytes = sprintf(buffer, "%d\n", ms);

	return set_sysfs_input_attr(class_path,"delay",buffer,bytes);
}

int GSensor:: readEvents(sensors_event_t* data, int count) {
	
    struct input_event event;
    int ret;
    int gspos = m_gspos ;


    if (data_fd < 0)
        return 0;
	
    while (1) {
        ret = read(data_fd, &event, sizeof(event));

        if (event.type == EV_ABS) {

            switch (event.code) {
                case ABS_X:
                    if(gspos == 0||gspos == 6){
                        data->acceleration.x = event.value * CONVERT;	
			} else if(gspos == 1||gspos == 7){
                         data->acceleration.y = event.value * CONVERT;	
                    } else if(gspos == 2||gspos == 4){
                        data->acceleration.x = -event.value * CONVERT;	
                    } else if(gspos == 3||gspos == 5){
                        data->acceleration.y = -event.value * CONVERT;	
                    }			    
			break;
                case ABS_Y:
                if(gspos == 0||gspos == 4){
                    data->acceleration.y = event.value * CONVERT;	
                } else if(gspos == 1||gspos == 5){
                    data->acceleration.x = -event.value * CONVERT;	
                } else if(gspos == 2||gspos == 6){
                    data->acceleration.y = -event.value * CONVERT;	
                } else if(gspos == 3||gspos == 7){
                    data->acceleration.x = event.value * CONVERT;	
                }
                    break;
                case ABS_Z:
                    if(gspos < 4){
                        data->acceleration.z = event.value * CONVERT;
                    }else{
                        data->acceleration.z = -event.value * CONVERT;			        
                    }
                    break;
            }
        } else if (event.type == EV_SYN) {

            data->timestamp = (int64_t)((int64_t)event.time.tv_sec*1000000000
            		                     + (int64_t)event.time.tv_usec*1000);
            data->sensor = ID_A;
            data->type = SENSOR_TYPE_ACCELEROMETER;
            data->acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
			
		
#ifdef DEBUG_SENSOR
    	    LOGD("Sensor data: t x,y,x: %f %f, %f, %f\n",
    			data->timestamp / 1000000000.0,
    					data->acceleration.x,
    					data->acceleration.y,
    					data->acceleration.z);
#endif
			
            return ret;	
        }
    }
	
    return 0;
}


