
/*
 * Copyright (C) 2011 Invensense, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "Sensors"
#define FUNC_LOG LOGV("%s", __PRETTY_FUNCTION__)

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>

#include <linux/input.h>

#include <utils/Atomic.h>
#include <utils/Log.h>

#include "sensors.h"

#include "Gsensor.h"
#include "LightSensor.h"

/*****************************************************************************/

#define DELAY_OUT_TIME 0x7FFFFFFF

#define LIGHT_SENSOR_POLLTIME    2000000000

#define SENSORS_ROTATION_VECTOR  (1<<ID_RV)
#define SENSORS_LINEAR_ACCEL     (1<<ID_LA)
#define SENSORS_GRAVITY          (1<<ID_GR)
#define SENSORS_GYROSCOPE        (1<<ID_GY)
#define SENSORS_ACCELERATION     (1<<ID_A)
#define SENSORS_MAGNETIC_FIELD   (1<<ID_M)
#define SENSORS_ORIENTATION      (1<<ID_O)

#define SENSORS_ACCELERATION_HANDLE     (ID_A)
#define SENSORS_MAGNETIC_FIELD_HANDLE   (ID_M)
#define SENSORS_LIGHT_HANDLE            (ID_L)

/*****************************************************************************/

/* The SENSORS Module */
static const struct sensor_t sSensorList[] = {
        { 	"BMA250 3-axis Accelerometer",
                "Bosch",
                1, SENSORS_ACCELERATION_HANDLE,
                SENSOR_TYPE_ACCELEROMETER, 
		4.0f*9.81f, 
		(4.0f*9.81f)/1024.0f, 
		0.2f, 
		0, 
		{ } 
	},
#ifdef ENABLE_COMPASS
      { "MPL magnetic field",
        "Invensense",
        1, SENSORS_MAGNETIC_FIELD_HANDLE,
        SENSOR_TYPE_MAGNETIC_FIELD, 10240.0f, 1.0f, 0.5f, 20000,{ } },
#endif
#ifdef ENABLE_LIGHT_SENSOR
      { "Light sensor",
          "(none)",
          1, SENSORS_LIGHT_HANDLE,
          SENSOR_TYPE_LIGHT, 5000.0f, 1.0f, 1.0f, 20000,{ } },
#endif


};


static int open_sensors(const struct hw_module_t* module, const char* id,
                        struct hw_device_t** device);


static int sensors__get_sensors_list(struct sensors_module_t* module,
                                     struct sensor_t const** list) 
{
    *list = sSensorList;
    return ARRAY_SIZE(sSensorList);
}

static struct hw_module_methods_t sensors_module_methods = {
        open: open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
        common: {
                tag: HARDWARE_MODULE_TAG,
                version_major: 1,
                version_minor: 0,
                id: SENSORS_HARDWARE_MODULE_ID,
                name: "Sensors module",
                author: "Amlogic",
                methods: &sensors_module_methods,
        },
        get_sensors_list: sensors__get_sensors_list,
};

struct sensors_poll_context_t {
    struct sensors_poll_device_t device; // must be first

        sensors_poll_context_t();
        ~sensors_poll_context_t();
    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int pollEvents(sensors_event_t* data, int count);

private:
    enum {
        aml_accel,
#ifdef ENABLE_COMPASS
        aml_compass,
#endif
#ifdef ENABLE_LIGHT_SENSOR
        aml_light,
#endif
        numSensorDrivers,   
    };

    struct pollfd mPollFds[numSensorDrivers];
    SensorBase* mSensors[numSensorDrivers];

    int handleToDriver(int handle) const {
        switch (handle) {
            case ID_A:
                return aml_accel;
#ifdef ENABLE_COMPASS
            case ID_M:
                return aml_compass;
#endif
            
#ifdef ENABLE_LIGHT_SENSOR
            case ID_L:
                return aml_light;
#endif
        }
        return -EINVAL;
    }
};

/*****************************************************************************/

sensors_poll_context_t::sensors_poll_context_t()
{
    FUNC_LOG;

    mSensors[aml_accel] = new GSensor();
    mPollFds[aml_accel].fd = mSensors[aml_accel]->getFd();
    mPollFds[aml_accel].events = POLLIN;
    mPollFds[aml_accel].revents = 0;


#ifdef ENABLE_LIGHT_SENSOR
    mSensors[aml_light] = new LightSensor();
    mPollFds[aml_light].fd = mSensors[aml_light]->getFd();
    mPollFds[aml_light].events = POLLIN;
    mPollFds[aml_light].revents = 0;
#endif
    
}

sensors_poll_context_t::~sensors_poll_context_t() 
{
    FUNC_LOG;
    for (int i=0 ; i<numSensorDrivers ; i++) {
        if(mSensors[i])
            delete mSensors[i];
    }
}

int sensors_poll_context_t::activate(int handle, int enabled) 
{
    FUNC_LOG;
    int index = handleToDriver(handle);
    if (index < 0) return index;
    int err =  mSensors[index]->enable(handle, enabled);
    if (err) {
        LOGE("handle[%d] ,sensors_poll_context_t failed to activate (%s)", handle,strerror(errno));
    }
    return err;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) 
{
    FUNC_LOG;
    int index = handleToDriver(handle);
    if (index < 0) return index;
    return mSensors[index]->setDelay(handle, ns);
}

int sensors_poll_context_t::pollEvents(sensors_event_t* data, int count)
{
    //FUNC_LOG;
    int nbEvents = 0;
    int n = 0;
    int polltime = -1;

    do {
        // see if we have some leftover from the last poll()
        for (int i=0 ; count && i<numSensorDrivers ; i++) {
            SensorBase* const sensor(mSensors[i]);
            if (mPollFds[i].revents & POLLIN) {
                
                int nb = sensor->readEvents(data, count);
                if (nb >0) {
                    // no more data for this sensor
                    mPollFds[i].revents = 0;
                    count = 0;
                    nbEvents += 1 ;
                }
            }
        }

        if (count) {
            // we still have some room, so try to see if we can get
            // some events immediately or just wait if we don't have
            // anything to return

            n = poll(mPollFds, numSensorDrivers, nbEvents ? 0 : polltime);
            if (n<0) {
                LOGE("poll() failed (%s)", strerror(errno));
                return -errno;
            }
        }
        // if we have events and space, go read them
    } while (n && count);

    return nbEvents;
}

/*****************************************************************************/

static int poll__close(struct hw_device_t *dev)
{
    FUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    if (ctx) {
        delete ctx;
    }
    return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev,
                          int handle, int enabled)
{
    FUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev,
                          int handle, int64_t ns)
{
    FUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev,
                      sensors_event_t* data, int count)
{
    //FUNC_LOG;
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->pollEvents(data, count);
}

/*****************************************************************************/

/** Open a new instance of a sensor device using name */
static int open_sensors(const struct hw_module_t* module, const char* id,
                        struct hw_device_t** device)
{
    FUNC_LOG;
    int status = -EINVAL;
    sensors_poll_context_t *dev = new sensors_poll_context_t();

    memset(&dev->device, 0, sizeof(sensors_poll_device_t));

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version  = 0;
    dev->device.common.module   = const_cast<hw_module_t*>(module);
    dev->device.common.close    = poll__close;
    dev->device.activate        = poll__activate;
    dev->device.setDelay        = poll__setDelay;
    dev->device.poll            = poll__poll;

    *device = &dev->device.common;
    status = 0;

    return status;
}


