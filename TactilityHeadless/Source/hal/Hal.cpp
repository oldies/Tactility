#include "Tactility/hal/Hal_i.h"
#include "Tactility/hal/i2c/I2c.h"

#include <Tactility/kernel/SystemEvents.h>

#define TAG "hal"

namespace tt::hal {

void init(const Configuration& configuration) {
    kernel::systemEventPublish(kernel::SystemEvent::BootInitHalBegin);

    kernel::systemEventPublish(kernel::SystemEvent::BootInitI2cBegin);
    tt_check(i2c::init(configuration.i2c), "I2C init failed");
    if (configuration.initHardware != nullptr) {
        TT_LOG_I(TAG, "Init hardware");
        tt_check(configuration.initHardware(), "Hardware init failed");
    }
    kernel::systemEventPublish(kernel::SystemEvent::BootInitI2cEnd);

    if (configuration.initBoot != nullptr) {
        TT_LOG_I(TAG, "Init power");
        tt_check(configuration.initBoot(), "Init power failed");
    }

    if (configuration.sdcard != nullptr) {
        TT_LOG_I(TAG, "Mounting sdcard");
        if (!configuration.sdcard->mount(TT_SDCARD_MOUNT_POINT)) {
            TT_LOG_W(TAG, "SD card mount failed (init can continue)");
        }
    }

    kernel::systemEventPublish(kernel::SystemEvent::BootInitHalEnd);
}

} // namespace
