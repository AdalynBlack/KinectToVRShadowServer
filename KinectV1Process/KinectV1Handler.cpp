#include "stdafx.h"
#include "KinectV1Handler.h"
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glew.h"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <KinectSettings.h>
#include <sfLine.h>
#include <iostream>
#include <KinectJoint.h>
#include <math.h>

#include <../LowPassFilter.h>

LowPassFilter lowPassFilter[3][4] = {
	{
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005)
	},
	{
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005)
	},
	{
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005),
		LowPassFilter(7.1, 0.005)
	}
};

static Networking networking;

void KinectV1Handler::initOpenGL()
{
	LOG(INFO) << "Attempted to initialise OpenGL";
	int width = 0, height = 0;
	if (kVersion == KinectVersion::Version1)
	{
		width = KinectSettings::kinectWidth;
		height = KinectSettings::kinectHeight;
	}
	else if (kVersion == KinectVersion::Version2)
	{
		width = KinectSettings::kinectV2Width;
		height = KinectSettings::kinectV2Height;
	} // REMOVE THIS INTO KINECT V2 IMPL
	// Initialize textures
	glGenTextures(1, &kinectTextureId);
	glBindTexture(GL_TEXTURE_2D, kinectTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
	             0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, static_cast<GLvoid*>(kinectImageData.get()));
	glBindTexture(GL_TEXTURE_2D, 0);

	// OpenGL setup
	glClearColor(1, 0, 0, 0);
	glClearDepth(1.0f);
	glEnable(GL_TEXTURE_2D);

	// Camera setup
	glViewport(0, 0, SFMLsettings::m_window_width, SFMLsettings::m_window_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, SFMLsettings::m_window_width, SFMLsettings::m_window_height, 0, 1, -1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

HRESULT KinectV1Handler::getStatusResult()
{
	return netStatus();
	/*if (kinectSensor)
		return kinectSensor->NuiStatus();
	return E_NUI_NOTCONNECTED;*/
}

std::string KinectV1Handler::statusResultString(HRESULT stat)
{
	switch (stat)
	{
	case S_OK: return "S_OK";
	case S_NUI_INITIALIZING: return "S_NUI_INITIALIZING\nThe device is connected, but still initializing.";
	case E_NUI_NOTCONNECTED: return "E_NUI_NOTCONNECTED\nThe device is not connected.";
	// case E_NUI_NOTGENUINE: return "E_NUI_NOTGENUINE The device is not a valid Kinect.";
	case E_NUI_NOTGENUINE: return "E_NUI_NOTGENUINE\nThe Kinect SDK is reporting your device as invalid, try reconnecting it physically to the computer\nor restarting, then click \"Reconnect Kinect\".";
	// case E_NUI_NOTSUPPORTED: return "E_NUI_NOTSUPPORTED The device is an unsupported model.";
	case E_NUI_NOTSUPPORTED: return "E_NUI_NOTSUPPORTED\nXbox 360 Kinect requires the Kinect for Windows SDK 1.8 to function.\nYou only have the runtime installed.";
	// case E_NUI_INSUFFICIENTBANDWIDTH: return "E_NUI_INSUFFICIENTBANDWIDTH The device is connected to a hub without the necessary bandwidth requirements.";
	case E_NUI_INSUFFICIENTBANDWIDTH: return "E_NUI_INSUFFICIENTBANDWIDTH\nA USB error occured! Try reconnecting the device physically or restarting your computer.";
	// case E_NUI_NOTPOWERED: return "E_NUI_NOTPOWERED The device is connected, but unpowered.";
	case E_NUI_NOTPOWERED: return "E_NUI_NOTPOWERED\nCheck that your adapter is lit up.\nIf it is, look in Device Manager under \"other devices\"\nand as a last resort, try reinstalling the Kinect drivers entirely.";
	// case E_NUI_NOTREADY: return "E_NUI_NOTREADY There was some other unspecified error.";
	case E_NUI_NOTREADY: return "E_NUI_NOTREADY\nTry reconnecting the USB port of the device or restarting your PC.\nIf it doesn't work, reinstall the Kinect for Windows SDK.";
	case S_FALSE: return "S_FALSE\nClient not connected, please connect it";
	default: return "Uh Oh undefined kinect error! " + std::to_string(stat);
	}
}

HRESULT KinectV1Handler::netStatus()
{
	if (networking.connected)
	{
		return S_OK;
	}
	else {
		return S_FALSE;
	}
}

void KinectV1Handler::initialise()
{
	try
	{
		kVersion = KinectVersion::Version1;
		kinectImageData
			= std::make_unique<GLubyte[]>(KinectSettings::kinectWidth * KinectSettings::kinectHeight * 4); // BGRA
		initialised = initKinect();
		LOG_IF(initialised, INFO) << "Kinect initialised successfully!";
		if (!initialised) throw FailedKinectInitialisation;
	}
	catch (std::exception& e)
	{
		LOG(ERROR) << "Failed to initialise Kinect " << e.what() << std::endl;
	}
}

void KinectV1Handler::update()
{
	if (isInitialised())
	{
		HRESULT kinectStatus = netStatus(); //kinectSensor->NuiStatus();
		printf("status: %d\n", kinectStatus);
		if (kinectStatus == S_OK)
		{
			getKinectRGBData();
			updateSkeletalData();
		}
		else {
			printf("wait for client\n");
			networking.waitForClient();
			printf("client connected\n");
			Sleep(1000);
		}
	}
}

void KinectV1Handler::drawKinectData(sf::RenderWindow& drawingWindow)
{
	if (isInitialised())
	{
		if (KinectSettings::isKinectDrawn)
		{
			drawKinectImageData(drawingWindow);
		}
		if (KinectSettings::isSkeletonDrawn)
		{
			drawTrackedSkeletons(drawingWindow);
		}
	}
};

void KinectV1Handler::drawKinectImageData(sf::RenderWindow& drawingWindow)
{
	glBindTexture(GL_TEXTURE_2D, kinectTextureId);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SFMLsettings::m_window_width, SFMLsettings::m_window_height, GL_BGRA_EXT,
	                GL_UNSIGNED_BYTE, static_cast<GLvoid*>(kinectImageData.get()));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(0, 0, 0);
	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(SFMLsettings::m_window_width, 0, 0);
	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(SFMLsettings::m_window_width, SFMLsettings::m_window_height, 0.0f);
	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(0, SFMLsettings::m_window_height, 0.0f);

	glEnd();
};

NUI_SKELETON_DATA backup;

void KinectV1Handler::drawTrackedSkeletons(sf::RenderWindow& drawingWindow)
{
	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
	{
		screenSkelePoints[i] = sf::Vector2f(0.0f, 0.0f);
	}
	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

		if (NUI_SKELETON_TRACKED == trackingState || NUI_SKELETON_POSITION_ONLY == trackingState)
			backup = skeletonFrame.SkeletonData[i];
	}

	if (KinectSettings::isSkeletonDrawn)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = backup.eTrackingState;
		if (NUI_SKELETON_TRACKED == trackingState)
		{
			drawingWindow.pushGLStates();
			drawingWindow.resetGLStates();

			DrawSkeleton(backup, drawingWindow);
			drawingWindow.popGLStates();
		}
		else if (NUI_SKELETON_POSITION_ONLY == trackingState)
		{
			sf::CircleShape circle(KinectSettings::g_JointThickness, 30);
			circle.setRadius(KinectSettings::g_JointThickness);
			circle.setPosition(SkeletonToScreen(backup.Position, SFMLsettings::m_window_width,
			                                    SFMLsettings::m_window_height));
			circle.setFillColor(sf::Color::Yellow);

			drawingWindow.pushGLStates();
			drawingWindow.resetGLStates();
			drawingWindow.draw(circle);
			drawingWindow.popGLStates();
		}
	}
};

//Consider moving this tracking stuff into a seperate class
void KinectV1Handler::zeroAllTracking(vr::IVRSystem*& m_sys)
{
	// Holdover from previous implementation
	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

		if (NUI_SKELETON_TRACKED == trackingState)
		{
			//KinectSettings::hmdZero = getHMDPosition(m_sys);

			setKinectToVRMultiplier(i);
			zeroed = true;
			break;
		}
	}
}

void KinectV1Handler::updateTrackersWithSkeletonPosition(
	std::vector<KVR::KinectTrackedDevice>& trackers)
{
	for (KVR::KinectTrackedDevice& device : trackers)
	{
		if (device.isSensor())
		{
			device.update(KinectSettings::kinectRepPosition, {0, 0, 0}, KinectSettings::kinectRepRotation);
		}
		else
		{
			vr::HmdVector3d_t jointPosition{0, 0, 0};
			vr::HmdQuaternion_t jointRotation{0, 0, 0, 0};
			if (getFilteredJoint(device, jointPosition, jointRotation))
			{
				device.update(trackedPositionVROffset, jointPosition, jointRotation);
			}
		}
	}
}

bool KinectV1Handler::getFilteredJoint(KVR::KinectTrackedDevice device, vr::HmdVector3d_t& position,
                                       vr::HmdQuaternion_t& rotation)
{
	for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
	{
		NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

		if (trackingState == NUI_SKELETON_TRACKED)
		{
			// If we can't find either of these joints, exit
			if (jointsUntracked(device.joint0, device.joint1, skeletonFrame.SkeletonData[i]))
			{
				return false;
			}

			// Don't track if both points are inferred
			bool ignoreInferredJoints = true;
			if (ignoreInferredJoints && jointsInferred(device.joint0, device.joint1, skeletonFrame.SkeletonData[i]))
			{
				return false;
			}

			{
				float jointX = jointPositions[convertJoint(device.joint0)].x;
				float jointY = jointPositions[convertJoint(device.joint0)].y;
				float jointZ = jointPositions[convertJoint(device.joint0)].z;
				position = vr::HmdVector3d_t{jointX, jointY, jointZ};

				//Rotation - Need to seperate into function
				Vector4 kRotation = {0, 0, 0, 1};
				switch (device.rotationFilterOption)
				{
				case KVR::JointRotationFilterOption::Unfiltered:
					kRotation = boneOrientations[convertJoint(device.joint0)].absoluteRotation.rotationQuaternion;
					break;
				case KVR::JointRotationFilterOption::Filtered:
					kRotation = rotFilter.GetFilteredJoints()[convertJoint(device.joint0)];
					break;
				case KVR::JointRotationFilterOption::HeadLook:
					{
						// Ew
						auto q = KinectSettings::hmdRotation;
						//Isolate Yaw
						float yaw = atan2(2 * q.w * q.y + 2 * q.x * q.z,
						                  +q.w * q.w + q.x * q.x - q.z * q.z - q.y * q.y);

						auto kq = vrmath::quaternionFromRotationY(yaw);
						kRotation.w = kq.w;
						kRotation.x = kq.x;
						kRotation.y = kq.y;
						kRotation.z = kq.z;
					}
					break;
				default:
					LOG(ERROR) << "JOINT ROTATION OPTION UNDEFINED IN DEVICE " << device.deviceId;
					break;
				}
				rotation.w = kRotation.w;
				rotation.x = kRotation.x;
				rotation.y = kRotation.y;
				rotation.z = kRotation.z;

				return true;
			}
		}
	}
	return false;
}

NUI_SKELETON_POSITION_INDEX KinectV1Handler::convertJoint(KVR::KinectJoint joint)
{
	using namespace KVR;
	//Unfortunately I believe this is required because there are mismatches between v1 and v2 joint IDs
	//Might consider investigating to see if there's a way to shorten this
	switch (joint.joint)
	{
	case KinectJointType::SpineBase:
		return NUI_SKELETON_POSITION_HIP_CENTER;
	case KinectJointType::SpineMid:
		return NUI_SKELETON_POSITION_SPINE;

	case KinectJointType::Head:
		return NUI_SKELETON_POSITION_HEAD;
	case KinectJointType::ShoulderLeft:
		return NUI_SKELETON_POSITION_SHOULDER_LEFT;
	case KinectJointType::ShoulderRight:
		return NUI_SKELETON_POSITION_SHOULDER_RIGHT;
	case KinectJointType::SpineShoulder:
		return NUI_SKELETON_POSITION_SHOULDER_CENTER;

	case KinectJointType::ElbowLeft:
		return NUI_SKELETON_POSITION_ELBOW_LEFT;
	case KinectJointType::WristLeft:
		return NUI_SKELETON_POSITION_WRIST_LEFT;
	case KinectJointType::HandLeft:
		return NUI_SKELETON_POSITION_HAND_LEFT;

	case KinectJointType::ElbowRight:
		return NUI_SKELETON_POSITION_ELBOW_RIGHT;
	case KinectJointType::WristRight:
		return NUI_SKELETON_POSITION_WRIST_RIGHT;
	case KinectJointType::HandRight:
		return NUI_SKELETON_POSITION_HAND_RIGHT;

	case KinectJointType::HipLeft:
		return NUI_SKELETON_POSITION_HIP_LEFT;
	case KinectJointType::HipRight:
		return NUI_SKELETON_POSITION_HIP_RIGHT;

	case KinectJointType::KneeLeft:
		return NUI_SKELETON_POSITION_KNEE_LEFT;
	case KinectJointType::KneeRight:
		return NUI_SKELETON_POSITION_KNEE_RIGHT;

	case KinectJointType::AnkleLeft:
		return NUI_SKELETON_POSITION_ANKLE_LEFT;
	case KinectJointType::AnkleRight:
		return NUI_SKELETON_POSITION_ANKLE_RIGHT;

	case KinectJointType::FootLeft:
		return NUI_SKELETON_POSITION_FOOT_LEFT;
	case KinectJointType::FootRight:
		return NUI_SKELETON_POSITION_FOOT_RIGHT;

		/*BELOW DO NOT HAVE A 1:1 V1 REPRESENTATION*/
		//refer to the skeleton images from Microsoft for diffs between v1 and 2

	case KinectJointType::Neck:
		return NUI_SKELETON_POSITION_SHOULDER_CENTER;
	case KinectJointType::HandTipLeft:
		return NUI_SKELETON_POSITION_HAND_LEFT;
	case KinectJointType::HandTipRight:
		return NUI_SKELETON_POSITION_HAND_RIGHT;
	case KinectJointType::ThumbLeft:
		return NUI_SKELETON_POSITION_HAND_LEFT;
	case KinectJointType::ThumbRight:
		return NUI_SKELETON_POSITION_HAND_RIGHT;

	default:
		LOG(ERROR) << "INVALID KinectJointType!!!";
		return NUI_SKELETON_POSITION_WRIST_LEFT;
		break;
	}
}

bool KinectV1Handler::initKinect()
{
	/*//Get a working Kinect Sensor
	int numSensors = 0;
	if (NuiGetSensorCount(&numSensors) < 0 || numSensors < 1)
	{
		LOG(ERROR) << "No Kinect Sensors found!";
		return false;
	}
	if (NuiCreateSensorByIndex(0, &kinectSensor) < 0)
	{
		LOG(ERROR) << "Sensor found, but could not create an instance of it!";
		return false;
	}
	//Initialise Sensor
	HRESULT hr = kinectSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX
		| NUI_INITIALIZE_FLAG_USES_SKELETON);
	LOG_IF(FAILED(hr), ERROR) << "Kinect sensor failed to initialise!";
	else
		LOG(INFO) << "Kinect sensor opened successfully.";*/
	/*
	kinectSensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,               //Depth Camera or RGB Camera?
		NUI_IMAGE_RESOLUTION_640x480,       //Image Resolution
		0,                                  //Image stream flags, e.g. near mode
		2,                                  //Number of frames to buffer
		NULL,                               //Event handle
		&kinectRGBStream);

	*/
	/*kinectSensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, //Depth Camera or RGB Camera?
		NUI_IMAGE_RESOLUTION_320x240, //Image Resolution
		0, //Image stream flags, e.g. near mode
		2, //Number of frames to buffer
		nullptr, //Event handle
		&kinectDepthStream);
	kinectSensor->NuiSkeletonTrackingEnable(
		nullptr,
		NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE
	);*/

	bool netErr = initNetworking() == 0;

	return netErr;
}

void KinectV1Handler::getKinectRGBData()
{
	return; // The data from this function is not used, the only use I can think of is that it might be necessary to track properly, so it is *probably* unnecessary on the server
	/*NUI_IMAGE_FRAME imageFrame{};
	NUI_LOCKED_RECT LockedRect{};
	if (acquireKinectFrame(imageFrame, kinectRGBStream, kinectSensor))
	{
		return;
	}
	INuiFrameTexture* texture = lockKinectPixelData(imageFrame, LockedRect);
	copyKinectPixelData(LockedRect, kinectImageData.get());
	unlockKinectPixelData(texture);

	releaseKinectFrame(imageFrame, kinectRGBStream, kinectSensor);*/
}

bool KinectV1Handler::acquireKinectFrame(NUI_IMAGE_FRAME& imageFrame, HANDLE& rgbStream, INuiSensor*& sensor)
{
	return (sensor->NuiImageStreamGetNextFrame(rgbStream, 1, &imageFrame) < 0);
}

INuiFrameTexture* KinectV1Handler::lockKinectPixelData(NUI_IMAGE_FRAME& imageFrame, NUI_LOCKED_RECT& LockedRect)
{
	INuiFrameTexture* texture = imageFrame.pFrameTexture;
	texture->LockRect(0, &LockedRect, nullptr, 0);
	return imageFrame.pFrameTexture;
}

void KinectV1Handler::copyKinectPixelData(NUI_LOCKED_RECT& LockedRect, GLubyte* dest)
{
	int bytesInFrameRow = LockedRect.Pitch;
	if (bytesInFrameRow != 0)
	{
		const BYTE* curr = static_cast<const BYTE*>(LockedRect.pBits);
		const BYTE* dataEnd = curr + (KinectSettings::kinectWidth * KinectSettings::kinectHeight) * 4;

		while (curr < dataEnd)
		{
			*dest++ = *curr++;
		}
	}
}

void KinectV1Handler::unlockKinectPixelData(INuiFrameTexture* texture)
{
	texture->UnlockRect(0);
}

void KinectV1Handler::releaseKinectFrame(NUI_IMAGE_FRAME& imageFrame, HANDLE& rgbStream, INuiSensor*& sensor)
{
	sensor->NuiImageStreamReleaseFrame(rgbStream, &imageFrame);
}

static bool flip = false;

int KinectV1Handler::initNetworking()
{
	LOG(INFO) << "Initializing Networking" << std::endl;
	if (*networking.error_state == 0)
	{
		LOG(INFO) << "Searching for client" << std::endl;
		networking.waitForClient();
		return 0;
	}
	else
	{
		LOG(ERROR) << "Failed To Init Networking" << std::endl;
		return -1;
	}
}

HRESULT KinectV1Handler::netGetNextFrame(NUI_SKELETON_FRAME *frame)
{
	*frame = networking.getLatestSkeleton(*frame);//wait for next frame and yoink it
	return S_OK;
}

void KinectV1Handler::updateSkeletalData()
{
	//if (kinectSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame) >= 0)
	//if (netGetNextFrame(&skeletonFrame) >= 0)
	netGetNextFrame(&skeletonFrame);
	if(true)
	{
		NUI_TRANSFORM_SMOOTH_PARAMETERS params;

		params.fCorrection = .25f;
		params.fJitterRadius = .4f;
		params.fMaxDeviationRadius = .25f;
		params.fPrediction = .25f;
		params.fSmoothing = .25f;

		/*
		params.fSmoothing = .25f;
		params.fCorrection = .25f;
		params.fMaxDeviationRadius = .05f;
		params.fJitterRadius = 0.03f;
		params.fPrediction = .25f;
		*/
		//kinectSensor->NuiTransformSmooth(&skeletonFrame, &params); //Smooths jittery tracking
		NUI_SKELETON_DATA data;
		for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
		{
			NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;
			data = skeletonFrame.SkeletonData[i];

			if (NUI_SKELETON_TRACKED == trackingState)
			{
				for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
				{
					jointPositions[j] = skeletonFrame.SkeletonData[i].SkeletonPositions[j];
				}
				printf("\n");
				NuiSkeletonCalculateBoneOrientations(&skeletonFrame.SkeletonData[i], boneOrientations);
				rotFilter.update(boneOrientations);
				break;
			}
		}

		KinectSettings::head_position = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::Head)].x,
			jointPositions[convertJoint(KVR::KinectJointType::Head)].y,
			jointPositions[convertJoint(KVR::KinectJointType::Head)].z
		);
		KinectSettings::left_hand_pose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].x,
			jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].y,
			jointPositions[convertJoint(KVR::KinectJointType::HandLeft)].z
		);
		KinectSettings::mHandPose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::HandRight)].x,
			jointPositions[convertJoint(KVR::KinectJointType::HandRight)].y,
			jointPositions[convertJoint(KVR::KinectJointType::HandRight)].z
		);
		KinectSettings::hElPose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].x,
			jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].y,
			jointPositions[convertJoint(KVR::KinectJointType::ElbowLeft)].z
		);
		KinectSettings::mElPose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].x,
			jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].y,
			jointPositions[convertJoint(KVR::KinectJointType::ElbowRight)].z
		);
		KinectSettings::left_foot_raw_pose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z
		);
		KinectSettings::right_foot_raw_pose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z
		);
		KinectSettings::waist_raw_pose = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].x,
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].y,
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].z
		);
		KinectSettings::left_foot_raw_ori = glm::quat(
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.w,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.x,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.y,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion.z);
		KinectSettings::right_foot_raw_ori = glm::quat(
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.w,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.x,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.y,
			boneOrientations[convertJoint(KVR::KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion.z);
		KinectSettings::waist_raw_ori = glm::quat(
			boneOrientations[convertJoint(KVR::KinectJointType::SpineBase)].absoluteRotation.rotationQuaternion.w,
			boneOrientations[convertJoint(KVR::KinectJointType::SpineBase)].absoluteRotation.rotationQuaternion.x,
			boneOrientations[convertJoint(KVR::KinectJointType::SpineBase)].absoluteRotation.rotationQuaternion.y,
			boneOrientations[convertJoint(KVR::KinectJointType::SpineBase)].absoluteRotation.rotationQuaternion.z);

		KinectSettings::lastPose[0][0] = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z
		);
		KinectSettings::lastPose[1][0] = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y,
			jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z
		);
		KinectSettings::lastPose[2][0] = glm::vec3(
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].x,
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].y,
			jointPositions[convertJoint(KVR::KinectJointType::SpineBase)].z
		);

		/***********************************************************************************************/
		glm::quat hFootRotF, mFootRotF;

		//calculate direction vectors
		glm::vec3 up(0, 1, 0),
		          ankleLeftPose(
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleLeft)].z),

		          ankleRightPose(
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::AnkleRight)].z),

		          footLeftPose(
			          jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::FootLeft)].z),

		          footRightPose(
			          jointPositions[convertJoint(KVR::KinectJointType::FootRight)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::FootRight)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::FootRight)].z),

		          kneeLeftPose(
			          jointPositions[convertJoint(KVR::KinectJointType::KneeLeft)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::KneeLeft)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::KneeLeft)].z),

		          kneeRightPose(
			          jointPositions[convertJoint(KVR::KinectJointType::KneeRight)].x,
			          jointPositions[convertJoint(KVR::KinectJointType::KneeRight)].y,
			          jointPositions[convertJoint(KVR::KinectJointType::KneeRight)].z);

		glm::vec3 feetRot[2] = {
			eulerAngles(glm::quat(lookAt(footLeftPose, ankleLeftPose, up))),
			eulerAngles(glm::quat(lookAt(footRightPose, ankleRightPose, up)))
		};

		glm::vec3 tibiaRotZ[2] = {
			eulerAngles(glm::quat(lookAt(
				glm::vec3(ankleLeftPose.x, ankleLeftPose.y, 1),
				glm::vec3(kneeLeftPose.x, kneeLeftPose.y, 0), up))),
			eulerAngles(glm::quat(lookAt(
				glm::vec3(ankleRightPose.x, ankleRightPose.y, 1),
				glm::vec3(kneeRightPose.x, kneeRightPose.y, 0), up)))
		};

		glm::vec3 tibiaRotX[2] = {
			eulerAngles(glm::quat(lookAt(
				glm::vec3(1, ankleLeftPose.y, ankleLeftPose.z),
				glm::vec3(0, kneeLeftPose.y, kneeLeftPose.z), up))),
			eulerAngles(glm::quat(lookAt(
				glm::vec3(1, ankleRightPose.y, ankleRightPose.z),
				glm::vec3(0, kneeRightPose.y, kneeRightPose.z), up)))
		};

		hFootRotF = glm::vec3(
			-tibiaRotX[0].x - M_PI / 3,
			-feetRot[0].y + /*2 */ KinectSettings::calibration_trackers_yaw / 180 * M_PI,
			-tibiaRotZ[0].z * 15 + M_PI);

		mFootRotF = glm::vec3(
			-tibiaRotX[1].x - M_PI / 3,
			-feetRot[1].y + /*2 */ KinectSettings::calibration_trackers_yaw / 180 * M_PI,
			-tibiaRotZ[1].z * 15 + M_PI);

		normalize(hFootRotF);
		normalize(mFootRotF);
		
		////smooth with lowpass filter
		/*hFootRotF.w = lowPassFilter[0][0].update(hFootRotF.w);
		hFootRotF.x = lowPassFilter[0][1].update(hFootRotF.x);
		hFootRotF.y = lowPassFilter[0][2].update(hFootRotF.y);
		hFootRotF.z = lowPassFilter[0][3].update(hFootRotF.z);

		mFootRotF.w = lowPassFilter[1][0].update(mFootRotF.w);
		mFootRotF.x = lowPassFilter[1][1].update(mFootRotF.x);
		mFootRotF.y = lowPassFilter[1][2].update(mFootRotF.y);
		mFootRotF.z = lowPassFilter[1][3].update(mFootRotF.z);*/
		
		KinectSettings::trackerSoftRot[0] = hFootRotF;
		KinectSettings::trackerSoftRot[1] = mFootRotF;

		/***********************************************************************************************/

		//DEBUG
		/*
		for (int i = 0; i < 20; i++) {
			Vector4 orientation = boneOrientations[i].absoluteRotation.rotationQuaternion;
			std::cerr << "Joint " << i << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
		}*/
		//Vector4 orientation = boneOrientations[convertJoint(KinectJointType::AnkleLeft)].absoluteRotation.rotationQuaternion;
		//using namespace SFMLsettings;

		//debugDisplayTextStream << "AnkleLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
		//orientation = boneOrientations[convertJoint(KinectJointType::FootLeft)].absoluteRotation.rotationQuaternion;
		//debugDisplayTextStream << "FootLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
		//orientation = boneOrientations[convertJoint(KinectJointType::AnkleRight)].absoluteRotation.rotationQuaternion;
		//debugDisplayTextStream << "AnkleRightRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
		//orientation = boneOrientations[convertJoint(KinectJointType::FootRight)].absoluteRotation.rotationQuaternion;
		//debugDisplayTextStream << "FootLeftRot " << ": " << orientation.w << ", " << orientation.x << ", " << orientation.y << ", " << orientation.z << '\n';
	}
};

void KinectV1Handler::DrawSkeleton(const NUI_SKELETON_DATA& skel, sf::RenderWindow& window)
{
	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
	{
		screenSkelePoints[i] = SkeletonToScreen(jointPositions[i], SFMLsettings::m_window_width,
		                                        SFMLsettings::m_window_height);
		//std::cerr << "m_points[" << i << "] = " << screenSkelePoints[i].x << ", " << screenSkelePoints[i].y << '\n';
		// Same with the other cerr, without this, the skeleton flickers
	}
	// Render Torso
	window.clear();
	DrawBone(skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER, window);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE, window);
	DrawBone(skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER, window);
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, window);

	// Left Arm
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT, window);

	// Right Arm
	DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT, window);

	// Left Leg
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT, window);

	// Right Leg
	DrawBone(skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, window);
	DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT, window);

	// Draw the joints in a different color
	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
	{
		sf::CircleShape circle{};
		circle.setRadius(KinectSettings::g_JointThickness);
		circle.setPosition(screenSkelePoints[i]);

		if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED)
		{
			circle.setFillColor(sf::Color::Red);
			window.draw(circle);
		}
		else if (skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED)
		{
			circle.setFillColor(sf::Color::Yellow);
			window.draw(circle);
		}
	}
}

sf::Vector2f KinectV1Handler::SkeletonToScreen(Vector4 skeletonPoint, int _width, int _height)
{
	LONG x = 0, y = 0;
	USHORT depth = 0;

	// Calculate the skeleton's position on the screen
	// NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
	NuiTransformSkeletonToDepthImage(skeletonPoint, &x, &y, &depth);

	float screenPointX = x * _width / 320.f;
	float screenPointY = y * _height / 240.f;
	//std::cerr << "x = " << x << " ScreenX = " << screenPointX << " y = " << y << " ScreenY = " << screenPointY << '\n';

	// The skeleton constantly flickers and drops out without the cerr command...
	return sf::Vector2f(screenPointX, screenPointY);
}

void KinectV1Handler::DrawBone(const NUI_SKELETON_DATA& skel, NUI_SKELETON_POSITION_INDEX joint0,
                               NUI_SKELETON_POSITION_INDEX joint1, sf::RenderWindow& window)
{
	NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[joint0];
	NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[joint1];

	// If we can't find either of these joints, exit
	if (joint0State == NUI_SKELETON_POSITION_NOT_TRACKED || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
	{
		return;
	}

	// Don't draw if both points are inferred
	if (joint0State == NUI_SKELETON_POSITION_INFERRED && joint1State == NUI_SKELETON_POSITION_INFERRED)
	{
		return;
	}
	// Assume all bones are inferred unless BOTH joints are tracked
	if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
	{
		DrawLine(screenSkelePoints[joint0], screenSkelePoints[joint1], sf::Color::Green,
		         KinectSettings::g_TrackedBoneThickness, window);
	}
	else
	{
		DrawLine(screenSkelePoints[joint0], screenSkelePoints[joint1], sf::Color::Red,
		         KinectSettings::g_InferredBoneThickness, window);
	}
}

void KinectV1Handler::DrawLine(sf::Vector2f start, sf::Vector2f end, sf::Color colour, float lineThickness,
                               sf::RenderWindow& window)
{
	sfLine line(start, end);
	line.setColor(colour);
	line.setThickness(lineThickness);
	window.draw(line);
	//std::cerr << "Line drawn at: " << start.x << ", " << start.y << " to " << end.x << ", " << end.y << "\n";
}

Vector4 KinectV1Handler::zeroKinectPosition(int trackedSkeletonIndex)
{
	return jointPositions[NUI_SKELETON_POSITION_HEAD];
}

void KinectV1Handler::setKinectToVRMultiplier(int skeletonIndex)
{
	/*
	KinectSettings::kinectToVRScale = KinectSettings::hmdZero.v[1]
		/ (jointPositions[NUI_SKELETON_POSITION_HEAD].y
			+
			-jointPositions[NUI_SKELETON_POSITION_FOOT_LEFT].y);
	//std::cerr << "HMD zero: " << KinectSettings::hmdZero.v[1] << '\n';
	std::cerr << "head pos: " << jointPositions[NUI_SKELETON_POSITION_HEAD].y << '\n';
	std::cerr << "foot pos: " << jointPositions[NUI_SKELETON_POSITION_FOOT_LEFT].y << '\n';
	*/
}

bool KinectV1Handler::jointsUntracked(KVR::KinectJoint joint0, KVR::KinectJoint joint1, NUI_SKELETON_DATA data)
{
	NUI_SKELETON_POSITION_TRACKING_STATE joint0State = data.eSkeletonPositionTrackingState[convertJoint(joint0)];
	NUI_SKELETON_POSITION_TRACKING_STATE joint1State = data.eSkeletonPositionTrackingState[convertJoint(joint1)];

	// If we can't find either of these joints, exit
	return ((joint0State == NUI_SKELETON_POSITION_NOT_TRACKED
			|| joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
		&& KinectSettings::ignoreInferredPositions);
}

bool KinectV1Handler::jointsInferred(KVR::KinectJoint joint0, KVR::KinectJoint joint1, NUI_SKELETON_DATA data)
{
	NUI_SKELETON_POSITION_TRACKING_STATE joint0State = data.eSkeletonPositionTrackingState[convertJoint(joint0)];
	NUI_SKELETON_POSITION_TRACKING_STATE joint1State = data.eSkeletonPositionTrackingState[convertJoint(joint1)];

	// If we can't find either of these joints, exit
	return (joint0State == NUI_SKELETON_POSITION_INFERRED
		&& joint1State == NUI_SKELETON_POSITION_INFERRED
		&& KinectSettings::ignoreInferredPositions);
}
