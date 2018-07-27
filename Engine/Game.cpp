#include "MainWindow.h"
#include "Game.h"

#include "Geometry.h"

using std::vector;

class ScreenTransformer
{
public:
	ScreenTransformer(void)
		: dx(Graphics::ScreenWidth / 2), dy(Graphics::ScreenHeight / 2)
	{}

	Vec3f & Transform(Vec3f & v) const
	{
		//const float zInv = 1.0f / v.z;
		// divide all position components and attributes by z
		// (we want to be interpolating our attributes in the
		//  same space where the x,y interpolation is taking
		//  place to prevent distortion)
		//v *= zInv;
		// adjust position x,y from perspective normalized space
		// to screen dimension space after perspective divide
		v.x = (v.x + 1.0f) * dx;
		v.y = (-v.y + 1.0f) * dy;
		// store 1/z in z (we will need the interpolated 1/z
		// so that we can recover the attributes after interp.)
		//v.z = zInv;

		return v;
	}

	Vec2i Transform(
		const Vec3f pWorld,
		const Matrix44f &worldToCamera,
		const float &canvasWidth,
		const float &canvasHeight,
		const uint32_t &imageWidth,
		const uint32_t &imageHeight
	) const
	{
		Vec2i pRaster;
		Vec3f pCamera;
		worldToCamera.multVecMatrix(pWorld, pCamera);
		Vec2f pScreen;
		pScreen.x = pCamera.x / -pCamera.z;
		pScreen.y = pCamera.y / -pCamera.z;
		Vec2f pNDC;
		pNDC.x = (pScreen.x + canvasWidth * 0.5) / canvasWidth;
		pNDC.y = (pScreen.y + canvasHeight * 0.5) / canvasHeight;
		pRaster.x = (int)(pNDC.x * imageWidth);
		pRaster.y = (int)((1 - pNDC.y) * imageHeight);
		return pRaster;
	}


private:
	double dx;
	double dy;
};

struct TIndexedLineList
{
	vector<Vec3f> points;
	vector<unsigned> indices;
};

class Camera
{
public:
	Camera(const Vec3f & p, const Vec3f look)
		: pos(p), looking_at(look)
	{}

	Vec3f pos;
	Vec3f looking_at;

	Matrix44f get_view(void) const
	{
		// the position of the camera, called 'eye'
		Vec3f c = pos;
		Vec3f u, v, w;

		// compute "negative" look direction by substracting
		w = c - looking_at;
		w.normalize();

		// compute cross product
		u = Vec3f(0, 0, 1).crossProduct(w); // side = (0,0,1) x w
		u.normalize();

		// up = side x look
		v = w.crossProduct(u);
		v.normalize();

		Matrix44f rotation; // identity

		// note the format: set(COLUMN, ROW, value)
		// it may be different for your matrix implementation

		rotation[0][0] = u.x;
		rotation[1][0] = u.y;
		rotation[2][0] = u.z;

		rotation[0][1] = v.x;
		rotation[1][1] = v.y;
		rotation[2][1] = v.z;

		rotation[0][2] = w.x;
		rotation[1][2] = w.y;
		rotation[2][2] = w.z;

		Matrix44f translation; // identity
		translation[3][0] = -c.x;
		translation[3][1] = -c.y;
		translation[3][2] = -c.z;

		// view matrix
		Matrix44f view;
		view = rotation * translation;

		// print matrix on console
		return view;
	}
};


class CCube
{
public:
	CCube(double size)
		:
		scale (1,1,1),
		rotation(0,0,0),
		translation (0,0,0)
	{
		float x = size / 2;
		points =
		{
			{ -x, +x, +x },
			{ +x, +x, +x },
			{ +x, +x, -x },
			{ -x, +x, -x },
			{ -x, -x, +x },
			{ +x, -x, +x },
			{ +x, -x, -x },
			{ -x, -x, -x }
		};
	}

	TIndexedLineList GetLines(void) const
	{
		vector<Vec3f> res = points;
		

		double cosa = std::cos(rotation.z);
		double sina = std::sin(rotation.z);

		double cosb = std::cos(rotation.x);
		double sinb = std::sin(rotation.x);

		double cosc = std::cos(rotation.y);
		double sinc = std::sin(rotation.y);

		double Axx = cosa * cosb;
		double Axy = cosa * sinb*sinc - sina * cosc;
		double Axz = cosa * sinb*cosc + sina * sinc;

		double Ayx = sina * cosb;
		double Ayy = sina * sinb*sinc + cosa * cosc;
		double Ayz = sina * sinb*cosc - cosa * sinc;

		double Azx = -sinb;
		double Azy = cosb * sinc;
		double Azz = cosb * cosc;

		for (int i = 0; i < res.size(); i++) {
			double px = res[i].x;
			double py = res[i].y;
			double pz = res[i].z;

			res[i].x = Axx * px + Axy * py + Axz * pz;
			res[i].y = Ayx * px + Ayy * py + Ayz * pz;
			res[i].z = Azx * px + Azy * py + Azz * pz;
		}

		for (auto & i : res)
		{
			i.x *= scale.x;
			i.y *= scale.y;
			i.z *= scale.z;
		}

		for (auto & i : res)
		{
			i.x += translation.x;
			i.y += translation.y;
			i.z += translation.z;
		}

		
		
		
		return{
			res,
			{ 4,5,6, 4,7,6, 3,2,6, 3,7,6, 0,3,7, 0,4,7, 0,1,5, 0,4,5, 1,2,6, 1,5,6, 0,1,2, 0,3,2 }
		};
	}

	void Scale(const Vec3f & s)
	{
		scale = scale + s;
	}

	void Translate(const Vec3f & t)
	{
		translation.x += t.x;
		translation.y += t.y;
		translation.z += t.z;
	}

	void Rotate(const Vec3f & r)
	{
		rotation = rotation + r;
	}

//private:
	Vec3f scale;
	Vec3f rotation;
	Vec3f translation;
	vector<Vec3f> points;
};

CCube c1(25);
ScreenTransformer t;

void computePixelCoordinates(
	const Vec3f pWorld,
	Vec2i &pRaster,
	const Matrix44f &worldToCamera,
	const float &canvasWidth,
	const float &canvasHeight,
	const uint32_t &imageWidth,
	const uint32_t &imageHeight)
{
	Vec3f pCamera;
	worldToCamera.multVecMatrix(pWorld, pCamera);
	Vec2f pScreen;
	pScreen.x = pCamera.x / -pCamera.z;
	pScreen.y = pCamera.y / -pCamera.z;
	Vec2f pNDC;
	pNDC.x = (pScreen.x + canvasWidth * 0.5) / canvasWidth;
	pNDC.y = (pScreen.y + canvasHeight * 0.5) / canvasHeight;
	pRaster.x = (int)(pNDC.x * imageWidth);
	pRaster.y = (int)((1 - pNDC.y) * imageHeight);
}

float canvasWidth = 10, canvasHeight = 10;
uint32_t imageWidth = Graphics::ScreenWidth, imageHeight = Graphics::ScreenHeight;

const Vec3f verts[146] = {
	{ 0,    39.034,         0 },{ 0.76212,    36.843,         0 },
{ 3,    36.604,         0 },{ 1,    35.604,         0 },
{ 2.0162,    33.382,         0 },{ 0,    34.541,         0 },
{ -2.0162,    33.382,         0 },{ -1,    35.604,         0 },
{ -3,    36.604,         0 },{ -0.76212,    36.843,         0 },
{ -0.040181,     34.31,         0 },{ 3.2778,    30.464,         0 },
{ -0.040181,    30.464,         0 },{ -0.028749,    30.464,         0 },
{ 3.2778,    30.464,         0 },{ 1.2722,    29.197,         0 },
{ 1.2722,    29.197,         0 },{ -0.028703,    29.197,         0 },
{ 1.2722,    29.197,         0 },{ 5.2778,    25.398,         0 },
{ -0.02865,    25.398,         0 },{ 1.2722,    29.197,         0 },
{ 5.2778,    25.398,         0 },{ 3.3322,    24.099,         0 },
{ -0.028683,    24.099,         0 },{ 7.1957,    20.299,         0 },
{ -0.02861,    20.299,         0 },{ 5.2778,    19.065,         0 },
{ -0.028663,    18.984,         0 },{ 9.2778,    15.265,         0 },
{ -0.028571,    15.185,         0 },{ 9.2778,    15.265,         0 },
{ 7.3772,    13.999,         0 },{ -0.028625,    13.901,         0 },
{ 9.2778,    15.265,         0 },{ 12.278,    8.9323,         0 },
{ -0.028771,    8.9742,         0 },{ 12.278,    8.9323,         0 },
{ 10.278,    7.6657,         0 },{ -0.028592,    7.6552,         0 },
{ 15.278,    2.5994,         0 },{ -0.028775,    2.6077,         0 },
{ 15.278,    2.5994,         0 },{ 13.278,    1.3329,         0 },
{ -0.028727,    1.2617,         0 },{ 18.278,   -3.7334,         0 },
{ 18.278,   -3.7334,         0 },{ 2.2722,   -1.2003,         0 },
{ -0.028727,   -1.3098,         0 },{ 4.2722,        -5,         0 },
{ 4.2722,        -5,         0 },{ -0.028727,        -5,         0 },
{ -3.3582,    30.464,         0 },{ -3.3582,    30.464,         0 },
{ -1.3526,    29.197,         0 },{ -1.3526,    29.197,         0 },
{ -1.3526,    29.197,         0 },{ -5.3582,    25.398,         0 },
{ -1.3526,    29.197,         0 },{ -5.3582,    25.398,         0 },
{ -3.4126,    24.099,         0 },{ -7.276,    20.299,         0 },
{ -5.3582,    19.065,         0 },{ -9.3582,    15.265,         0 },
{ -9.3582,    15.265,         0 },{ -7.4575,    13.999,         0 },
{ -9.3582,    15.265,         0 },{ -12.358,    8.9323,         0 },
{ -12.358,    8.9323,         0 },{ -10.358,    7.6657,         0 },
{ -15.358,    2.5994,         0 },{ -15.358,    2.5994,         0 },
{ -13.358,    1.3329,         0 },{ -18.358,   -3.7334,         0 },
{ -18.358,   -3.7334,         0 },{ -2.3526,   -1.2003,         0 },
{ -4.3526,        -5,         0 },{ -4.3526,        -5,         0 },
{ 0,     34.31,  0.040181 },{ 0,    30.464,   -3.2778 },
{ 0,    30.464,  0.040181 },{ 0,    30.464,  0.028749 },
{ 0,    30.464,   -3.2778 },{ 0,    29.197,   -1.2722 },
{ 0,    29.197,   -1.2722 },{ 0,    29.197,  0.028703 },
{ 0,    29.197,   -1.2722 },{ 0,    25.398,   -5.2778 },
{ 0,    25.398,   0.02865 },{ 0,    29.197,   -1.2722 },
{ 0,    25.398,   -5.2778 },{ 0,    24.099,   -3.3322 },
{ 0,    24.099,  0.028683 },{ 0,    20.299,   -7.1957 },
{ 0,    20.299,   0.02861 },{ 0,    19.065,   -5.2778 },
{ 0,    18.984,  0.028663 },{ 0,    15.265,   -9.2778 },
{ 0,    15.185,  0.028571 },{ 0,    15.265,   -9.2778 },
{ 0,    13.999,   -7.3772 },{ 0,    13.901,  0.028625 },
{ 0,    15.265,   -9.2778 },{ 0,    8.9323,   -12.278 },
{ 0,    8.9742,  0.028771 },{ 0,    8.9323,   -12.278 },
{ 0,    7.6657,   -10.278 },{ 0,    7.6552,  0.028592 },
{ 0,    2.5994,   -15.278 },{ 0,    2.6077,  0.028775 },
{ 0,    2.5994,   -15.278 },{ 0,    1.3329,   -13.278 },
{ 0,    1.2617,  0.028727 },{ 0,   -3.7334,   -18.278 },
{ 0,   -3.7334,   -18.278 },{ 0,   -1.2003,   -2.2722 },
{ 0,   -1.3098,  0.028727 },{ 0,        -5,   -4.2722 },
{ 0,        -5,   -4.2722 },{ 0,        -5,  0.028727 },
{ 0,    30.464,    3.3582 },{ 0,    30.464,    3.3582 },
{ 0,    29.197,    1.3526 },{ 0,    29.197,    1.3526 },
{ 0,    29.197,    1.3526 },{ 0,    25.398,    5.3582 },
{ 0,    29.197,    1.3526 },{ 0,    25.398,    5.3582 },
{ 0,    24.099,    3.4126 },{ 0,    20.299,     7.276 },
{ 0,    19.065,    5.3582 },{ 0,    15.265,    9.3582 },
{ 0,    15.265,    9.3582 },{ 0,    13.999,    7.4575 },
{ 0,    15.265,    9.3582 },{ 0,    8.9323,    12.358 },
{ 0,    8.9323,    12.358 },{ 0,    7.6657,    10.358 },
{ 0,    2.5994,    15.358 },{ 0,    2.5994,    15.358 },
{ 0,    1.3329,    13.358 },{ 0,   -3.7334,    18.358 },
{ 0,   -3.7334,    18.358 },{ 0,   -1.2003,    2.3526 },
{ 0,        -5,    4.3526 },{ 0,        -5,    4.3526 }
};

const uint32_t numTris = 128;

const uint32_t tris[numTris * 3] = {
	8,   7,   9,   6,   5,   7,   4,   3,   5,   2,   1,   3,   0,   9,   1,
	5,   3,   7,   7,   3,   9,   9,   3,   1,  10,  12,  11,  13,  15,  14,
	15,  13,  16,  13,  17,  16,  18,  20,  19,  17,  20,  21,  20,  23,  22,
	20,  24,  23,  23,  26,  25,  24,  26,  23,  26,  27,  25,  26,  28,  27,
	27,  30,  29,  28,  30,  27,  30,  32,  31,  30,  33,  32,  27,  30,  34,
	32,  36,  35,  33,  36,  32,  36,  38,  37,  36,  39,  38,  38,  41,  40,
	39,  41,  38,  41,  43,  42,  41,  44,  43,  44,  45,  43,  44,  47,  46,
	44,  48,  47,  48,  49,  47,  48,  51,  50,  10,  52,  12,  13,  53,  54,
	55,  17,  54,  13,  54,  17,  56,  57,  20,  17,  58,  20,  20,  59,  60,
	20,  60,  24,  60,  61,  26,  24,  60,  26,  26,  61,  62,  26,  62,  28,
	62,  63,  30,  28,  62,  30,  30,  64,  65,  30,  65,  33,  62,  66,  30,
	65,  67,  36,  33,  65,  36,  36,  68,  69,  36,  69,  39,  69,  70,  41,
	39,  69,  41,  41,  71,  72,  41,  72,  44,  44,  72,  73,  44,  74,  75,
	44,  75,  48,  48,  75,  76,  48,  77,  51,  78,  80,  79,  81,  83,  82,
	83,  81,  84,  81,  85,  84,  86,  88,  87,  85,  88,  89,  88,  91,  90,
	88,  92,  91,  91,  94,  93,  92,  94,  91,  94,  95,  93,  94,  96,  95,
	95,  98,  97,  96,  98,  95,  98, 100,  99,  98, 101, 100,  95,  98, 102,
	100, 104, 103, 101, 104, 100, 104, 106, 105, 104, 107, 106, 106, 109, 108,
	107, 109, 106, 109, 111, 110, 109, 112, 111, 112, 113, 111, 112, 115, 114,
	112, 116, 115, 116, 117, 115, 116, 119, 118,  78, 120,  80,  81, 121, 122,
	123,  85, 122,  81, 122,  85, 124, 125,  88,  85, 126,  88,  88, 127, 128,
	88, 128,  92, 128, 129,  94,  92, 128,  94,  94, 129, 130,  94, 130,  96,
	130, 131,  98,  96, 130,  98,  98, 132, 133,  98, 133, 101, 130, 134,  98,
	133, 135, 104, 101, 133, 104, 104, 136, 137, 104, 137, 107, 137, 138, 109,
	107, 137, 109, 109, 139, 140, 109, 140, 112, 112, 140, 141, 112, 142, 143,
	112, 143, 116, 116, 143, 144, 116, 145, 119
};

Game::Game( MainWindow& wnd )
	:
	wnd( wnd ),
	gfx( wnd )
{



	
}

void Game::Go()
{
	if (wnd.kbd.KeyIsPressed(VK_ESCAPE))
		wnd.Kill();

	gfx.BeginFrame();
	UpdateModel();
	ComposeFrame();
	gfx.EndFrame();
}

int counterx = 0;
int countery = 0;
bool forward = false;
bool up = true;
Camera c(Vec3f(50, 50, 50), Vec3f(150, 1, 1));

void Game::UpdateModel()
{
	if (forward == false)
	{
		counterx++;
		if (counterx >= 50)
			forward = true;
	}
	else
	{
		counterx--;
		if (counterx < -50)
			forward = false;
	}

	if (up == false)
	{
		countery++;
		if (countery >= 50)
			up = true;
	}
	else
	{
		countery--;
		if (countery < -50)
			up = false;
	}

	//c1.Translate(Vec3f(counterx * 5, 0, countery * 5));
	c1.translation =  Vec3f(counterx * 1, 0, countery * 1);
	//c1.Scale(Vec3f(counter * 0.0001, -counter * 0.0001, -counter * 0.0001));
	c1.Rotate(Vec3f(0.01,0.01,0.01));


	/*-----------------------------------------------*/
	// up down with cam
	if (wnd.kbd.KeyIsPressed(VK_SPACE))
	{
		c.pos = c.pos + Vec3f(0, 1, 0);
		c.looking_at = c.looking_at + Vec3f(0, 1, 0);
	}
	if (wnd.kbd.KeyIsPressed(VK_SHIFT))
	{
		c.pos = c.pos - Vec3f(0, 1, 0);
		c.looking_at = c.looking_at - Vec3f(0, 1, 0);
	}

	// forward backward with cam
	if (wnd.kbd.KeyIsPressed(0x41 + ('w' - 'a')))
	{
		c.pos = c.pos + Vec3f(0.1, 0, 0);
	}
	if (wnd.kbd.KeyIsPressed(0x41 + ('s' - 'a')))
	{
		c.pos = c.pos - Vec3f(0.1, 0, 0);
	}
	

	// sideways with cam
	//if (wnd.kbd.KeyIsPressed(0x41 + 'w'))
	//	c.pos = c.pos + Vec3f(-0.1, 0, 0);
	//if (wnd.kbd.KeyIsPressed(0x41 + 'w'))
	//	c.pos = c.pos + Vec3f(-0.1, 0, 0);

	/*-----------------------------------------------*/
}

void Game::ComposeFrame()
{
	Matrix44f cameraToWorld;
	
	//cameraToWorld = c.get_view();
	
	cameraToWorld = Matrix44f (
		0.871214, 0, -0.490904, 0,
		-0.192902, 0.919559, -0.342346, 0,
		0.451415, 0.392953, 0.801132, 0,
		24.777467, 29.361945, 27.993464, 1);
	Matrix44f worldToCamera = cameraToWorld.inverse();
	Vec2i v0Raster, v1Raster, v2Raster;
	TIndexedLineList list = c1.GetLines();
	ScreenTransformer transformer;

	//for (unsigned i = 0; i < numTris; ++i)
	//{
	//	const Vec3f &v0World = verts[tris[i * 3]];
	//	const Vec3f &v1World = verts[tris[i * 3 + 1]];
	//	const Vec3f &v2World = verts[tris[i * 3 + 2]];
	//	Vec2i v0Raster, v1Raster, v2Raster;
	//	computePixelCoordinates(v0World, v0Raster, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	//	computePixelCoordinates(v1World, v1Raster, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	//	computePixelCoordinates(v2World, v2Raster, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	//
	//	gfx.DrawLine_s(v0Raster.x, v0Raster.y, v1Raster.x, v1Raster.y, Colors::Gray);
	//	gfx.DrawLine_s(v1Raster.x, v1Raster.y, v2Raster.x, v2Raster.y, Colors::Gray);
	//	gfx.DrawLine_s(v2Raster.x, v2Raster.y, v0Raster.x, v0Raster.y, Colors::Gray);
	//}

	for (unsigned i = 0; i < list.indices.size() - 1; i += 3)
	{
		const Vec3f point1 = list.points[list.indices[i]];
		const Vec3f point2 = list.points[list.indices[i + 1]];
		const Vec3f point3 = list.points[list.indices[i + 2]];
		Vec2i res1 = transformer.Transform(point1, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
		Vec2i res2 = transformer.Transform(point2, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
		Vec2i res3 = transformer.Transform(point3, worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);

		gfx.DrawLine_s(res1.x, res1.y, res2.x, res2.y, Colors::Blue);
		gfx.DrawLine_s(res2.x, res2.y, res3.x, res3.y, Colors::Blue);
	}

	//draw axes:
	Vec2i op = transformer.Transform(Vec3f(0, 0, 0), worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	Vec2i x_axis = transformer.Transform(Vec3f(100, 0, 0), worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	Vec2i y_axis = transformer.Transform(Vec3f(0, 100, 0), worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	Vec2i z_axis = transformer.Transform(Vec3f(0, 0, 100), worldToCamera, canvasWidth, canvasHeight, imageWidth, imageHeight);
	gfx.DrawLine_s(op.x, op.y, x_axis.x, x_axis.y, Colors::MakeRGB(125, 0, 0));
	gfx.DrawLine_s(op.x, op.y, y_axis.x, y_axis.y, Colors::MakeRGB(0, 125, 0));
	gfx.DrawLine_s(op.x, op.y, z_axis.x, z_axis.y, Colors::MakeRGB(0, 0, 125));
}