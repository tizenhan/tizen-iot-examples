## I.o.T.A ( Initialize On Tizen AI )
* 한민수 (발표, 개발, 디버깅, 설계,기획,디자인)
* 박신호 (기획, 기술조언 )



## AI 얼굴 인식 출입 제어 서비스



## 프로젝트 배경 혹은 목적
* 많은 사람들이 이용하는 모든 출입문에 타이젠 AI 얼굴 인식 출입문,
  출입자 관리 제어 시스템을 적용하여 관리자와 이용자의 효율과 편리,
  보안성을 더하여 간절한 필요와 요구에 기여하고자 기획하였습니다.



## 파일 리스트
### 오픈소스(타이젠 예제 파일 ) 가져와 일부 수정 및 덧붙임
* 앱 라이프사이클 : app.h app_data_s
* 앱 라이프사이클 : app.c _tp_timer_cb, service_app_control, service_app_terminate
* 로컬서버 : index.html face
* 로컬서버 : script.js face
* 클라우드 : thingpark_api.c API 및 채널 관련 작업



### 직접 만든 소스 파일
* 카메라 : usb-camera.h
* 카메라 : usb-camera.c
* 얼굴 찾기 : face-detect.h
* 얼굴 찾기 : face-detect.c
* 이미지 자르기 : image-cropper.h
* 이미지 자르기 : image-cropper.c
* 얼굴 인식 : face-recognize.h
* 얼굴 인식 : face-recognize.c
* 릴레이 : resource_relay.h(재수정)
* 릴레이 : resource_relay.c(재수정)
* 릴레이 : resource_relay_internal.h(재수정)
* 로컬서버 : hs-route-api-face-detect.h
* 로컬서버 : hs-route-api-face-detect.c



## 코드 기여자
### 오픈소스(타이젠 예제 파일 ) 가져와 일부 수정및 덧붙임
* 앱 라이프사이클 : app.h app_data_s - 한민수
* 앱 라이프사이클 : app.c _tp_timer_cb, service_app_control, service_app_terminate - 한민수
* 로컬서버 : index.html face - 한민수
* 로컬서버 : script.js face - 한민수
* 클라우드 : thingpark_api.c API 및 채널 관련 작업 - 한민수

### 직접 만든 소스 파일
* 카메라 : usb-camera.h - 한민수
* 카메라 : usb-camera.c - 한민수
* 얼굴 찾기 : face-detect.h - 한민수
* 얼굴 찾기 : face-detect.c - 한민수
* 이미지 자르기 : image-cropper.h - 한민수
* 이미지 자르기 : image-cropper.c - 한민수
* 얼굴 인식 : face-recognize.h - 한민수
* 얼굴 인식 : face-recognize.c - 한민수
* 릴레이 : resource_relay.h(재수정) - 한민수
* 릴레이 : resource_relay.c - 한민수
* 릴레이 : resource_relay_internal.h(재수정) - 한민수
* 로컬서버 : hs-route-api-face-detect.h - 한민수
* 로컬서버 : hs-route-api-face-detect.c - 한민수



## 보드
* RPI3 B+ : [[ 카메라 얼굴 인식 및 릴레이 연동]
    (https://github.com/tizenhan/tizen-iot-examples)]



## 구현사항
* GPIO  프로토콜 사용
* thingspark 클라우드 사용
* HTTP Server 사용
* 카메라 사용
* 이미지 분석 기능 사용(Face dectect / Recognize)
