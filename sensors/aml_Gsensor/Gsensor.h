
#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <poll.h>
#include <utils/Vector.h>

#include "sensors.h"
#include "SensorBase.h"

/*****************************************************************************/

class GSensor : public SensorBase {
public:
            GSensor();
    virtual ~GSensor();

    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
    virtual int readEvents(sensors_event_t *data, int count);
    virtual int getFd() const;
   // virtual int getPollTime();
private:
    int accel_fd;

    uint32_t mEnabled;
    int m_gspos;
    
};


