 /*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
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
#include <Ecore.h>
/* To use the functions and data types of the Camera API (in mobile and wearable applications),
 * include the <camera.h> header file in your application */
#include <camera.h>
#include <time.h>
#include <usb-camera.h>

#include "app.h"
#include "http-server-log-private.h"
#include "face-detect.h"

#define CAMERA_PREVIEW_INTERVAL_MIN 3000 // 1 sec
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240

struct _camdata {
    camera_h g_camera; /* Camera handle */
};
typedef struct _camdata camdata;
static camdata cam_data;


static void _print_camera_state(camera_state_e previous, camera_state_e current, bool by_policy, void *user_data)
{
	switch (current) {
	case CAMERA_STATE_CREATED:
		_D("Camera state: CAMERA_STATE_CREATED");
		break;
	case CAMERA_STATE_PREVIEW:
		_D("Camera state: CAMERA_STATE_PREVIEW");
		break;
	case CAMERA_STATE_CAPTURING:
		_D("Camera state: CAMERA_STATE_CAPTURING");
		break;
	case CAMERA_STATE_CAPTURED:
		_D("Camera state: CAMERA_STATE_CAPTURED");
		break;
	default:
		_D("Camera state: CAMERA_STATE_NONE");
		break;
	}
}

static long long int _get_monotonic_ms(void)
{
	long long int ret_time = 0;
	struct timespec time_s;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &time_s))
		ret_time = time_s.tv_sec* 1000 + time_s.tv_nsec / 1000000;
	else
		_E("Failed to get time");

	return ret_time;
}

static void _rotate_90(unsigned char *source, int width, int height, unsigned char *result)
{
	int idx = 0;
	int x = 0;
	int y;
	for (x = width - 1; x >= 0; --x) {
		for (y = 0; y < height; ++y) {
			result[idx++] = source[y * width + x];
		}
	}
}

static int _frame_to_source(camera_preview_data_s *frame, mv_source_h *source)
{
	mv_colorspace_e colorspace = MEDIA_VISION_COLORSPACE_INVALID;
	unsigned char *buff_y = NULL;
	int image_plane = 0;
	int size = 0;
	int error_code = 0;

	image_plane = frame->num_of_planes;
	size = frame->width * frame->height;

	switch (frame->format) {
	case CAMERA_PIXEL_FORMAT_NV12: /**< NV12 pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_NV12;
		break;
	case CAMERA_PIXEL_FORMAT_NV21: /**< NV21 pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_NV21;
		break;
	case CAMERA_PIXEL_FORMAT_YUYV: /**< YUYV(YUY2) pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_YUYV;
		break;
	case CAMERA_PIXEL_FORMAT_UYVY: /**< UYVY pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_UYVY;
		break;
	case CAMERA_PIXEL_FORMAT_422P: /**< YUV422(Y:U:V) planar pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_422P;
		break;
	case CAMERA_PIXEL_FORMAT_I420:
		if (frame->num_of_planes == 2)
			colorspace = MEDIA_VISION_COLORSPACE_NV12;
		else
			colorspace = MEDIA_VISION_COLORSPACE_I420;
		break;
	case CAMERA_PIXEL_FORMAT_YV12: /**< YV12 pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_YV12;
		break;
	case CAMERA_PIXEL_FORMAT_RGB565: /**< RGB565 pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_RGB565;
		break;
	case CAMERA_PIXEL_FORMAT_RGB888: /**< RGB888 pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_RGB888;
		break;
	case CAMERA_PIXEL_FORMAT_RGBA: /**< RGBA pixel format */
		colorspace = MEDIA_VISION_COLORSPACE_RGBA;
		break;
	default:
		_E("No case for this colorspace[%d]", colorspace);
	}
	retv_if(colorspace == MEDIA_VISION_COLORSPACE_INVALID, -1);

	if (*source) {
		//_D("Clear the MV source");
		mv_source_clear(*source);
	} else {
		//_D("Create a MV source");
		error_code = mv_create_source(source);
		retv_if(error_code != MEDIA_VISION_ERROR_NONE, -1);
	}

	// Image Plane : 3
	//_D("Image Plane : %d", image_plane);
	switch (image_plane) {
	case 3:
		buff_y = frame->data.triple_plane.y;
		break;
	case 2:
		buff_y = frame->data.double_plane.y;
		break;
	case 1:
		buff_y = frame->data.single_plane.yuv;
		break;
	default:
		_E("default : %d", image_plane);
	}

	//_D("Filling the source");
	error_code = mv_source_fill_by_buffer(*source, buff_y, size,
			frame->width,
			frame->height,
			MEDIA_VISION_COLORSPACE_Y800);
			//colorspace); /* FIXME : MEDIA_VISION_COLORSPACE_Y800 */
	goto_if(error_code != MEDIA_VISION_ERROR_NONE, ERROR);

	return 0;

ERROR:
	mv_destroy_source(*source);
	*source = NULL;
	return -1;
}

static void _camera_preview_cb(camera_preview_data_s *frame, void *user_data)
{
	static long long int last = 0;
	long long int now = _get_monotonic_ms();
	int error_code = 0;
	app_data *ad = user_data;

	if (now - last < CAMERA_PREVIEW_INTERVAL_MIN)
		return;

	error_code = _frame_to_source(frame, &ad->source);
	if (error_code != 0) _E("FAIL : Frame to source");

	error_code = face_detect_with_source(ad->source, user_data);
	if (error_code < 0) _E("Failed to detect faces");

	last = now;
}

int usb_camera_prepare(void *data)
{
	int error_code = 0;
	camera_state_e state;

	_D("Preparing your camera.");

	/* Create the camera handle */
	/* The CAMERA_DEVICE_CAMERA0 parameter means that the currently activated device camera is 0,
	 * which is the primary camera. You can select between the primary (0) and secondary (1) camera.
	 * These values are defined in the  camera_device_e enumeration (in mobile and wearable applications). */
	error_code = camera_create(CAMERA_DEVICE_CAMERA0, &cam_data.g_camera);
	retv_if(error_code != CAMERA_ERROR_NONE, -1);

	/* Check the camera state after creating the camera */
	/* The returned state is one of the values defined in the camera_state_e enumeration
	 * (in mobile and wearable applications).
	 * If the state is not CAMERA_STATE_CREATED, re-initialize the camera by recreating the handle. */
	error_code = camera_get_state(cam_data.g_camera, &state);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);
	goto_if(state != CAMERA_STATE_CREATED, ERROR);

	/* The image quality value can range from 1 (lowest quality) to 100 (highest quality). */
	error_code = camera_attr_set_image_quality(cam_data.g_camera, 100);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	error_code = camera_set_preview_resolution(cam_data.g_camera, IMAGE_WIDTH, IMAGE_HEIGHT);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	error_code = camera_set_capture_resolution(cam_data.g_camera, IMAGE_WIDTH, IMAGE_HEIGHT);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	/* CAMERA_PIXEL_FORMAT_RGBA : Not supported */
	/* FIXME : CAMERA_PIXEL_FORMAT_JPEG */
	error_code = camera_set_capture_format(cam_data.g_camera, CAMERA_PIXEL_FORMAT_JPEG);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	error_code = camera_set_state_changed_cb(cam_data.g_camera, _print_camera_state, NULL);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	error_code = camera_set_preview_cb(cam_data.g_camera, _camera_preview_cb, data);
	goto_if(error_code != CAMERA_ERROR_NONE, ERROR);

	return 0;

ERROR:
	if (cam_data.g_camera) {
		camera_destroy(cam_data.g_camera);
		cam_data.g_camera = NULL;
	}

	return -1;
}

static const char * _cam_err_to_str(camera_error_e err)
{
	const char *err_str;

	switch (err) {
	case CAMERA_ERROR_NONE:
		err_str = "CAMERA_ERROR_NONE";
		break;
	case CAMERA_ERROR_INVALID_PARAMETER:
		err_str = "CAMERA_ERROR_INVALID_PARAMETER";
		break;
	case CAMERA_ERROR_INVALID_STATE:
		err_str = "CAMERA_ERROR_INVALID_STATE";
		break;
	case CAMERA_ERROR_OUT_OF_MEMORY:
		err_str = "CAMERA_ERROR_OUT_OF_MEMORY";
		break;
	case CAMERA_ERROR_DEVICE:
		err_str = "CAMERA_ERROR_DEVICE";
		break;
	case CAMERA_ERROR_INVALID_OPERATION:
		err_str = "CAMERA_ERROR_INVALID_OPERATION";
		break;
	case CAMERA_ERROR_SECURITY_RESTRICTED:
		err_str = "CAMERA_ERROR_SECURITY_RESTRICTED";
		break;
	case CAMERA_ERROR_DEVICE_BUSY:
		err_str = "CAMERA_ERROR_DEVICE_BUSY";
		break;
	case CAMERA_ERROR_DEVICE_NOT_FOUND:
		err_str = "CAMERA_ERROR_DEVICE_NOT_FOUND";
		break;
	case CAMERA_ERROR_ESD:
		err_str = "CAMERA_ERROR_ESD";
		break;
	case CAMERA_ERROR_PERMISSION_DENIED:
		err_str = "CAMERA_ERROR_PERMISSION_DENIED";
		break;
	case CAMERA_ERROR_NOT_SUPPORTED:
		err_str = "CAMERA_ERROR_NOT_SUPPORTED";
		break;
	case CAMERA_ERROR_RESOURCE_CONFLICT:
		err_str = "CAMERA_ERROR_RESOURCE_CONFLICT";
		break;
	case CAMERA_ERROR_SERVICE_DISCONNECTED:
		err_str = "CAMERA_ERROR_SERVICE_DISCONNECTED";
		break;
	default:
		err_str = "Unknown";
		break;
	}
	return err_str;
}

int usb_camera_preview(void *data)
{
	camera_state_e state;
	int error_code = 0;

	retv_if(!cam_data.g_camera, -1);

	_D("Start to preview with your camera.");

	error_code = camera_get_state(cam_data.g_camera, &state);
	if (error_code != CAMERA_ERROR_NONE) {
		_E("Failed to get camera state [%s]", _cam_err_to_str(error_code));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		_D("Camera is now capturing");
		return -1;
	}

	if (state != CAMERA_STATE_PREVIEW) {
		error_code = camera_start_preview(cam_data.g_camera);
		if (error_code != CAMERA_ERROR_NONE) {
			_E("Failed to start preview [%s]", _cam_err_to_str(error_code));
			return -1;
		}
	}

	return 0;
}

static void __capturing_cb(camera_image_data_s *image, camera_image_data_s *postview, camera_image_data_s *thumbnail, void *user_data)
{
	if (image == NULL) {
		_E("Image is NULL");
		return;
	}

	_D("Now is on Capturing: Image size[%d x %d]", image->width, image->height);
}

static void _completed_cb(void *user_data)
{
	int error_code = 0;

	if (!cam_data.g_camera) {
		_E("Camera is NULL");
		return;
	}

	error_code = camera_start_preview(cam_data.g_camera);
	if (error_code != CAMERA_ERROR_NONE) {
		_E("Failed to start preview [%s]", _cam_err_to_str(error_code));
		return;
	}
}

int usb_camera_capture(void *user_data)
{
	camera_state_e state;
	int error_code = 0;

	retv_if(!cam_data.g_camera, -1);

	_D("Try to capture the preview.");

	error_code = camera_get_state(cam_data.g_camera, &state);
	if (error_code != CAMERA_ERROR_NONE) {
		_E("Failed to get camera state [%s]", _cam_err_to_str(error_code));
		return -1;
	}

	if (state == CAMERA_STATE_CAPTURING) {
		_D("Camera is now capturing");
		return -1;
	}

	if (state != CAMERA_STATE_PREVIEW) {
		_I("Preview is not started [%d]", state);
		error_code = camera_start_preview(cam_data.g_camera);
		if (error_code != CAMERA_ERROR_NONE) {
			_E("Failed to start preview [%s]", _cam_err_to_str(error_code));
			return -1;
		}
	}

	error_code = camera_start_capture(cam_data.g_camera, __capturing_cb, _completed_cb, user_data);
	if (error_code != CAMERA_ERROR_NONE) {
		_E("Failed to start capturing [%s]", _cam_err_to_str(error_code));
		return -1;
	}

	return 0;
}

void usb_camera_unprepare(void *data)
{
	if (!cam_data.g_camera)
		return;

	camera_unset_preview_cb(cam_data.g_camera);
	camera_stop_preview(cam_data.g_camera);

	camera_destroy(cam_data.g_camera);
	cam_data.g_camera = NULL;
}
