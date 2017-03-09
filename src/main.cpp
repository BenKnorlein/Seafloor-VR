
// OpenGL platform-specific headers
#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <gl/GLU.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#endif


// MinVR header
#include <api/MinVR.h>
#include "main/VREventInternal.h"
#include "main/VRGraphicsStateInternal.h"
#include "tinyxml2.h"
using namespace MinVR;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

// Just included for some simple Matrix math used below
// This is not required for use of MinVR in general
#include <math/VRMath.h>

struct pt {
	float vertex[3];
	float color[3];
};
std::vector <pt> points;

/** MyVRApp is a subclass of VRApp and overrides two key methods: 1. onVREvent(..)
    and 2. onVRRenderGraphics(..).  This is all that is needed to create a
    simple graphics-based VR application and run it on any display configured
    for use with MinVR.
 */
class MyVRApp : public VRApp {
public:
	MyVRApp(int argc, char** argv, const std::string& configFile) : VRApp(argc, argv, configFile), isInitialised(false){
		filename = argv[2];
		computeCenter();

    }

    virtual ~MyVRApp() {}

	bool startsWith(std::string string1, std::string string2)
	{
		if (strlen(string1.c_str()) < strlen(string2.c_str())) return false;

		return !strncmp(string1.c_str(), string2.c_str(), strlen(string2.c_str()));
	}
	
	template <typename T> int sgn(T val) {
		return (T(0) < val) - (val < T(0));
	}

	// Callback for event handling, inherited from VRApp
	virtual void onVREvent(const VREvent &event) {

		if (startsWith(event.getName(), "HTC_Controller_1"))
		{
			if(event.getInternal()->getDataIndex()->exists("/HTC_Controller_1/Pose")){
				controllerpose = event.getDataAsDoubleArray("Pose");
				controllerpose = controllerpose.transpose();
			}
			if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_1/State/Axis0Button_Pressed")&&
				(int) event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0Button_Pressed")){
				double x = event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0/XPos");
				double y = event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0/YPos");
				bool rotate = false;
				if (fabs(x) > fabs(y)) rotate = true;
				
				//if (!rotate)
				{
					VRVector3 offset = 0.1 * controllerpose * VRVector3(0, 0, y);
					VRMatrix4 trans = VRMatrix4::translation(offset);
					for (int i = 0; i < 16; i++)
					{
						std::cerr << roompose.m[i] << " ";
					}
					std::cerr << std::endl;
					roompose = trans * roompose;
				}
				//else
				{
					VRMatrix4 rot = VRMatrix4::rotationY(x / 10 / CV_PI);
					roompose = rot * roompose;
				}

			}
		}

		if (event.getName() == "KbdEsc_Down") {
            shutdown();
            return;
		}
        
	}

	// Callback for rendering, inherited from VRRenderHandler
	virtual void onVRRenderGraphicsContext(const VRGraphicsState& state) {
		createDisplayList();
	}

	// Callback for rendering, inherited from VRRenderHandler
    virtual void onVRRenderGraphics(const VRGraphicsState &state) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glClearDepth(1.0f);
		glClearColor(0.0, 0.0, 0.0, 1.f);
		glDisable(GL_LIGHTING);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(state.getProjectionMatrix());

		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(state.getViewMatrix());
	
			glPushMatrix();
			glMultMatrixd(&roompose.m[0]);
			glCallList(list);
		glPopMatrix();

		glPushMatrix();
			glMultMatrixd(&controllerpose.m[0]);
			glBegin(GL_LINES);                // Begin drawing the color cube with 6 quads
			// Back face (z = -1.0f)
			glColor3f(0.5f, 0.5f, 0.0f);     // Yellow
			glVertex3f(0.0f, 0.0f, -1.0f);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glEnd();  // End of drawing color-cube
		glPopMatrix();
	}

	void computeCenter()
	{
		double x = 0;
		double y = 0;
		double z = 0;

		for (int i = 0; i < points.size(); i++)
		{
			x += points[i].vertex[0];
			y += points[i].vertex[1];
			z += points[i].vertex[2];
		}

		x = x / points.size();
		y = y / points.size();
		z = z / points.size();

		roompose = VRMatrix4::translation(VRVector3(-x, -y, -z));
	}

	void drawPoints()
	{
		glColor3f(1, 1, 1);
		glBegin(GL_POINTS);
		for (int i = 0; i < points.size(); i+= 2)
		{
			//std::cerr << points[i].vertex[0] << " , " << points[i].vertex[1] << " , " << points[i].vertex[2] << std::endl;
			//glColor3f(points[i].color[0], points[i].color[1], points[i].color[2]);		
			glVertex3f(points[i].vertex[0], points[i].vertex[1], points[i].vertex[2]);
			
		}
		glEnd();
	}

	void createDisplayList()
	{
		if (!isInitialised)
		{
			glLineWidth(10.0);
			list = glGenLists(1);
			glNewList(list, GL_COMPILE);
			drawPoints();
			glEndList();
			isInitialised = true;
		}
	}

	

protected:
	
	std::string filename;
	bool isInitialised;
	unsigned int list;
	VRMatrix4 controllerpose;
	VRMatrix4 roompose;
};
void addPoint(float x, float y, float z)
{
	float scale = 1;
	pt p;
	p.vertex[0] = 300 * x / scale;
	p.vertex[1] = z / scale / 100;
	p.vertex[2] = 300 * y / scale;

	points.push_back(p);
}

inline void colormap_jet(
		double value,
		double min_val,
		double max_val,
		unsigned char &r,
		unsigned char &g,
		unsigned char &b
	)
{

	// scale the gray value into the range [0, 8]
	const double gray = 8 *  (value - min_val) / (max_val - min_val));
	// s is the slope of color change
	const double s = 1.0 / 2.0;

	if (gray <= 1)
	{
		r = 0;
		g = 0;
		b = static_cast<unsigned char>((gray + 1)*s * 255 + 0.5);
	}
	else if (gray <= 3)
	{
		r = 0;
		g = static_cast<unsigned char>((gray - 1)*s * 255 + 0.5);
		b = 255;
	}
	else if (gray <= 5)
	{
		r = static_cast<unsigned char>((gray - 3)*s * 255 + 0.5);
		g = 255;
		b = static_cast<unsigned char>((5 - gray)*s * 255 + 0.5);
	}
	else if (gray <= 7)
	{
		r = 255;
		g = static_cast<unsigned char>((7 - gray)*s * 255 + 0.5);
		b = 0;
	}
	else
	{
		r = static_cast<unsigned char>((9 - gray)*s * 255 + 0.5);
		g = 0;
		b = 0;
	}
}

std::istream& safeGetline(std::istream& is, std::string& t)
{
	t.clear();

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	std::istream::sentry se(is, true);
	std::streambuf* sb = is.rdbuf();

	for (;;)
	{
		int c = sb->sbumpc();
		switch (c)
		{
		case '\n':
			return is;
		case '\r':
			if (sb->sgetc() == '\n')
				sb->sbumpc();
			return is;
		case EOF:
			// Also handle the case when the last line has no line ending
			if (t.empty())
			{
				is.setstate(std::ios::eofbit);
			}
			return is;
		default:
			t += (char)c;
		}
	}
}

std::istream& comma(std::istream& in)
{
	if ((in >> std::ws).peek() != std::char_traits<char>::to_int_type(' '))
	{
		in.setstate(std::ios_base::failbit);
	}
	return in.ignore();
}
void loadDataSet(std::string file)
{
	std::ifstream fin(file);
	std::istringstream in;
	std::string line;
	std::string s;
	int count = 0;
	while (!safeGetline(fin, line).eof())
	{
		in.clear();
		in.str(line);
		std::vector<double> tmp;
		while (getline(in, s, ' ')) {
			tmp.push_back(std::atof(s.c_str()));
		}

		if (tmp.size() == 3)
		{
			addPoint(tmp[0], tmp[1], tmp[2]);
		}
		line.clear();
		tmp.clear();
		count++;
	}
	fin.close();
}

int main(int argc, char **argv) {
	loadDataSet(argv[2]);
    MyVRApp app(argc, argv, argv[1]);
  	app.run();
	exit(0);
}

