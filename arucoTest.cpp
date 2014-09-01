/*
*
* HowTo: cd ..\..\opencvProjects\arucoTest1\build\Debug
* arucoTest live c:\aruco-1.2.5\build\testdata\highCalib.yml 0.1
*/




#include "server.h"
#include <fstream>
#include <sstream>
//OpenGL and freeglut
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include "windows.h"
#include <GL/gl.h>
#include <GL/glut.h>
#endif
//OpenCV
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//aruco
#include <aruco/aruco.h>
//sockets
//#include <WinSock2.h>
//#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace cv;
using namespace aruco;

/* Variables to control the spped of rotation/translation/zoom */
#define DEGREES_PER_PIXEL	0.6f
#define UNITS_PER_PIXEL		0.1f
#define ZOOM_FACTOR		0.04f

/* Enumeration of body parts */
enum{TORSO=0, LUA, LLA, RUA, RLA, LUL, LLL, RUL, RLL, DANCE, QUIT};

/* Variables to control the size of the robot */
#define HEAD_HEIGHT 3.0
#define HEAD_RADIUS 1.0

#define UPPER_ARM_HEIGHT 0.1
#define LOWER_ARM_HEIGHT 2.0
#define UPPER_ARM_RADIUS  0.5
#define LOWER_ARM_RADIUS  0.5

/* Structure to define the state of the mouse */
typedef struct
{
	bool leftButton;
	bool rightButton;
	int x;
	int y;
} MouseState;

MouseState mouseState = {false, false, 0, 0};

/* Scene rotation angles to alter interactively */
float xRotate = 0, yRotate = 0;

/* Camera position and orientation -- used by gluLookAt */
GLfloat eye[] = {0, 0, 20};
GLfloat at[] = {0, 0, 0}; 

GLUquadricObj	*t, *h,	/* torso and head */	
				*lua, *lla, *rua, *rla, /* arms */
				*lul, *lll, *rul, *rll;	/* legs */
/*
 * lua - left upper arm
 * lla - left lower arm
 * rua - right upper arm
 * rla - right lower arm
 * lul - left upper leg
 * lll - left lower leg
 * rul - right upper leg
 * rll - right lower leg
*/

/* initial joint angles */
static GLfloat theta[QUIT] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0}; 

/* torso position */
static GLfloat center[2] = {0,0};
/* joint angle to alter interactively */
static GLint angle = 0; /* initially, TORSO */
/* Dance mode or not? */
bool dance = false;
bool isPulse= false;


///opencv & aruco
string TheInputVideo;
string TheIntrinsicFile;
bool The3DInfoAvailable=false;
float TheMarkerSize=-1;
MarkerDetector PPDetector;
VideoCapture TheVideoCapturer;
vector<Marker> TheMarkers;
Mat TheInputImage,TheUndInputImage,TheResizedImage,FlippedImage;
CameraParameters TheCameraParams;
Size TheGlWindowSize;
bool TheCaptureFlag=true;
bool readIntrinsicFile(string TheIntrinsicFile,Mat & TheIntriscCameraMatrix,Mat &TheDistorsionCameraParams,Size size);


//methods
void vDrawScene();
void vIdle();
void vResize( GLsizei iWidth, GLsizei iHeight );
void Keyboard(unsigned char key, int x, int y);
void vMouse(int b,int s,int x,int y);
void vPulse(int d);
void DrawRobot( float x, float y, 
                float lua, float lla, float rua, float rla, 
                float lul, float lll, float rul, float rll );
void InitQuadrics();
void codo();
void brazo();
void antebrazo();


/************************************
 *
 *
 *
 *
 ************************************/
bool readArguments ( int argc,char **argv )
{
    if (argc!=4) {
        cerr<<"Invalid number of arguments"<<endl;
        cerr<<"Usage: (in.avi|live)  intrinsics.yml   size "<<endl;
        return false;
    }
    TheInputVideo=argv[1];
    TheIntrinsicFile=argv[2];
    TheMarkerSize=atof(argv[3]);
    return true;
}


/************************************
 *
 *
 *
 *
 ************************************/



int main(int argc,char **argv)
{

	/* start communication */
		
	if(initServer()==-1){
		fprintf(stderr, "Error creating server\n");
	}
		int xThread;
		pthread_t idHilo_EMF;
		
		/* create a second thread which executes inc_x(&x) */
		if(pthread_create(&idHilo_EMF,NULL,listenEMF,&xThread)) {
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}else{
			printf( "thread correctly created\n");
		}
    try
    {//parse arguments
        if (readArguments (argc,argv)==false) return 0;
        //read from camera
        if (TheInputVideo=="live") TheVideoCapturer.open(0);
        else TheVideoCapturer.open(TheInputVideo);
        if (!TheVideoCapturer.isOpened())
        {
            cerr<<"Could not open video"<<endl;
            return -1;

        }

        //read first image
        TheVideoCapturer>>TheInputImage;
        //read camera paramters if passed
        TheCameraParams.readFromXMLFile(TheIntrinsicFile);
        TheCameraParams.resize(TheInputImage.size());

        glutInit(&argc, argv);
        glutInitWindowPosition( 0, 0);
        glutInitWindowSize(TheInputImage.size().width,TheInputImage.size().height);
        glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );


        glutCreateWindow( "BIOS - Augmented Limbs" );
		/* register callback functions */
        glutDisplayFunc( vDrawScene ); //called to redraw scene
        glutIdleFunc( vIdle ); //camera frame to scene
        glutReshapeFunc( vResize );
		glutKeyboardFunc(Keyboard); //kb input
        glutMouseFunc(vMouse);
		glutTimerFunc(800,vPulse,true);

        glClearColor( 0.0, 0.0, 0.0, 1.0 );
        glClearDepth( 1.0 );
        TheGlWindowSize=TheInputImage.size();
        vResize(TheGlWindowSize.width,TheGlWindowSize.height);

		/* start event processing */
        glutMainLoop();

    } catch (std::exception &ex)

    {
        cout<<"Exception :"<<ex.what()<<endl;
    }

}
/************************************
 *
 ************************************/
void vMouse(int b,int s,int x,int y)
{
    if (b==GLUT_LEFT_BUTTON && s==GLUT_DOWN) {
        TheCaptureFlag=!TheCaptureFlag;
    }

}

/*
 * An idle event is generated when no other event occurs.
 * Robot dances during idle times.
 */
void vPulse(int d) {
  /* insert code here to make robot dance */
	if(d==1){
		isPulse=true;
		glutTimerFunc(200, vPulse,2);
		
	}else
	{
		isPulse=false;
		glutTimerFunc(800, vPulse,1);
	}
  glutPostRedisplay();
}

/*
 * A keyboard event occurs when the user presses a key:
 * '+' should cause joint angles to increase by 5 degrees
 * (within reasonable bounds)
 * '-' should cause joint angles to decrease by 5 degrees
 */
void Keyboard(unsigned char key, int x, int y) {
  switch (key) {
    case 'q':
		theta[LUA] += 5;
		break;
    case 'w':
      theta[LUA] -= 5;
      break;
    case 'e':
		if ( theta[LLA]<=110){
			theta[LLA] += 5;
		}
      break;
    case 'r':
		if ( theta[LLA]>=0){
			theta[LLA] -= 5;
		}
      break;
    case 'p':
      exit(0);
  }
  glutPostRedisplay();
}

/************************************
 *
 ************************************/
void axis(float size)
{
	//x=red hacia abajo
    glColor3f (1,0,0 );
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); // origin of the line
    glVertex3f(size,0.0f, 0.0f); // ending point of the line
    glEnd( );

	//y=green hacia derecha
    glColor3f ( 0,1,0 );
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); // origin of the line
    glVertex3f( 0.0f,size, 0.0f); // ending point of the line
    glEnd( );

	//z=blue hacia mi
    glColor3f (0,0,1 );
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f); // origin of the line
    glVertex3f(0.0f, 0.0f, size); // ending point of the line
    glEnd( );


}
/************************************
 *
 ************************************/
void vDrawScene()
{
    if (TheResizedImage.rows==0) //prevent from going on until the image is initialized
        return;
    ///clear the display
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    ///draw image in the buffer
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, TheGlWindowSize.width, 0, TheGlWindowSize.height, -1.0, 1.0);
    glViewport(0, 0, TheGlWindowSize.width , TheGlWindowSize.height);
    glDisable(GL_TEXTURE_2D);
    glPixelZoom( 1, -1);
    glRasterPos3f( 0, TheGlWindowSize.height  - 0.5, -1.0 );
    glDrawPixels ( TheGlWindowSize.width , TheGlWindowSize.height , GL_RGB , GL_UNSIGNED_BYTE , TheResizedImage.ptr(0) );
    ///Set the appropriate projection matrix so that rendering is done in a enrvironment
    //like the real camera (without distorsion)
    glMatrixMode(GL_PROJECTION);
    double proj_matrix[16];
    TheCameraParams.glGetProjectionMatrix(TheInputImage.size(),TheGlWindowSize,proj_matrix,0.05,10);
    glLoadIdentity();
    glLoadMatrixd(proj_matrix);

	//articulated arm allocation
	InitQuadrics();

    //now, for each marker,
    double modelview_matrix[16];
    for (unsigned int m=0;m<TheMarkers.size();m++)
    {
        TheMarkers[m].glGetModelViewMatrix(modelview_matrix);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glLoadMatrixd(modelview_matrix);


        axis(TheMarkerSize);
		if (isPulse){
			glColor3f(1.0,0.0,0.0);
		}else{
			glColor3f(0.70,0.53,0.43);
		}
		
        //glColor3f(0.86,0.78,0.7);
		glTranslatef(0, -UPPER_ARM_HEIGHT, 0);

		DrawRobot( 0, 0, 
	       theta[LUA], theta[LLA], theta[RUA], theta[RLA], 
               theta[LUL], theta[LLL], theta[RUL], theta[RLL] );
    }

    glutSwapBuffers();

}

/************************************
 *
 ************************************/
void vIdle()
{
    if (TheCaptureFlag) {
        //capture image
        TheVideoCapturer.grab();
        TheVideoCapturer.retrieve( TheInputImage);
		FlippedImage.create(TheInputImage.size(),CV_8UC3);
		TheUndInputImage.create(TheInputImage.size(),CV_8UC3);
        
		
		//transform color that by default is BGR to RGB because windows systems do not allow reading BGR images with opengl properly
        cv::cvtColor(TheInputImage,TheInputImage,CV_BGR2RGB);
		//flip image to get mirror effect
		cv::flip(TheInputImage, FlippedImage, 1);
        //remove distorion in image
        cv::undistort(FlippedImage,TheUndInputImage, TheCameraParams.CameraMatrix, TheCameraParams.Distorsion);
        //detect markers
        PPDetector.detect(TheUndInputImage,TheMarkers, TheCameraParams.CameraMatrix,Mat(),TheMarkerSize,false);
        //resize the image to the size of the GL window
        cv::resize(TheUndInputImage,TheResizedImage,TheGlWindowSize);
    }
    glutPostRedisplay();
}

/************************************
 *
 ************************************/
void vResize( GLsizei iWidth, GLsizei iHeight )
{
    TheGlWindowSize=Size(iWidth,iHeight);
    //not all sizes are allowed. OpenCv images have padding at the end of each line in these that are not aligned to 4 bytes
    if (iWidth*3%4!=0) {
        iWidth+=iWidth*3%4;//resize to avoid padding
        vResize(iWidth,TheGlWindowSize.height);
    }
    else {
        //resize the image to the size of the GL window
        if (TheUndInputImage.rows!=0)
            cv::resize(TheUndInputImage,TheResizedImage,TheGlWindowSize);
    }
}

/* Allocate quadrics with filled drawing style */
void InitQuadrics()
{
    lua = gluNewQuadric();
	gluQuadricDrawStyle(lua, GLU_LINE);
	h = gluNewQuadric();
	gluQuadricDrawStyle(h, GLU_LINE);
	lla = gluNewQuadric();
	gluQuadricDrawStyle(lla, GLU_LINE);
	/* Insert code here */
}

void DrawRobot( float x, float y, 
                float lua, float lla, float rua, float rla, 
                float lul, float lll, float rul, float rll )
{

	brazo();
	glPushMatrix();
    glTranslatef(0, 2*UPPER_ARM_HEIGHT+UPPER_ARM_HEIGHT/4, 0);
    codo();
	glTranslatef(0, UPPER_ARM_HEIGHT/4, 0);
	glRotatef(lla, 1, 0, 0);
	antebrazo();
	glPopMatrix();
}

void brazo()
{
   	glPushMatrix();
   	glRotatef(-90.0, 1.0, 0.0, 0.0);
	gluCylinder(lua,UPPER_ARM_HEIGHT/2, UPPER_ARM_HEIGHT/2, UPPER_ARM_HEIGHT*2,40,40);
   	glPopMatrix();
}

void codo() {
  glPushMatrix();
  //glScalef(TheMarkerSize/2, TheMarkerSize / 2, TheMarkerSize/2);
  gluSphere(h, UPPER_ARM_HEIGHT/2, 20, 20);
  glPopMatrix();
}

void antebrazo()
{
	glPushMatrix();
	glRotatef(-90, 1, 0, 0); //perpendicular a brazo
	//glRotatef(-0, 1, 0, 0);
	gluCylinder(lla, UPPER_ARM_HEIGHT/2, UPPER_ARM_HEIGHT/3, UPPER_ARM_HEIGHT*2, 40,
              40);
	glPopMatrix();
}