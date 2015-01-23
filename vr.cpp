#include "vr.h"
 
 
struct rotateWrapper {
    std::string axis;
    int angle;
    int bN;
}; 


//-------------------------------------------------------------------------------------
vr::vr(void)
{
	currState=12;
	
	localServer=new Server(8080,2,512);
	if(localServer->initServer()==-1){
		fprintf(stderr, "Error creating server\n");
	}
	int xThread;
	localServer->StartInternalThread();
}
//-------------------------------------------------------------------------------------
vr::~vr(void)
{
}
//-------------------------------------------------------------------------------------
int vr::createScene(aruco::CameraParameters camParams, unsigned char* buffer, std::string resourcePath="")
{
	/// INIT OGRE FUNCTIONS
    root = new Ogre::Root(resourcePath + "plugins.cfg", resourcePath + "ogre.cfg");
    if (!root->showConfigDialog()) return -1;
   
    smgr = root->createSceneManager(Ogre::ST_GENERIC);
    /// CREATE WINDOW, CAMERA AND VIEWPORT
    window = root->initialise(true);
    
    Ogre::SceneNode* cameraNode;
    camera = smgr->createCamera("camera");
    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(10.0f);
    camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
    camera->setPosition(0, 0, 0);
    camera->lookAt(0, 0, 1);
    //camera->setPolygonMode(Ogre::PM_WIREFRAME);
    double pMatrix[16];
    camParams.OgreGetProjectionMatrix(camParams.CamSize,camParams.CamSize, pMatrix, 0.05,10, false);
    Ogre::Matrix4 PM(pMatrix[0], pMatrix[1], pMatrix[2] , pMatrix[3],
    pMatrix[4], pMatrix[5], pMatrix[6] , pMatrix[7],
    pMatrix[8], pMatrix[9], pMatrix[10], pMatrix[11],
    pMatrix[12], pMatrix[13], pMatrix[14], pMatrix[15]);
    camera->setCustomProjectionMatrix(true, PM);
    camera->setCustomViewMatrix(true, Ogre::Matrix4::IDENTITY);
    window->addViewport(camera);
    cameraNode = smgr->getRootSceneNode()->createChildSceneNode("cameraNode");
    cameraNode->attachObject(camera);
    
    /// CREATE BACKGROUND FROM CAMERA IMAGE
    int width = camParams.CamSize.width;
    int height = camParams.CamSize.height;
    // create background camera image
    mPixelBox = Ogre::PixelBox(width, height, 1, Ogre::PF_R8G8B8, buffer);
    // Create Texture
    mTexture = Ogre::TextureManager::getSingleton().createManual("CameraTexture",Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
    Ogre::TEX_TYPE_2D,width,height,0,Ogre::PF_R8G8B8,Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    
    //Create Camera Material
    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().create("CameraMaterial", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    Ogre::Technique *technique = material->createTechnique();
    technique->createPass();
    material->getTechnique(0)->getPass(0)->setLightingEnabled(false);
    material->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    material->getTechnique(0)->getPass(0)->createTextureUnitState("CameraTexture");
    
    Ogre::Rectangle2D* rect = new Ogre::Rectangle2D(true);
    rect->setCorners(-1.0, 1.0, 1.0, -1.0);
    rect->setMaterial("CameraMaterial");
    
    // Render the background before everything else
    rect->setRenderQueueGroup(Ogre::RENDER_QUEUE_BACKGROUND);
    
    // Hacky, but we need to set the bounding box to something big, use infinite AAB to always stay visible
    Ogre::AxisAlignedBox aabInf;
    aabInf.setInfinite();
    rect->setBoundingBox(aabInf);
    
    
    
    // Attach background to the scene
    Ogre::SceneNode* node = smgr->getRootSceneNode()->createChildSceneNode("Background");
    node->attachObject(rect);
    /// CREATE SIMPLE OGRE SCENE
    // add sinbad.mesh
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation("male_body.zip", "Zip", "Popular");
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    for(int i=0; i<MAX_MARKERS; i++) {
        Ogre::String entityName = "Marker_" + Ogre::StringConverter::toString(i);
        ogreEntity = smgr->createEntity(entityName, "male_body.mesh");
        //ogreEntity->setDisplaySkeleton(true);
        setupArmature();
		/*
		skel = ogreEntity->getSkeleton();
		bones[0] = skel->getBone("mano.R");
		bones[0]->setManuallyControlled(true);
		
		int numAnimations = skel->getNumAnimations();
		for(int j=0;j<numAnimations;j++){
			anim = skel->getAnimation(j);
			anim->destroyNodeTrack(bone->getHandle());
		}*/
        Ogre::Real offset = ogreEntity->getBoundingBox().getHalfSize().y;
        ogreNode[i] = smgr->getRootSceneNode()->createChildSceneNode();
        // add entity to a child node to correct position (this way, entity axis is on feet of sinbad)
        Ogre::SceneNode *ogreNodeChild = ogreNode[i]->createChildSceneNode();
        ogreNodeChild->attachObject(ogreEntity);
        // Sinbad is placed along Y axis, we need to rotate to put it along Z axis so it stands up over the marker
        // first rotate along X axis, then add offset in Z dir so it is over the marker and not in the middle of it
        ogreNodeChild->rotate(Ogre::Vector3(0,0,1), Ogre::Radian(Ogre::Degree(90)));
        ogreNodeChild->translate(offset*(4/3),offset*(3/3),-offset*(6/3),Ogre::Node::TS_PARENT);
        //ogreNodeChild->translate(-offset*(9/3),0,-offset*(3/3),Ogre::Node::TS_PARENT);
        // mesh is too big, rescale!
        //const float scale = 0.06675f;
        const float scale = 0.1f;
        ogreNode[i]->setScale(scale, scale, 0.11f);
        //ogreNode[i]->setScale(scale, scale+0.02, scale);
    }
    createFrameListener();
    return 1;
}
//-------------------------------------------------------------------------------------
void vr::createFrameListener()
{
	/// KEYBOARD INPUT READING
    size_t windowHnd = 0;
    window->getCustomAttribute("WINDOW", &windowHnd);
    im = OIS::InputManager::createInputSystem(windowHnd);
    keyboard = static_cast<OIS::Keyboard*>(im->createInputObject(OIS::OISKeyboard, true));
    //keyboard->setEventCallback(this);
    root->addFrameListener(this);
}
//-------------------------------------------------------------------------------------
void vr::setupArmature()
{
	//grab materials
	frontSkin=Ogre::MaterialManager::getSingleton().getByName("front1");
	backSkin=Ogre::MaterialManager::getSingleton().getByName("back1");
	frontSkin->getBestTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA); 
	backSkin->getBestTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA); 
	//grab colours
	origSI1Colour = frontSkin->getBestTechnique(0)->getPass(0)->getSelfIllumination();
	origSI2Colour = backSkin->getBestTechnique(0)->getPass(0)->getSelfIllumination();
	 
	skel = ogreEntity->getSkeleton();
	bones[0] = skel->getBone("root");
	initR[0]=bones[0]->getOrientation();
	bones[0]->setManuallyControlled(true);
	
	bones[1] = skel->getBone("antebrazo.R");
	bones[1]->setManuallyControlled(true);
	Ogre::Quaternion start, rot, end;
	start=bones[1]->getOrientation();
	rot.FromAngleAxis(Ogre::Degree(110), Ogre::Vector3::UNIT_X);
	end=Ogre::Quaternion::Slerp(1.0,start,rot,true);
	bones[1]->setOrientation(end);
	initR[1]=bones[1]->getOrientation();

	bones[2] = skel->getBone("mano.R");
	initR[2]=bones[2]->getOrientation();
	bones[2]->setManuallyControlled(true);
	bones[3] = skel->getBone("polegar1.R");
	initR[3]=bones[3]->getOrientation();
	bones[3]->setManuallyControlled(true);
	bones[4] = skel->getBone("polegar2.R");
	initR[4]=bones[4]->getOrientation();
	bones[4]->setManuallyControlled(true);
	bones[5] = skel->getBone("indicador1.R");
	initR[5]=bones[5]->getOrientation();
	bones[5]->setManuallyControlled(true);
	bones[6] = skel->getBone("indicador2.R");
	initR[6]=bones[6]->getOrientation();
	bones[6]->setManuallyControlled(true);
	bones[7] = skel->getBone("indicador3.R");
	initR[7]=bones[7]->getOrientation();
	bones[7]->setManuallyControlled(true);
	bones[8] = skel->getBone("medio1.R");
	initR[8]=bones[8]->getOrientation();
	bones[8]->setManuallyControlled(true);
	bones[9] = skel->getBone("medio2.R");
	initR[9]=bones[9]->getOrientation();
	bones[9]->setManuallyControlled(true);
	bones[10] = skel->getBone("medio3.R");
	initR[10]=bones[10]->getOrientation();
	bones[10]->setManuallyControlled(true);
	bones[11] = skel->getBone("anelar1.R");
	initR[11]=bones[11]->getOrientation();
	bones[11]->setManuallyControlled(true);
	bones[12] = skel->getBone("anelar2.R");
	initR[12]=bones[12]->getOrientation();
	bones[12]->setManuallyControlled(true);
	bones[13] = skel->getBone("anelar3.R");
	initR[13]=bones[13]->getOrientation();
	bones[13]->setManuallyControlled(true);
	bones[14] = skel->getBone("minimo1.R");
	initR[14]=bones[14]->getOrientation();
	bones[14]->setManuallyControlled(true);
	bones[15] = skel->getBone("minimo2.R");
	initR[15]=bones[15]->getOrientation();
	bones[15]->setManuallyControlled(true);
	bones[16] = skel->getBone("minimo3.R");
	initR[16]=bones[16]->getOrientation();
	bones[16]->setManuallyControlled(true);
}
//-------------------------------------------------------------------------------------
void vr::flexArm()
{
	Ogre::Quaternion start, rot, end;
	start=bones[1]->getOrientation();
	cout<<"flexArm: "<<start.getPitch()<<endl;
	if (ratio<0.8) 
	{
		rot.FromAngleAxis(Ogre::Degree(125), Ogre::Vector3::UNIT_X);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[1]->setOrientation(end);
		ratio+=0.05;
		currState=1;
	}else
	{
		currState=12;//acabo movimiento
		ratio=0.01;
	}
	
}
//-------------------------------------------------------------------------------------
void vr::flexHand()
{
	Ogre::Quaternion start, rot, end;
	if (ratio<1) {
		cout<<"flexHand: "<<Ogre::Degree(bones[2]->getOrientation().getRoll())<<endl;
		rot.FromAngleAxis(Ogre::Degree(70), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[2]->setOrientation(end);

		ratio+=0.03;
		currState=2;
	}else
	{
		currState=12;//acabo movimiento
		//ratio=0.01;
	}
	//if (Ogre::Degree(bones[2]->getOrientation().getRoll())<Ogre::Degree(90)) bones[2]->roll(Ogre::Degree(10));
}
//-------------------------------------------------------------------------------------
void vr::extendHand()
{
	Ogre::Quaternion start, rot, end;
	if (ratio<1) {
		cout<<"extendHand: "<<Ogre::Degree(bones[2]->getOrientation().getRoll())<<endl;
		rot.FromAngleAxis(Ogre::Degree(-70), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[2]->setOrientation(end);

		ratio+=0.02;
		currState=3;
	}else
	{
		currState=12;//acabo movimiento
		//ratio=0.01;
	}
	
	//if (Ogre::Degree(bones[2]->getOrientation().getRoll())>Ogre::Degree(-90)) bones[2]->roll(Ogre::Degree(-10));
}
//-------------------------------------------------------------------------------------
void vr::pronation()
{
	cout<<"pronation: "<<Ogre::Degree(bones[1]->getOrientation().getYaw())<<endl;
	if (Ogre::Degree(bones[1]->getOrientation().getYaw())>Ogre::Degree(-55)) 
	bones[1]->yaw(Ogre::Degree(-10));
}
//-------------------------------------------------------------------------------------
void vr::supination()
{
	cout<<"supination: "<<Ogre::Degree(bones[1]->getOrientation().getYaw())<<endl;
	if (Ogre::Degree(bones[1]->getOrientation().getYaw())<Ogre::Degree(60)) bones[1]->yaw(Ogre::Degree(10));
	currState=6;
}
//-------------------------------------------------------------------------------------
void vr::agreeHand()
{
	Ogre::Quaternion start, rot, end;

	if (ratio<0.5) 
	{
		cout<<"agreeHand: "<<ratio<<endl;
		
		///pitch -> unit_Z for fingers.1
		start=bones[5]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(80), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[5]->setOrientation(end);
		bones[8]->setOrientation(end);
		bones[11]->setOrientation(end);
		bones[14]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[6]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[6]->setOrientation(end);
		bones[9]->setOrientation(end);
		bones[12]->setOrientation(end);
		bones[15]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[7]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[7]->setOrientation(end);
		bones[10]->setOrientation(end);
		bones[13]->setOrientation(end);
		bones[16]->setOrientation(end);
		
		ratio+=0.01;

		currState=9;
	}else
		{
			
			currState=12;//acabo movimiento
			//ratio=0.01;
		}
	
}
//-------------------------------------------------------------------------------------
void vr::closeHand()
{
	Ogre::Quaternion start, rot, end;

	if (ratio<0.5) 
	{
		cout<<"closeHand: "<<ratio<<endl;
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		
		///pitch -> unit_Z for fingers.1
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[3]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(45), Ogre::Vector3::UNIT_Z);
		//rot.FromAngleAxis(Ogre::Degree(-45), Ogre::Vector3::UNIT_Y);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[3]->setOrientation(end);
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[4]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		rot.FromAngleAxis(Ogre::Degree(-90), Ogre::Vector3::UNIT_Y);
		
		//rot.FromAngleAxis(Ogre::Degree(-45), Ogre::Vector3::UNIT_Y);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[4]->setOrientation(end);
		
		
		///pitch -> unit_Z for fingers.1
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[5]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(80), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[5]->setOrientation(end);
		bones[8]->setOrientation(end);
		bones[11]->setOrientation(end);
		bones[14]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[6]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[6]->setOrientation(end);
		bones[9]->setOrientation(end);
		bones[12]->setOrientation(end);
		bones[15]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[7]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[7]->setOrientation(end);
		bones[10]->setOrientation(end);
		bones[13]->setOrientation(end);
		bones[16]->setOrientation(end);
		
		ratio+=0.01;

		currState=8;
	}else
		{
			
			currState=12;//acabo movimiento
			//ratio=0.01;
		}
	
}
//-------------------------------------------------------------------------------------
void vr::pointerHand()
{
	Ogre::Quaternion start, rot, end;

	if (ratio<1.0) 
	{
		cout<<"pointHand: "<<ratio<<endl;
		///pitch -> unit_Z for fingers.1
		start=bones[5]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(80), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[8]->setOrientation(end);
		bones[11]->setOrientation(end);
		bones[14]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[6]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[9]->setOrientation(end);
		bones[12]->setOrientation(end);
		bones[15]->setOrientation(end);
		
		///roll -> unit_Z for fingers.3
		start=Ogre::Quaternion::IDENTITY;
		rot=Ogre::Quaternion::IDENTITY;
		start=bones[7]->getOrientation();
		rot.FromAngleAxis(Ogre::Degree(90), Ogre::Vector3::UNIT_Z);
		end=Ogre::Quaternion::Slerp(ratio,start,rot,true);
		bones[10]->setOrientation(end);
		bones[13]->setOrientation(end);
		bones[16]->setOrientation(end);
		ratio+=0.05;
		currState=10;
	}/*else
	{
		currState=12;//acabo movimiento
		ratio=0.01;
	}*/
}
//-------------------------------------------------------------------------------------
void vr::relaxHand()
{
	Ogre::Quaternion start, rot, end;
	/*
	bones[5]->setOrientation(initR[5]);
	bones[11]->setOrientation(initR[11]);
	bones[14]->setOrientation(initR[14]);
	*/
	
	if (ratio<0.5) 
	{
		cout<<"relaxHand: "<<ratio<<endl;
		///pitch -> unit_Z for fingers.1
		start=Ogre::Quaternion::IDENTITY;
		start=bones[1]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[1],true);
		bones[1]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[2]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[2],true);
		bones[2]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[3]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[3],true);
		bones[3]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[4]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[4],true);
		bones[4]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[5]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[5],true);
		bones[5]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[8]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[8],true);
		bones[8]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[11]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[11],true);
		bones[11]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[14]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[14],true);
		bones[14]->setOrientation(end);
		
		///roll -> unit_Z for fingers.2
		start=Ogre::Quaternion::IDENTITY;
		start=bones[6]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[6],true);
		bones[6]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[9]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[9],true);
		bones[9]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[12]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[12],true);
		bones[12]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[15]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[15],true);
		bones[15]->setOrientation(end);
		
		///roll -> unit_Z for fingers.3
		start=Ogre::Quaternion::IDENTITY;
		start=bones[7]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[7],true);
		bones[7]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[10]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[10],true);
		bones[10]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[13]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[13],true);
		bones[13]->setOrientation(end);
		
		start=Ogre::Quaternion::IDENTITY;
		start=bones[16]->getOrientation();
		end=Ogre::Quaternion::Slerp(ratio,start,initR[16],true);
		bones[16]->setOrientation(end);
		
		ratio+=0.01;
		currState=11;
	}else
		{
			currState=12;//acabo movimiento
			//ratio=0.01;
		}
		
		//currState=12;//acabo movimiento
			//ratio=0.01;
}
//-------------------------------------------------------------------------------------
void vr::callHand()
{
	cout<<"agreeHand: "<<Ogre::Degree(bones[5]->getOrientation().getRoll())<<endl;
	if (currState!=12) 
	{
		bones[5]->pitch(Ogre::Degree(80));
		bones[6]->roll(Ogre::Degree(90));
		bones[7]->roll(Ogre::Degree(80));
		bones[8]->pitch(Ogre::Degree(80));
		bones[9]->roll(Ogre::Degree(90));
		bones[10]->roll(Ogre::Degree(80));
		bones[11]->pitch(Ogre::Degree(80));
		bones[12]->roll(Ogre::Degree(90));
		bones[13]->roll(Ogre::Degree(80));
		currState=12;
	}
}
//-------------------------------------------------------------------------------------
bool vr::processUnbufferedInput(const Ogre::FrameEvent& evt)
{
	//Ogre::Real ratio = 1;
	//static Ogre::Real mRotate = 0.13;   // The rotate constant

	//cout<<"event capture at listener"<<endl;
	emf=localServer->getMessage();
	
	if(_timer.getMilliseconds() > 650 && _timer.getMilliseconds() < 720)
	{ 
        frontSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(Ogre::ColourValue(1,origSI1Colour.g,origSI1Colour.b,origSI1Colour.a));
		backSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(Ogre::ColourValue(1,origSI2Colour.g,origSI2Colour.b,origSI2Colour.a));
	}
	else if(_timer.getMilliseconds() > 720 && _timer.getMilliseconds() < 750)
	{
		frontSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(origSI1Colour);
		backSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(origSI2Colour);
	
	}
	else if(_timer.getMilliseconds() > 750 && _timer.getMilliseconds() < 800)
	{
		frontSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(Ogre::ColourValue(1,origSI1Colour.g,origSI1Colour.b,origSI1Colour.a));
		backSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(Ogre::ColourValue(1,origSI2Colour.g,origSI2Colour.b,origSI2Colour.a));
		
	}else if(_timer.getMilliseconds() > 800)
	{
		frontSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(origSI1Colour);
		backSkin->getBestTechnique(0)->getPass(0)->setSelfIllumination(origSI2Colour);
		_timer.reset();
	}
	
	cout<<"1. event capture at listener"<<emf.compare(oldState)<<" "<<emf<<oldState<<currState<<endl;
	//if(emf.compare(oldState)!=0 && currState==12)  //use this with communication enabled
	if(currState==12)
	{
		ratio=0.01;
		busy=true;
	}
	if (keyboard->isKeyDown(OIS::KC_ESCAPE)) return false;
	else if(keyboard->isKeyDown(OIS::KC_E) || currState==2 || emf=="FH")		/// flex hand
	{
		//emf="FH";
		flexHand();
	} else if(keyboard->isKeyDown(OIS::KC_R) || currState==3 || emf=="EH")		/// extend hand
	{
		//emf="EH";
		extendHand();
	} else if(keyboard->isKeyDown(OIS::KC_C) || currState==8 || emf=="CH")		/// close hand
	{
		//emf="CH";
		closeHand();
	} else if(keyboard->isKeyDown(OIS::KC_O) || currState==9)		/// ok hand
	{
		agreeHand();
	} else if(keyboard->isKeyDown(OIS::KC_P) || currState==10)		/// point hand
	{
		pointerHand();
	} else if(keyboard->isKeyDown(OIS::KC_L) ||  currState==11 || emf=="RH")		/// relax hand
	{
		//emf="NM";
		relaxHand();
	} 
	oldState=emf;
	
	cout<<"2. event capture at listener"<<emf.compare(oldState)<<";"<<emf<<";"<<oldState<<";"<<currState<<endl;
    return true;
}
//-------------------------------------------------------------------------------------
bool vr::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
    //bool ret = BaseApplication::frameRenderingQueued(evt);
	keyboard->capture();
    if(!processUnbufferedInput(evt)) return false;
 
    return true;
}
//-------------------------------------------------------------------------------------
