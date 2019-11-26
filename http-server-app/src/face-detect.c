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
#include <stdlib.h>
#include <time.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <mv_common.h>
/* To use the functions and data types of the Media Vision Face API (in mobile and wearable applications), include the  <mv_face.h> header file in your application. */
#include <mv_face.h>
/* In addition, you must include the <image_util.h> header file to handle the image decoding tasks, or the  <camera.h> header file to provide preview images. */
/* Image decoding for face detection and recognition */
#include <image_util.h>
/* Preview images for face tracking */
#include <camera.h>

#include "http-server-log-private.h"
#include "http-server-route.h"
#include "hs-util-json.h"
#include "face-detect.h"
#include "face-recognize.h"
#include "image-cropper.h"
#include "resource_relay.h"

/* Face Detect Model from Tizen */
#define FACE_DETECT_MODEL_FILEPATH "/usr/share/OpenCV/haarcascades/haarcascade_frontalface_alt.xml"

/* For face detection, use the following facedata_s structure: */
struct _facedata_s {
    mv_source_h g_source;
    mv_engine_config_h g_engine_config;
    int is_working;

	char *type;
	char *result;
	unsigned char *image_data;
	void *user_data;
	int size;
};
typedef struct _facedata_s facedata_s;
static facedata_s facedata;

static bool _crop_i420(mv_source_h image, unsigned int result_x, unsigned int result_y,
		unsigned int result_width, unsigned int result_height,
		unsigned char **result_buff, mv_colorspace_e *result_colorspace, int *res_size)
{
	unsigned char *image_buff = NULL;
	unsigned int image_buff_size = 0;

	unsigned int image_width = 0;
	unsigned int image_height = 0;

	int err = mv_source_get_buffer(image, &image_buff, &image_buff_size);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_width(image, &image_width);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_height(image, &image_height);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	*res_size = result_width * result_height * 1.5;
	*result_buff = (unsigned char *)calloc(*res_size, sizeof(unsigned char));
	_D("res_size : %u", *res_size);

	int x = 0;
	int y = 0;

	/* Fill the Y channel. */
	for (y = 0; y < result_height; ++y) {

		for (x = 0; x < result_width; ++x) {

			(*result_buff)[y * result_width + x] =
					image_buff[(y + result_y) * image_width +
												(x + result_x)];
		}
	}

	(*result_colorspace) = MEDIA_VISION_COLORSPACE_I420;

	/* Fill the U and V channels. */
	int u1_result_x = result_x / 2.0;
	int u1_result_y = image_height + result_y / 4.0;
	int u2_result_x = result_x / 2.0;
	int u2_result_y = image_height * 1.25 + result_y / 4.0;

	int v1_result_x = result_x / 2.0 + image_width / 2.0;
	int v1_result_y = image_height + result_y / 4.0;
	int v2_result_x = result_x / 2.0 + image_width / 2.0;
	int v2_result_y = image_height * 1.25 + result_y / 4.0;

	int uv_width = result_width / 2;
	int uv_height = result_height / 4;

	int src_x;
	int src_y;
	int dst_x;
	int dst_y;

	for (y = 0; y < uv_height; ++y) {

		for (x = 0; x < uv_width; ++x) {

			src_x = x + u1_result_x;
			src_y = y + u1_result_y;
			dst_x = x;
			dst_y = y + result_height;
			(*result_buff)[dst_y * result_width + dst_x] =
					image_buff[src_y * image_width + src_x];


			src_x = x + u2_result_x;
			src_y = y + u2_result_y;
			dst_x = x;
			dst_y = y + result_height + uv_height;
			(*result_buff)[dst_y * result_width + dst_x] =
					image_buff[src_y * image_width + src_x];


			src_x = x + v1_result_x;
			src_y = y + v1_result_y;
			dst_x = x + uv_width;
			dst_y = y + result_height;
			(*result_buff)[dst_y * result_width + dst_x] =
					image_buff[src_y * image_width + src_x];


			src_x = x + v2_result_x;
			src_y = y + v2_result_y;
			dst_x = x + uv_width;
			dst_y = y + result_height + uv_height;
			(*result_buff)[dst_y * result_width + dst_x] =
					image_buff[src_y * image_width + src_x];
		}
	}

	return true;
}

static bool _crop_nv12(mv_source_h image, unsigned int result_x, unsigned int result_y,
		unsigned int result_width, unsigned int result_height,
		unsigned char **result_buff, mv_colorspace_e *result_colorspace, int *res_size)
{
	unsigned char *image_buff = NULL;
	unsigned int image_buff_size = 0;

	unsigned int image_width = 0;
	unsigned int image_height = 0;

	int err = mv_source_get_buffer(image, &image_buff, &image_buff_size);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_width(image, &image_width);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_height(image, &image_height);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	*res_size = result_width * result_height * 1.5;
	*result_buff = (unsigned char *)calloc(*res_size, sizeof(unsigned char));

	int x = 0;
	int y = 0;

	/* Fill the Y channel. */
	for (y = 0; y < result_height; ++y) {

		for (x = 0; x < result_width; ++x) {

			(*result_buff)[y * result_width + x] =
					image_buff[(y + result_y) * image_width +
												(x + result_x)];
		}
	}

	/* Fill the U and V channels. */
	int i = result_width * result_height;
	for (; i < *res_size; ++i)
		(*result_buff)[i] = 127;

	(*result_colorspace) = MEDIA_VISION_COLORSPACE_NV12;

	return true;
}

static bool _crop_y800(mv_source_h image, unsigned int result_x, unsigned int result_y,
		unsigned int result_width, unsigned int result_height,
		unsigned char **result_buff, mv_colorspace_e *result_colorspace, int *res_size)
{
	unsigned char *image_buff = NULL;
	unsigned int image_buff_size = 0;

	unsigned int image_width = 0;
	unsigned int image_height = 0;

	int err = mv_source_get_buffer(image, &image_buff, &image_buff_size);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_width(image, &image_width);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	err = mv_source_get_height(image, &image_height);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	*res_size = result_width * result_height * 1.5;
	*result_buff = (unsigned char *)calloc(*res_size, sizeof(unsigned char));

	int x = 0;
	int y = 0;

	/* Fill the Y channel. */
	for (y = 0; y < result_height; ++y) {

		for (x = 0; x < result_width; ++x) {

			(*result_buff)[y * result_width + x] =
					image_buff[(y + result_y) * image_width +
												(x + result_x)];
		}
	}

	(*result_colorspace) = MEDIA_VISION_COLORSPACE_I420;

	int uv_width = result_width / 2;
	int uv_height = result_height / 4;

	int dst_x;
	int dst_y;

	for (y = 0; y < uv_height; ++y) {

		for (x = 0; x < uv_width; ++x) {

			dst_x = x;
			dst_y = y + result_height;
			(*result_buff)[dst_y * result_width + dst_x] = 127;

			dst_x = x;
			dst_y = y + result_height + uv_height;
			(*result_buff)[dst_y * result_width + dst_x] = 127;

			dst_x = x + uv_width;
			dst_y = y + result_height;
			(*result_buff)[dst_y * result_width + dst_x] = 127;

			dst_x = x + uv_width;
			dst_y = y + result_height + uv_height;
			(*result_buff)[dst_y * result_width + dst_x] = 127;
		}
	}

	return true;
}

static bool _crop_result(mv_source_h image, unsigned int result_x, unsigned int result_y,
		unsigned int result_width, unsigned int result_height,
		unsigned char **result_buff, mv_colorspace_e *result_colorspace, int *res_size)
{
	if (image == NULL || result_buff == NULL || result_colorspace == NULL ||
			result_width == 0 || result_height == 0 || res_size == NULL)
		return false;

	mv_colorspace_e colorspace;
	int err = mv_source_get_colorspace(image, &colorspace);
	if (err != MEDIA_VISION_ERROR_NONE)
		return false;

	if (colorspace == MEDIA_VISION_COLORSPACE_I420) {
		return _crop_i420(image, result_x, result_y, result_width, result_height, result_buff, result_colorspace, res_size);
	} else if (colorspace == MEDIA_VISION_COLORSPACE_NV12) {
		return _crop_nv12(image, result_x, result_y, result_width, result_height, result_buff, result_colorspace, res_size);
	} else if (colorspace == MEDIA_VISION_COLORSPACE_Y800){
		return _crop_y800(image, result_x, result_y, result_width, result_height, result_buff, result_colorspace, res_size);
	} else	{
		_E("Need to additional implemenation!!!! (csp: %d)", colorspace);
	}
	/* Extend _crop_result() function for another formats if it necessary. */

	return false;
}

/* The mv_face_detect() function invokes the _on_face_detected_cb() callback. */
static void _on_face_detected_cb(mv_source_h source, mv_engine_config_h engine_cfg,
	mv_rectangle_s *locations, int number_of_faces, void *user_data)
{
	unsigned char *data_buffer = NULL;
	unsigned int buffer_size = 0;
	unsigned int image_width = 0;
	unsigned int image_height = 0;
	mv_colorspace_e image_colorspace = 0;

	int error_code = 0;

	if (number_of_faces == 0) {
		error_code = resource_write_relay(19, 0);
        if (error_code < 0) _E("cannot control the relay");
		return;
	}
	_D("\nNumber of Faces : %d\n", number_of_faces);

	error_code = mv_source_get_buffer(source, &data_buffer, &buffer_size);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_width(source, &image_width);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_height(source, &image_height);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_colorspace(source, &image_colorspace);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	for (int i = 0; i < number_of_faces; ++i) {
		unsigned char *result_buff = NULL;
		int result_size = 0;
		int temp_width = 0;
		int temp_height = 0;
		mv_colorspace_e result_colorspace = MEDIA_VISION_COLORSPACE_INVALID;
		mv_source_h face_part = NULL;

		_D("Face[%d] : [%d,%d] [%d:%d]", i, locations[i].point.x, locations[i].point.y, locations[i].width, locations[i].height);

		/* Result of this function will be used to call the image_util_encode_jpeg() function.
		 * Unfortunately, that function may fail when the width or height aren't multiples of 16.  */
		temp_width = ((locations[i].width + 8) / 16) * 16;
		temp_height = ((locations[i].height + 8) / 16) * 16;

		if ((locations[i].point.x + temp_width) >= image_width || (locations[i].point.y + temp_height) >= image_height) {
			temp_width -= 16;
			temp_height -= 16;
		}

		goto_if(!_crop_result(source,
				locations[i].point.x,
				locations[i].point.y,
				temp_width,
				temp_height,
				&result_buff,
				&result_colorspace,
				&result_size),
				ERROR);

		error_code = mv_create_source(&face_part);
		if (error_code != MEDIA_VISION_ERROR_NONE) {
			free(result_buff);
			continue;
		}

		error_code = mv_source_fill_by_buffer(face_part,
				result_buff,
				result_size,
				temp_width,
				temp_height,
				result_colorspace);
		if (error_code != MEDIA_VISION_ERROR_NONE) {
			mv_destroy_source(face_part);
			free(result_buff);
			continue;
		}
		free(result_buff);

		error_code = face_recognize_with_source(face_part, user_data);
		if (error_code !=0) _E("cannot recognize faces in the source");

		mv_destroy_source(face_part);
	}

	return;

ERROR:
	return;
}

static void _unset_engine_config(void)
{
	if (!facedata.g_engine_config) return;
	mv_destroy_engine_config(facedata.g_engine_config);
	facedata.g_engine_config = NULL;
}

static int _set_engine_config(void)
{
	int error_code = 0;

	if (facedata.g_engine_config) return 0;

	/* Create the media vision engine using the mv_create_engine_config() function.
	 * The function creates the g_engine_config engine configuration handle and configures it with default attributes. */
	error_code = mv_create_engine_config(&facedata.g_engine_config);
	retv_if(error_code != MEDIA_VISION_ERROR_NONE, -1);

	/* Face detection details can be configured by setting attributes to the engine configuration handle.
	 * In this use case, the MV_FACE_DETECTION_MODEL_FILE_PATH attribute is configured. */
	error_code = mv_engine_config_set_string_attribute(facedata.g_engine_config,
		MV_FACE_DETECTION_MODEL_FILE_PATH,
		FACE_DETECT_MODEL_FILEPATH);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	return 0;

ERROR:
	_unset_engine_config();
	return -1;
}

static gboolean _after_detect_cb(void *user_data)
{
	//_D("After mv_face_detect()");
	facedata.is_working = 0;
	return FALSE;
}

static gpointer _create_thread_with_source(void *data)
{
	int error_code = 0;

	/* When the source and engine configuration handles are ready, use the mv_face_detect() function to detect faces: */
	error_code = mv_face_detect(facedata.g_source, facedata.g_engine_config, _on_face_detected_cb, data);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
		_after_detect_cb, NULL, NULL);

	return NULL;

ERROR:
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
		_after_detect_cb, NULL, NULL);

	return NULL;
}

int face_detect_with_source(mv_source_h source, void *data)
{
	GThread *th = NULL;
	unsigned char *data_buffer = NULL;
	unsigned int buffer_size = 0;
	unsigned int image_width = 0;
	unsigned int image_height = 0;
	mv_colorspace_e image_colorspace = 0;

	int error_code = 0;

	retv_if(!source, -1);
	if (facedata.is_working) return 0;
	facedata.is_working = 1;

	if (facedata.g_source) {
		//_D("Clear the clonned source");
		mv_source_clear(facedata.g_source);
	} else {
		//_D("Create a clonned source");
		/* Create a source handle using the mv_create_source() function with the mv_source_h member of the detection data structure as the out parameter: */
		/* The source stores the face to be detected and all related data. You manage the source through the source handle. */
		error_code = mv_create_source(&facedata.g_source);
		goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);
	}

	error_code = mv_source_get_buffer(source, &data_buffer, &buffer_size);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_width(source, &image_width);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_height(source, &image_height);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_get_colorspace(source, &image_colorspace);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	/* Fill the dataBuffer to g_source */
	error_code = mv_source_fill_by_buffer(facedata.g_source, data_buffer, buffer_size,
			image_width, image_height, image_colorspace);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	_D("Fill the image[%u x %u = %u]", image_width, image_height, buffer_size);

	error_code = _set_engine_config();
	goto_if(error_code, ERROR);

	th = g_thread_try_new(NULL, _create_thread_with_source, data, NULL);
	retvm_if(!th, -1, "failed to create a thread");

	return 0;

ERROR:
	facedata.is_working = 0;
	return -1;
}

void face_undetect(void)
{
	/* After the face detection is complete, destroy the source and engine configuration handles using the  mv_destroy_source() and mv_destroy_engine_config() functions: */
	mv_destroy_source(facedata.g_source);
	facedata.g_source = NULL;
	_unset_engine_config();
}
