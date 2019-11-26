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

#ifndef __IMAGE_CROPPER_H__
#define __IMAGE_CROPPER_H__

int image_cropper_crop(unsigned char *image_data, unsigned int size, unsigned int start_x, unsigned int start_y, unsigned int end_x, unsigned int end_y, const char *file);

#endif /* __IMAGE_CROPPER_H__ */
