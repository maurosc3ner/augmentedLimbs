#include <iostream>
 
#include "vr.h"

/*
*
* HowTo: 
* WIN_32
* cd ..\..\opencvProjects\arucoTest1\build\Debug
* arucoTest live c:\aruco-1.2.5\build\testdata\highCalib.yml 0.1
* 
* UNIX
* cd /home/esteban/opencvProjects/arucoOgre2/buildLinux
* make
* ./aruco_test_ogre live ../../highCalib.yml 0.076
*/
 
#define MAX_MARKERS 1 // maximun number of markers we assume they will be detected simultaneously


/// ArUco variables
string TheInputVideo;
cv::VideoCapture TheVideoCapturer;
cv::Mat TheInputImage, FlippedImage, TheInputImageUnd;
aruco::CameraParameters CameraParams, CameraParamsUnd;
float TheMarkerSize=1;
aruco::MarkerDetector TheMarkerDetector;
std::vector<aruco::Marker> TheMarkers;


bool readParameters(int argc, char** argv);

void usage()
{
    cout<<" This program test Ogre version of ArUco (single marker version) \n\n";
    cout<<" Usage <video.avi>|live <camera.yml> <markersize>"<<endl;
    cout<<" <video.avi>|live: specifies a input video file. Use 'live' to capture from camera"<<endl;
    cout<<" <camera.yml>: camera calibration file"<<endl;
    cout<<" <markersize>: in meters "<<endl;
}

/**
*
*/
int main(int argc, char** argv)
{

    cout<<" INIT "<<endl;
    /// READ PARAMETERS
    if(!readParameters(argc, argv))
    return false;
	
	vr *app;
	app=new vr();
	
	///aruco
	//TheMarkerDetector.setDesiredSpeed(3);
	
	
	
	///Kalman Position
	cv::KalmanFilter kfPos(6, 3, 0);
	// intialization of KF...
	kfPos.transitionMatrix = *(cv::Mat_<float>(6, 6) << 1,0,0,1,0,0,  0,1,0,0,1,0,  0,0,1,0,0,1,  0,0,0,1,0,0,  0,0,0,0,1,0,  0,0,0,0,0,1);
	cv::Mat_<float> measurePos(3,1); measurePos.setTo(cv::Scalar(0));
	 cv::Mat estimatedPos;
	kfPos.statePre.at<float>(0) = 1;
	kfPos.statePre.at<float>(1) = 1;
	kfPos.statePre.at<float>(2) = 1;
	kfPos.statePre.at<float>(3) = 0;
	kfPos.statePre.at<float>(4) = 0;
	kfPos.statePre.at<float>(5) = 0;
	cv::setIdentity(kfPos.measurementMatrix);
	cv::setIdentity(kfPos.processNoiseCov, cv::Scalar::all(1e-4));
	cv::setIdentity(kfPos.measurementNoiseCov, cv::Scalar::all(5));
	cv::setIdentity(kfPos.errorCovPost, cv::Scalar::all(1));
	
	///Kalman Orientation
	cv::KalmanFilter kfOri(8, 4, 0);
	// intialization of KF...
	kfOri.transitionMatrix = *(cv::Mat_<float>(8, 8) << 1,0,0,0,1,0,0,0,  0,1,0,0,0,1,0,0,  0,0,1,0,0,0,1,0,  0,0,0,1,0,0,0,1,  0,0,0,0,1,0,0,0,  0,0,0,0,0,1,0,0,  0,0,0,0,0,0,1,0,  0,0,0,0,0,0,0,1);
	cv::Mat_<float> measureOri(4,1); measureOri.setTo(cv::Scalar(0));
	 cv::Mat estimatedOri;
	kfOri.statePre.at<float>(0) = 1;
	kfOri.statePre.at<float>(1) = 1;
	kfOri.statePre.at<float>(2) = 1;
	kfOri.statePre.at<float>(3) = 1;
	kfOri.statePre.at<float>(4) = 0;
	kfOri.statePre.at<float>(5) = 0;
	kfOri.statePre.at<float>(6) = 0;
	kfOri.statePre.at<float>(7) = 0;
	cv::setIdentity(kfOri.measurementMatrix);
	cv::setIdentity(kfOri.processNoiseCov, cv::Scalar::all(1e-4));
	cv::setIdentity(kfOri.measurementNoiseCov, cv::Scalar::all(5));
	cv::setIdentity(kfOri.errorCovPost, cv::Scalar::all(0.0001));
	
	
    /// CREATE UNDISTORTED CAMERA PARAMS
    CameraParamsUnd=CameraParams;
    CameraParamsUnd.Distorsion=cv::Mat::zeros(4,1,CV_32F);
    cout<<" CREATE UNDISTORTED CAMERA PARAMS "<<endl;
    
    /// CAPTURE FIRST FRAME
    TheVideoCapturer.grab();
    TheVideoCapturer.retrieve ( TheInputImage );
    FlippedImage.create(TheInputImage.size(),CV_8UC3);
    cv::flip(TheInputImage, FlippedImage, 1);
    cv::undistort(FlippedImage,TheInputImageUnd,CameraParams.CameraMatrix,CameraParams.Distorsion);
    cout<<" CAPTURE FIRST FRAME "<<endl;
    /// INIT OGRE
    
    app->createScene(CameraParamsUnd, TheInputImageUnd.ptr<uchar>(0),"");
    cout<<" INIT OGRE "<<endl;
    double position[3], orientation[4];
    while (TheVideoCapturer.grab())
    {
        /// READ AND UNDISTORT IMAGE
        TheVideoCapturer.retrieve ( TheInputImage );
        cv::flip(TheInputImage, FlippedImage, 1);
        cv::undistort(FlippedImage,TheInputImageUnd,CameraParams.CameraMatrix,CameraParams.Distorsion);
        
        ///Predict
        cv::Mat predictPos = kfPos.predict();
        cv::Mat predictOri = kfOri.predict();
        
        /// DETECT MARKERS
        TheMarkerDetector.detect(TheInputImageUnd, TheMarkers, CameraParamsUnd, TheMarkerSize);
        
        /// UPDATE BACKGROUND IMAGE
        app->mTexture->getBuffer()->blitFromMemory(app->mPixelBox);
        
        /// UPDATE SCENE
        unsigned int i,idx;
        
        // show nodes for detected markers
        
        if(TheMarkers.size()>0)
        {
			for(i=0; i<TheMarkers.size() && i<MAX_MARKERS; i++) {
				TheMarkers[i].OgreGetPoseParameters(position, orientation);
				measurePos(0)=position[0];
				measurePos(1)=position[1];
				measurePos(2)=position[2];
				estimatedPos = kfPos.correct(measurePos);
				
				measureOri(0)=orientation[0];
				measureOri(1)=orientation[1];
				measureOri(2)=orientation[2];
				measureOri(3)=orientation[3];
				estimatedOri = kfOri.correct(measureOri);
				
				app->ogreNode[i]->setPosition( position[0], position[1], position[2]  );
				//app->ogreNode[i]->setPosition( estimatedPos.at<float>(0), estimatedPos.at<float>(1), estimatedPos.at<float>(2));
				app->ogreNode[i]->setOrientation( estimatedOri.at<float>(0), estimatedOri.at<float>(1), estimatedOri.at<float>(2), estimatedOri.at<float>(3) );
				app->ogreNode[i]->setVisible(true);
			}
						
			// hide rest of nodes
			for( ; i<MAX_MARKERS; i++) app->ogreNode[i]->setVisible(false);
			
		}else
		{
			if ((predictPos.at<float>(0) > 0) && (predictPos.at<float>(1)>0), (predictPos.at<float>(2)>0))
			{
				cout<<" KALMAN USED************************************************ "<<endl;
				app->ogreNode[0]->setPosition( predictPos.at<float>(0), predictPos.at<float>(1), predictPos.at<float>(2) );
				app->ogreNode[0]->setOrientation( estimatedOri.at<float>(0), estimatedOri.at<float>(1), estimatedOri.at<float>(2), estimatedOri.at<float>(3) );
				
				//app->ogreNode[0]->setOrientation( orientation[0], orientation[1], orientation[2], orientation[3]  );
				app->ogreNode[0]->setVisible(true);
				
			}
			
		}

        /// RENDER FRAME
        if(app->root->renderOneFrame() == false) break;
        Ogre::WindowEventUtilities::messagePump();  
    }
    app->im->destroyInputObject(app->keyboard);
    app->im->destroyInputSystem(app->im);
    app->im = 0;
    delete app->root;
    return 0;
}


/**
*
*/
bool readParameters(int argc, char** argv)
{
    if (argc<3) {
        usage();
        return false;
    }
    TheInputVideo=argv[1];
    if (TheInputVideo=="live") TheVideoCapturer.open(0);
    else TheVideoCapturer.open(TheInputVideo);
    if (!TheVideoCapturer.isOpened())
    {
        cerr<<"Could not open video"<<endl;
        return -1;
    }
    cout<<" VIDEO OPENED "<<endl;
    // read intrinsic file
    try {
        CameraParams.readFromXMLFile(argv[2]);
        } catch (std::exception &ex) {
        cout<<ex.what()<<endl;
        return false;
    }
    cout<<" params OPENED "<<endl;
    if(argc>3) TheMarkerSize=atof(argv[3]);
    else TheMarkerSize=1.;
    return true;
}
