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

#include <glib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <system_info.h>
#include <stdio.h>
#include "app.h"
#include "http-server-log-private.h"
#include "http-server-route.h"
#include "hs-util-json.h"

#define SIZE 1024

static void route_api_face_detect_callback(SoupMessage *msg,
					const char *path, GHashTable *query,
					SoupClientContext *client, gpointer user_data)
{
	app_data *ad = user_data;
	bool bool_val = false;
	char *response_msg = NULL;
	char temp_str[SIZE] = {0, };
	gsize resp_msg_size = 0;
	JsonBuilder *builder = NULL;

	ret_if(!ad);

	if (msg->method != SOUP_METHOD_GET) {
		soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
		return;
	}

	builder = json_builder_new();
	json_builder_begin_object(builder);

	if (ad->recognize_label == 1)
		snprintf(temp_str, SIZE - 1, "Han Min Su");
	else
		snprintf(temp_str, SIZE - 1, "Guest");
	util_json_add_str(builder, "Label", temp_str);

	snprintf(temp_str, SIZE - 1, "%.2f", ad->recognize_percent);
	util_json_add_str(builder, "Confidence", temp_str);

	snprintf(temp_str, SIZE - 1, "%d", ad->recognize_x);
	util_json_add_str(builder, "X", temp_str);

	snprintf(temp_str, SIZE - 1, "%d", ad->recognize_y);
	util_json_add_str(builder, "Y", temp_str);

	snprintf(temp_str, SIZE - 1, "%d", ad->recognize_width);
	util_json_add_str(builder, "Width", temp_str);

	snprintf(temp_str, SIZE - 1, "%d", ad->recognize_height);
	util_json_add_str(builder, "Height", temp_str);

	json_builder_end_object(builder);

	response_msg = util_json_generate_str(builder, &resp_msg_size);
	g_clear_pointer(&builder, g_object_unref);

	soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY,
					response_msg, resp_msg_size);
	g_clear_pointer(&response_msg, g_free);

	soup_message_headers_set_content_type(
						msg->response_headers, "application/json", NULL);

	soup_message_set_status(msg, SOUP_STATUS_OK);
}

int hs_route_api_face_detect_init(void *data)
{
	return http_server_route_handler_add("/api/faceDetect",
			route_api_face_detect_callback, data, NULL);
}
