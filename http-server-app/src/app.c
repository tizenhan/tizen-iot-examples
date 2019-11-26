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

#include <service_app.h>
#include <mv_common.h>
#include "http-server-log-private.h"
#include "http-server-common.h"
#include "hs-route-root.h"
#include "hs-route-api-connection.h"
#include "hs-route-api-applist.h"
#include "hs-route-api-sysinfo.h"
#include "hs-route-api-storage.h"
#include "hs-route-api-image-upload.h"
#include "hs-route-api-face-detect.h"
#include "app.h"
#include "face-recognize.h"
#include "usb-camera.h"
#include "thingspark_api.h"
#include "resource_relay.h"
#include "resource_relay_internal.h"

#define SERVER_NAME "http-server-app"
#define SERVER_PORT 8080

static int route_modules_init(void *data)
{
	int ret = 0;

	ret = hs_route_root_init();
	retv_if(ret, -1);

	ret = hs_route_api_connection_init();
	retv_if(ret, -1);

	ret = hs_route_api_applist_init();
	retv_if(ret, -1);

	ret = hs_route_api_sysinfo_init();
	retv_if(ret, -1);

	ret = hs_route_api_storage_init();
	retv_if(ret, -1);

	ret = hs_route_api_image_upload_init();
	retv_if(ret, -1);

	ret = hs_route_api_face_detect_init(data);
	retv_if(ret, -1);

	return 0;
}

static void server_destroy(void)
{
	http_server_destroy();
	_D("server is destroyed");
}

static int server_init_n_start(void *data)
{
	int ret = 0;

	ret = http_server_create(SERVER_NAME, SERVER_PORT);
	retv_if(ret, -1);

	ret = route_modules_init(data);
	retv_if(ret, -1);

	ret = http_server_start();
	retv_if(ret, -1);

	_D("server is started");
	return 0;
}

static void conn_type_changed_cb(connection_type_e type, void *data)
{
	app_data *ad = data;

	_D("connection type is changed [%d] -> [%d]", ad->cur_conn_type, type);

	server_destroy();

	if (type != CONNECTION_TYPE_DISCONNECTED) {
		int ret = 0;
		_D("restart server");
		ret = server_init_n_start(data);
		if (ret) {
			_E("failed to start server");
			service_app_exit();
		}
	}

	ad->cur_conn_type = type;

	return;
}

Eina_Bool _tp_timer_cb(void *data)
{
	app_data *ad = data;
	int ret = 0;

	ret = tp_initialize("czRXVbgv72ILyJUl", &ad->handle);
	retv_if(ret != 0, ECORE_CALLBACK_RENEW);

	ret = tp_set_field_value(ad->handle, 1, "0");
	retv_if(ret != 0, ECORE_CALLBACK_RENEW);

	ret = tp_send_data(ad->handle);
	retv_if(ret != 0, ECORE_CALLBACK_RENEW);

	tp_finalize(ad->handle);

	return ECORE_CALLBACK_RENEW;
}

static void service_app_control(app_control_h app_control, void *data)
{
	int ret = 0;
	app_data *ad = data;
	tp_handle_h handle = NULL;

	face_recognize();

	ret = usb_camera_prepare(data);
	ret_if(ret < 0);

	ret = usb_camera_preview(data);
	ret_if(ret < 0);

	ad->tp_timer = ecore_timer_add(20.0f, _tp_timer_cb, ad);
	ret_if(!ad->tp_timer);
}

static bool service_app_create(void *data)
{
	app_data *ad = data;
	int ret = 0;

	retv_if(!ad, false);

	if (ad->tp_timer) {
		ecore_timer_del(ad->tp_timer);
		ad->tp_timer = NULL;
	}

	ret = connection_create(&ad->conn_h);
	retv_if(ret, false);

	ret = connection_set_type_changed_cb(ad->conn_h, conn_type_changed_cb, ad);
	goto_if(ret, ERROR);

	connection_get_type(ad->conn_h, &ad->cur_conn_type);
	if (ad->cur_conn_type == CONNECTION_TYPE_DISCONNECTED) {
		_D("network is not connected, waiting to be connected to any type of network");
		return true;
	}

	ret = server_init_n_start(data);
	goto_if(ret, ERROR);

	return true;

ERROR:
	if (ad->conn_h)
		connection_destroy(ad->conn_h);

	server_destroy();
	return false;
}

static void service_app_terminate(void *data)
{
	app_data *ad = data;

	resource_close_relay(19);
	resource_close_relay(26);

	usb_camera_unprepare(data);
	server_destroy();

	if (ad->conn_h) {
		connection_destroy(ad->conn_h);
		ad->conn_h = NULL;
	}
	ad->cur_conn_type = CONNECTION_TYPE_DISCONNECTED;

	return;
}

int main(int argc, char* argv[])
{
	app_data ad = {0, };
	service_app_lifecycle_callback_s event_callback;

	ad.conn_h = NULL;
	ad.cur_conn_type = CONNECTION_TYPE_DISCONNECTED;

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	service_app_main(argc, argv, &event_callback, &ad);

	return 0;
}
