#ifndef __vr_h_
#define __vr_h_
 
#include "Ogre.h"
#include <OIS/OIS.h>

#include <aruco/aruco.h>  
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <opencv2/objdetect/objdetect.hpp>

#include "PulseDetector.h" 

#include <pthread.h>
#include <time.h>

#include "Server.h"
#include <fstream>
#include <sstream>


#define MAX_MARKERS 1 // maximun number of markers we assume they will be detected simultaneously     ,public OIS::KeyListener
 
class vr : public Ogre::FrameListener
{
public:
    vr(void);
    virtual ~vr(void);
    virtual int createScene(aruco::CameraParameters camParams, unsigned char* buffer, std::string resourcePath);
    /// Ogre general variables
	Ogre::Root* root;
	Ogre::RenderWindow* window;
	Ogre::Camera *camera;
	Ogre::SceneManager* smgr;
	OIS::InputManager* im;
	OIS::Keyboard* keyboard;
	
	/// communication
	Server *localServer;

	/// Ogre background variables
	Ogre::PixelBox mPixelBox;
	Ogre::TexturePtr mTexture;

	/// Ogre scene variables
	Ogre::SceneNode* detectedNode;
	Ogre::SceneNode* ogreNode[MAX_MARKERS];
	Ogre::AnimationState *baseAnim[MAX_MARKERS], *topAnim[MAX_MARKERS];
	Ogre::Entity* ogreEntity;			//mesh
	Ogre::SkeletonInstance* skel;		//rigs
	Ogre::Animation* anim;				//animation
	Ogre::MaterialPtr frontSkin;
	Ogre::MaterialPtr backSkin;
	Ogre::ColourValue origSI1Colour;	//color front texture
	Ogre::ColourValue origSI2Colour;	//color back texture
	Ogre::Timer _timer;					//timer
	
	Ogre::Vector3 mDirection;			
	Ogre::Real mRotate;
	Ogre::Bone *bones[17];		//17 bones in the armature
	Ogre::Quaternion initR[17];
	Ogre::Quaternion r;
		Ogre::Real ratio;
	int currState;
	string emf,oldState;
	bool busy;

	//heart monitoring
	int bpm;
protected:
	void createFrameListener(void);
	void setupArmature(void);
	/// arm movements
	void openHand(void);
	void closeHand(void);
	void flexArm(void);
	void flexHand(void);
	void extendHand(void);
	void pronation(void);
	void supination(void);
	void agreeHand(void);		//OK
	void pointerHand(void);
	void callHand(void);
	void relaxHand(void);
	   
    virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);
   
private:
    bool processUnbufferedInput(const Ogre::FrameEvent& evt);
};
 
#endif // #ifndef __vr_h_
