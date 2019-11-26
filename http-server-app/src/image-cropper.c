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

/* To use the functions and data types of the Image Util API (in mobile and wearable applications),
 * include the  <image_util.h> header file in your application: */
#include <image_util.h>

#include "http-server-log-private.h"

int image_cropper_crop(unsigned char *image_data, unsigned int size, unsigned int start_x, unsigned int start_y, unsigned int end_x, unsigned int end_y, const char *file)
{
	image_util_decode_h decode_h = NULL;
	transformation_h transform_h = NULL;

	image_util_image_h src_image = NULL;
	image_util_image_h dst_image = NULL;

	image_util_encode_h encode_h = NULL;

	int ret = 0;

	/* To support image_util_transform_run(), which is used for all image transformations,
	 * set the source image and create a handle for it (to be used as the second parameter): */
	ret = image_util_decode_create(&decode_h);
	retv_if(ret != IMAGE_UTIL_ERROR_NONE, -1);

	ret = image_util_decode_set_input_buffer(decode_h, image_data, size);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	ret = image_util_decode_set_colorspace(decode_h, IMAGE_UTIL_COLORSPACE_YUV422);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Decodes the image with the given decode handle. */
	/* This function decodes the image synchronously. */
	ret = image_util_decode_run2(decode_h, &src_image);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Ready for a source image to crop.");

	image_util_decode_destroy(decode_h);
	decode_h = NULL;

	/* Create a transformation handle using image_util_transform_create(): */
	ret = image_util_transform_create(&transform_h);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	int width = end_x - start_x;
	int height = end_y - start_y;

	width = ((width + 8) / 16) * 16;
	height = ((height + 8) / 16) * 16;

	/* Set the crop area using image_util_transform_set_crop_area(): */
	ret = image_util_transform_set_crop_area(transform_h, start_x, start_y, start_x + width, start_y + height);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Execute the transformation using image_util_transform_run2() */
	/* Synchronously transforms an image with the given transformation handle. */
	ret = image_util_transform_run2(transform_h, src_image, &dst_image);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Checking a crop area");

	image_util_transform_destroy(transform_h);
	transform_h = NULL;

	/* Create an encoding handle using image_util_encode_create(): */
	ret = image_util_encode_create(IMAGE_UTIL_JPEG, &encode_h);
	//ret = image_util_encode_create(IMAGE_UTIL_BMP, &encode_h);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	ret = image_util_encode_set_resolution(encode_h, width, height);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Additionally, you can set the JPEG quality or PNG compression using image_util_encode_set_quality() or image_util_encode_set_png_compression(): */
	ret = image_util_encode_set_quality(encode_h, 100);
	//goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	ret = image_util_encode_set_output_path(encode_h, file);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	unsigned long long *cropped_size = 0;
	ret = image_util_encode_run(encode_h, &cropped_size);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Execute the encoding using image_util_encode_run_to_file() or image_util_encode_run_to_buffer(): */
	/* The function executes synchronously. */
	// ret = image_util_encode_run_to_file(encode_h, dst_image, file);
	// goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Encode the buffer to a jpeg file(%lld)", cropped_size);

	image_util_encode_destroy(encode_h);
	encode_h = NULL;

	image_util_destroy_image(src_image);
	src_image = NULL;

	image_util_destroy_image(dst_image);
	dst_image = NULL;

	return 0;

ERROR:
	if (decode_h)
		image_util_decode_destroy(decode_h);

	if (transform_h)
		image_util_transform_destroy(transform_h);

	if (encode_h)
		image_util_encode_destroy(encode_h);

	if (src_image)
		image_util_destroy_image(src_image);

	if (dst_image)
		image_util_destroy_image(dst_image);

	return -1;
}

int image_cropper_crop_data(unsigned char *image_data,
		unsigned int size,
		//mv_colorspace_e image_colorspace,
		unsigned int start_x,
		unsigned int start_y,
		unsigned int end_x,
		unsigned int end_y,
		unsigned char *result)
{
	image_util_decode_h decode_h = NULL;
	transformation_h transform_h = NULL;

	image_util_image_h src_image = NULL;
	image_util_image_h dst_image = NULL;

	image_util_encode_h encode_h = NULL;

	int ret = 0;

	/* To support image_util_transform_run(), which is used for all image transformations,
	 * set the source image and create a handle for it (to be used as the second parameter): */
	ret = image_util_decode_create(&decode_h);
	retv_if(ret != IMAGE_UTIL_ERROR_NONE, -1);

	ret = image_util_decode_set_input_buffer(decode_h, image_data, size);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	//ret = image_util_decode_set_colorspace(decode_h, image_colorspace);
	//goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Decodes the image with the given decode handle. */
	/* This function decodes the image synchronously. */
	ret = image_util_decode_run2(decode_h, &src_image);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Ready for a source image to crop.");

	image_util_decode_destroy(decode_h);
	decode_h = NULL;

	/* Create a transformation handle using image_util_transform_create(): */
	ret = image_util_transform_create(&transform_h);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	int width = end_x - start_x;
	int height = end_y - start_y;

	width = ((width + 8) / 16) * 16;
	height = ((height + 8) / 16) * 16;

	/* Set the crop area using image_util_transform_set_crop_area(): */
	ret = image_util_transform_set_crop_area(transform_h, start_x, start_y, start_x + width, start_y + height);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Execute the transformation using image_util_transform_run2() */
	/* Synchronously transforms an image with the given transformation handle. */
	ret = image_util_transform_run2(transform_h, src_image, &dst_image);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Checking a crop area");

	image_util_transform_destroy(transform_h);
	transform_h = NULL;

	/* Create an encoding handle using image_util_encode_create(): */
	ret = image_util_encode_create(IMAGE_UTIL_JPEG, &encode_h);
	//ret = image_util_encode_create(IMAGE_UTIL_BMP, &encode_h);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	ret = image_util_encode_set_resolution(encode_h, width, height);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Additionally, you can set the JPEG quality or PNG compression using image_util_encode_set_quality() or image_util_encode_set_png_compression(): */
	ret = image_util_encode_set_quality(encode_h, 100);
	//goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	//ret = image_util_encode_set_output_path(encode_h, file);
	//goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	unsigned long long *cropped_size = 0;
	ret = image_util_encode_run(encode_h, &cropped_size);
	goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	/* Execute the encoding using image_util_encode_run_to_file() or image_util_encode_run_to_buffer(): */
	/* The function executes synchronously. */
	// ret = image_util_encode_run_to_file(encode_h, dst_image, file);
	// goto_if(ret != IMAGE_UTIL_ERROR_NONE, ERROR);

	_D("Encode the buffer to a jpeg file(%lld)", cropped_size);

	image_util_encode_destroy(encode_h);
	encode_h = NULL;

	image_util_destroy_image(src_image);
	src_image = NULL;

	image_util_destroy_image(dst_image);
	dst_image = NULL;

	return 0;

ERROR:
	if (decode_h)
		image_util_decode_destroy(decode_h);

	if (transform_h)
		image_util_transform_destroy(transform_h);

	if (encode_h)
		image_util_encode_destroy(encode_h);

	if (src_image)
		image_util_destroy_image(src_image);

	if (dst_image)
		image_util_destroy_image(dst_image);

	return -1;
}
