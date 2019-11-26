 /*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
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

#ifndef __APP_H__
#define __APP_H__

#include <mv_common.h>
#include <net_connection.h>
#include <Ecore.h>

#include "thingspark_api.h"

struct app_data_s {
	connection_h conn_h;
	connection_type_e cur_conn_type;

	/* Private */
	mv_source_h *source;
	int recognize_label;
	double recognize_percent;
	int recognize_x;
	int recognize_y;
	int recognize_width;
	int recognize_height;

	tp_handle_h handle;
	Ecore_Timer *tp_timer;
};
typedef struct app_data_s app_data;

#endif /* __APP_H__ */
