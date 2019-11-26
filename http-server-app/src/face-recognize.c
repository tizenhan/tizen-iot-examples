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
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <app_common.h>
#include <mv_common.h>
/* To use the functions and data types of the Media Vision Face API (in mobile and wearable applications), include the  <mv_face.h> header file in your application. */
#include <mv_face.h>
/* In addition, you must include the <image_util.h> header file to handle the image decoding tasks, or the  <camera.h> header file to provide preview images. */
/* Image decoding for face detection and recognition */
#include <image_util.h>

#include "app.h"
#include "http-server-log-private.h"
#include "thingspark_api.h"
#include "resource_relay.h"

#define FILEPATH_SIZE 1024
#define FACE_MODEL_FILE_NAME "face_model.dat"
#define MINIMUM_RECOGNIZE 0.95f

/* For face recognition, use the following facedata_s structure: */
struct _facedata_s {
    mv_source_h g_source;
    mv_engine_config_h g_engine_config;
    mv_face_recognition_model_h g_face_recog_model;

	int recognize_label;
	double recognize_percent;
	int recognize_x;
	int recognize_y;
	int recognize_width;
	int recognize_height;
};
typedef struct _facedata_s facedata_s;
static facedata_s facedata;

/* Add face examples to the face recognition model handle.
 * Make sure that the face examples are of the same person but captured at different angles.
 * The following example assumes that 10 face samples
 * (face_sample_0.jpg - face_sample_9.jpg in the <OwnDataPath> folder)
 * are used and that the face area in each example covers approximately 95~100% of the image.
 * The label of the face is set to ‘1’. */
static int _create_model(void)
{
	int example_index = 0;
	int face_label = 1;

	char filePath[FILEPATH_SIZE] = {0, };
	char *app_res_dir = NULL;
	char *app_data_dir = NULL;

	unsigned char *dataBuffer = NULL;
	unsigned long long bufferSize = 0;
	unsigned long width = 0;
	unsigned long height = 0;
	mv_rectangle_s roi = {0, };

	image_util_decode_h imageDecoder = NULL;
	int error_code = 0;

	app_res_dir = app_get_resource_path();
	retv_if(!app_res_dir, -1);

	/* Create a g_face_recog_model media vision face recognition model handle using the  mv_face_recognition_model_create() function.
	 * The handle must be created before any recognition is attempted. */
	error_code = mv_face_recognition_model_create(&facedata.g_face_recog_model);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = image_util_decode_create(&imageDecoder);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	for (example_index = 0; example_index < 10; ++example_index) {
	   /* Decode image and fill the image data to g_source handle */
	   snprintf(filePath, FILEPATH_SIZE, "%simages/face_sample_%d.png", app_res_dir, example_index);
	   _D("Adding an image[%s]", filePath);
	   if (access(filePath, F_OK)) {
		   _E("Not found[%s]", filePath);
		   continue;
	   }

	   error_code = image_util_decode_set_input_path(imageDecoder, filePath);
	   continue_if(error_code != IMAGE_UTIL_ERROR_NONE);

	   error_code = image_util_decode_set_output_buffer(imageDecoder, &dataBuffer);
	   continue_if(error_code != IMAGE_UTIL_ERROR_NONE);

	   /* FIXME : colorspace has to be set after input_path */
	   error_code = image_util_decode_set_colorspace(imageDecoder, IMAGE_UTIL_COLORSPACE_RGBA8888);
	   continue_if(error_code != IMAGE_UTIL_ERROR_NONE);

	   error_code = image_util_decode_run(imageDecoder, &width, &height, &bufferSize);
	   continue_if(error_code != IMAGE_UTIL_ERROR_NONE);

	   roi.point.x = roi.point.y = 0;
	   roi.width = width;
	   roi.height = height;

	   error_code = mv_source_clear(facedata.g_source);
	   continue_if(error_code != MEDIA_VISION_ERROR_NONE);

	   error_code = mv_source_fill_by_buffer(facedata.g_source, dataBuffer, (unsigned int)bufferSize,
	                                          (unsigned int)width, (unsigned int)height, MEDIA_VISION_COLORSPACE_RGBA);
	   continue_if(error_code != MEDIA_VISION_ERROR_NONE);

	   free(dataBuffer);
	   dataBuffer = NULL;

	   error_code = mv_face_recognition_model_add(facedata.g_source,
	                                               facedata.g_face_recog_model, &roi, face_label);
	   continue_if(error_code != MEDIA_VISION_ERROR_NONE);
	}

	/* Use the mv_face_recognition_model_learn() function to train the face recognition model with the added */
	error_code = mv_face_recognition_model_learn(facedata.g_engine_config, facedata.g_face_recog_model);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	app_data_dir = app_get_data_path();
	goto_if(!app_data_dir, ERROR);

	snprintf(filePath, FILEPATH_SIZE, "%s%s", app_data_dir, FACE_MODEL_FILE_NAME);
	_D("Creating a model file[%s]", filePath);
	free(app_data_dir);

	error_code = mv_face_recognition_model_save(filePath, facedata.g_face_recog_model);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	image_util_decode_destroy(imageDecoder);
	imageDecoder = NULL;

	free(app_res_dir);

	return 0;

ERROR:
	if (app_data_dir)
		free(app_data_dir);

	if (dataBuffer)
		free(dataBuffer);

	if (imageDecoder)
		image_util_decode_destroy(imageDecoder);

	if (app_res_dir)
		free(app_res_dir);

	return -1;
}

static gboolean _after_recognize_cb(void *user_data)
{
	app_data *ad = user_data;

	int ret = 0;

	retv_if(!user_data, FALSE);

	ad->recognize_label = facedata.recognize_label;
	ad->recognize_percent = facedata.recognize_percent;
	ad->recognize_x = facedata.recognize_x;
	ad->recognize_y = facedata.recognize_y;
	ad->recognize_width = facedata.recognize_width;
	ad->recognize_height = facedata.recognize_height;

	if (ad->recognize_percent > MINIMUM_RECOGNIZE) {
		ret = tp_initialize("czRXVbgv72ILyJUl", &ad->handle);
		retv_if(ret != 0, FALSE);

		ret = tp_set_field_value(ad->handle, 1, "1");
		goto_if(ret != 0, ERROR);

		ret = tp_send_data(ad->handle);
		goto_if(ret != 0, ERROR);

		tp_finalize(ad->handle);
	}

	return FALSE;
ERROR:
	tp_finalize(ad->handle);
	return FALSE;
}

static void _on_face_recognized_cb(mv_source_h source, mv_face_recognition_model_h recognition_model,
                       mv_engine_config_h engine_config, mv_rectangle_s *face_location,
                       const int *face_label, double confidence, void *user_data)
{
	int ret = 0;

    if (face_label) {
    	facedata.recognize_label = *face_label;
    	facedata.recognize_percent = confidence;
    	facedata.recognize_x = face_location->point.x;
    	facedata.recognize_y = face_location->point.y;
    	facedata.recognize_width = face_location->width;
    	facedata.recognize_height = face_location->height;

        _D("Face Recognized : Label[%d], Confidence [%.2f], [%d,%d], [%d:%d]",
        		facedata.recognize_label,
				facedata.recognize_percent,
				facedata.recognize_x,
				facedata.recognize_y,
				facedata.recognize_width,
				facedata.recognize_height);

        _D("Relay On");
        ret = resource_write_relay(19, 1);
        if (ret < 0) _E("cannot control the relay");

        //ret = resource_write_relay(26, 1);
        //if (ret < 0) _E("cannot control the relay");

        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
        	_after_recognize_cb, user_data, NULL);
    } else {
        _D("Relay Off");
        ret = resource_write_relay(19, 0);
        if (ret < 0) _E("cannot control the relay");

        //ret = resource_write_relay(26, 0);
        //if (ret < 0) _E("cannot control the relay");
    }
}

static int _recognize_face()
{
	char filePath[FILEPATH_SIZE] = {0, };
	char *app_res_dir = NULL;

	image_util_decode_h imageDecoder = NULL;

	unsigned char *dataBuffer = NULL;
	unsigned long long bufferSize = 0;

	unsigned long width = 0;
	unsigned long height = 0;

	int error_code = 0;

	app_res_dir = app_get_resource_path();
	retv_if(!app_res_dir, -1);

	/* Decode the image and fill the image data to g_source handle */
	snprintf(filePath, FILEPATH_SIZE, "%s/images/face_sample_10.png", app_res_dir);
	free(app_res_dir);

	error_code = image_util_decode_create(&imageDecoder);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	error_code = image_util_decode_set_input_path(imageDecoder, filePath);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* The image space of the model has to be same with a new image which will be recognized. */
	error_code = image_util_decode_set_colorspace(imageDecoder, IMAGE_UTIL_COLORSPACE_RGBA8888);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	error_code = image_util_decode_set_output_buffer(imageDecoder, &dataBuffer);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	error_code = image_util_decode_run(imageDecoder, &width, &height, &bufferSize);
	goto_if(error_code != IMAGE_UTIL_ERROR_NONE, ERROR);

	image_util_decode_destroy(imageDecoder);
	imageDecoder = NULL;

	error_code = mv_source_clear(facedata.g_source);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	error_code = mv_source_fill_by_buffer(facedata.g_source, dataBuffer, (unsigned int)bufferSize,
	                                      (unsigned int)width, (unsigned int)height, MEDIA_VISION_COLORSPACE_RGBA);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	free(dataBuffer);
	dataBuffer = NULL;

	error_code = mv_face_recognize(facedata.g_source, facedata.g_face_recog_model, facedata.g_engine_config,
	                               NULL, _on_face_recognized_cb, NULL);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	return 0;

ERROR:
	if (dataBuffer)
		free(dataBuffer);

	if (imageDecoder)
		image_util_decode_destroy(imageDecoder);

	return -1;
}

static const char *_colorspace_err_to_str(image_util_colorspace_e colorspace)
{
	const char *str = NULL;
	switch (colorspace) {
	case IMAGE_UTIL_COLORSPACE_YV12:
		str = "YV12 - YCrCb planar format";
		break;
	case IMAGE_UTIL_COLORSPACE_YUV422:
		str = "YUV422 - planar";
		break;
	case IMAGE_UTIL_COLORSPACE_I420:
		str = "YUV420 - planar";
		break;
	case IMAGE_UTIL_COLORSPACE_NV12:
		str = "NV12- planar";
		break;
	case IMAGE_UTIL_COLORSPACE_UYVY:
		str = "UYVY - packed";
		break;
	case IMAGE_UTIL_COLORSPACE_YUYV:
		str = "YUYV - packed";
		break;
	case IMAGE_UTIL_COLORSPACE_RGB565:
		str = "RGB565, high-byte is Blue";
		break;
	case IMAGE_UTIL_COLORSPACE_RGB888:
		str = "RGB888, high-byte is Blue";
		break;
	case IMAGE_UTIL_COLORSPACE_ARGB8888:
		str = "ARGB8888, high-byte is Blue";
		break;
	case IMAGE_UTIL_COLORSPACE_BGRA8888:
		str = "BGRA8888, high-byte is Alpha";
		break;
	case IMAGE_UTIL_COLORSPACE_RGBA8888:
		str = "RGBA8888, high-byte is Alpha";
		break;
	case IMAGE_UTIL_COLORSPACE_BGRX8888:
		str = "BGRX8888, high-byte is X";
		break;
	case IMAGE_UTIL_COLORSPACE_NV21:
		str = "NV21- planar";
		break;
	case IMAGE_UTIL_COLORSPACE_NV16:
		str = "NV16- planar";
		break;
	case IMAGE_UTIL_COLORSPACE_NV61:
		str = "NV61- planar";
		break;
	default:
		str = "Nothing";
		break;
	}

	return str;
}

static bool _supported_colorspace_cb(image_util_colorspace_e colorspace, void *user_data)
{
	_D("%s", _colorspace_err_to_str(colorspace));
	return true;
}

static void _check_supported_type(void)
{
	int error_code = 0;

	error_code = image_util_foreach_supported_colorspace(IMAGE_UTIL_JPEG,
			_supported_colorspace_cb, NULL);
	ret_if(error_code != IMAGE_UTIL_ERROR_NONE);
}

int face_recognize(void)
{
	char filePath[FILEPATH_SIZE] = {0, };
	char *app_data_dir = NULL;

	int error_code = 0;

	/* Use this routine when you want to check which types your device is supporting */
	/* RPI3 B+ Tizen 5.5
	 * RGBA8888, high-byte is Alpha
	 * BGRA8888, high-byte is Alpha
	 * ARGB8888, high-byte is Blue
	 * RGB888, high-byte is Blue
	 * NV12- planar
	 * YUV420 - planar
	 * YV12 - YCrCb planar format
	 */
#if 0
	_check_supported_type();
#endif

	/* Create the source and engine configuration handles: */
	error_code = mv_create_source(&facedata.g_source);
	retv_if(error_code != MEDIA_VISION_ERROR_NONE, -1);

	error_code = mv_create_engine_config(&facedata.g_engine_config);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	app_data_dir = app_get_data_path();
	goto_if(!app_data_dir, ERROR);

	snprintf(filePath, FILEPATH_SIZE, "%s%s", app_data_dir, FACE_MODEL_FILE_NAME);
	free(app_data_dir);

	if (access(filePath, F_OK)) {
		_D("Creating a face model");
		error_code = _create_model();
		goto_if(error_code != 0, ERROR);
	} else {
		_D("Loading the face model from %s", filePath);
		error_code = mv_face_recognition_model_load(filePath, &facedata.g_face_recog_model);
		goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);
	}

	error_code = _recognize_face();
	goto_if(error_code != 0, ERROR);

	mv_destroy_source(facedata.g_source);
	facedata.g_source = NULL;

	mv_destroy_engine_config(facedata.g_engine_config);
	facedata.g_engine_config = NULL;

	mv_face_recognition_model_destroy(facedata.g_face_recog_model);
	facedata.g_face_recog_model = NULL;

	return 0;

ERROR:
	if (facedata.g_source) {
		mv_destroy_source(facedata.g_source);
		facedata.g_source = NULL;
	}

	if (facedata.g_engine_config) {
		mv_destroy_engine_config(facedata.g_engine_config);
		facedata.g_engine_config = NULL;
	}

	if (facedata.g_face_recog_model) {
		mv_face_recognition_model_destroy(facedata.g_face_recog_model);
		facedata.g_face_recog_model = NULL;
	}

	return -1;
}


static int _recognize_face_with_source(mv_source_h source, void *data)
{
	int error_code = 0;

	error_code = mv_face_recognize(source, facedata.g_face_recog_model, facedata.g_engine_config,
	                               NULL, _on_face_recognized_cb, data);
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	return 0;

ERROR:
	return -1;
}

int face_recognize_with_source(mv_source_h source, void *data)
{
	char filePath[FILEPATH_SIZE] = {0, };
	char *app_data_dir = NULL;


	int error_code = 0;

	/* Engine */
	if (!facedata.g_engine_config) {
		error_code = mv_create_engine_config(&facedata.g_engine_config);
		goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);
	}

	/* Model */
	if (!facedata.g_face_recog_model) {
		app_data_dir = app_get_data_path();
		goto_if(!app_data_dir, ERROR);
		snprintf(filePath, FILEPATH_SIZE, "%s%s", app_data_dir, FACE_MODEL_FILE_NAME);
		free(app_data_dir);

		if (access(filePath, F_OK)) {
			_D("Creating a face model");
			error_code = _create_model();
			goto_if(error_code != 0, ERROR);
		} else {
			_D("Loading the face model from %s", filePath);
			error_code = mv_face_recognition_model_load(filePath, &facedata.g_face_recog_model);
			goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);
		}
	}

	error_code = _recognize_face_with_source(source, data);
	goto_if(error_code != 0, ERROR);

	return 0;

ERROR:
	if (facedata.g_source) {
		mv_destroy_source(facedata.g_source);
		facedata.g_source = NULL;
	}

	if (facedata.g_engine_config) {
		mv_destroy_engine_config(facedata.g_engine_config);
		facedata.g_engine_config = NULL;
	}

	if (facedata.g_face_recog_model) {
		mv_face_recognition_model_destroy(facedata.g_face_recog_model);
		facedata.g_face_recog_model = NULL;
	}

	return -1;
}

void face_unrecognize(void)
{
	mv_destroy_source(facedata.g_source);
	facedata.g_source = NULL;

	mv_destroy_engine_config(facedata.g_engine_config);
	facedata.g_engine_config = NULL;

	mv_face_recognition_model_destroy(facedata.g_face_recog_model);
	facedata.g_face_recog_model = NULL;
}
