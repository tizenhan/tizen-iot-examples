/*
 * Copyright (c) 2019 SINO TECH Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <peripheral_io.h>

#include "http-server-log-private.h"

struct _resource_s {
	int opened;
	peripheral_gpio_h sensor_h;
};
typedef struct _resource_s resource_s;
static resource_s resource = {0, };

void resource_close_relay(int pin_num)
{
	if (!resource.opened) return;

	_I("Relay is finishing...");
	peripheral_gpio_close(resource.sensor_h);
	resource.opened = 0;
}

int resource_write_relay(int pin_num, int write_value)
{
	int ret = PERIPHERAL_ERROR_NONE;

	if (!resource.opened) {
		ret = peripheral_gpio_open(pin_num, &resource.sensor_h);
		retv_if(!resource.sensor_h, -1);

		ret = peripheral_gpio_set_direction(resource.sensor_h, PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
		retv_if(ret != 0, -1);

		resource.opened = 1;
	}

	ret = peripheral_gpio_write(resource.sensor_h, write_value);
	retv_if(ret < 0, -1);

#ifdef DEBUG
	_I("Relay Value : %s", write_value ? "ON":"OFF");
#endif

	return 0;
}
