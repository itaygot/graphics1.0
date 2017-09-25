//
//  BouncingBalls.cpp
//  cg-projects
//
//  Based on exercise given at HUJI Computer Graphics course, 2013.
//

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "bouncingBalls.h"
#include "ogldev_glut_backend.h"		// glut calls
#include "ShaderIO.h"					// handling shaders


#include <cmath>				// std::sin(); std::cos(); std::rand();
#include "glm/geometric.hpp"	// normalize(); length();
#include <algorithm>			// min(); max();


#define VERTEX_SHADER_PATH "SimpleShader.vert"
#define FRAGMENT_SHADER_PATH "SimpleShader.frag"
#define PI 3.1415926f


/************************************************************/
//				 STATIC IMPLEMENTATION SECTION				//
/************************************************************/
#define CIRCLE_EDGES_AMOUNT 30
#define SCALARS_PER_VERTEX 2
#define DFLT_RADIUS 0.1f
#define BOUNCINESS 0.85f

#define FRAME_RATE_MILIS 20

#define LIGHT_POS_X 1.5f
#define LIGHT_POS_Y -2.0f







/************************************************************/
//					HELPER FUNCTIONS						//
/************************************************************/

static float rand_float(size_t log_n) {
	int n = 1 << log_n;
	int m = std::rand() & (2 * n - 1);
	return (float)(m - n) / (float)n;
}

static const float* pickAColor()
{
	// Colors:
	const static float RED[4] = { 1.f, 0.f, 0.f, 1.f };
	const static float GREEN[4] = { 0.f, 1.f, 0.f, 1.f };
	const static float BLUE[4] = { 0.f, 0.f, 1.f, 1.f };
	const static float YELLOW[4] = { 1.f, 1.f, 0.f, 1.f };
	const static float NAVY[4] = { 0.f, 0.f, 128.f / 255, 1.f };
	const static float LAWN_GREEN[4] = { 124.f / 252, 252.f / 255, 0.f, 1.f };


	const static float* COLORS_ARRAY[] = { RED, BLUE, YELLOW, GREEN, LAWN_GREEN, NAVY };

	return COLORS_ARRAY[std::rand() % (sizeof(COLORS_ARRAY) / sizeof(float *))];

}



/************************************************************/
//				BOUNCING BALLS IMPLEMENTATION				//
/************************************************************/



BouncingBalls::BouncingBalls(){}

BouncingBalls::~BouncingBalls()
{
	if (_vbo != 0)
		glDeleteBuffers(1, &_vbo);
}

bool BouncingBalls::Init()
{
	// Use glut Timer func instead of the Idle func
	GLUTBackendUseTimer(true, FRAME_RATE_MILIS);

	// Set animation on
	_animate = true;

	/** light position **/
	_lightPos[0] = LIGHT_POS_X;
	_lightPos[1] = LIGHT_POS_Y;


	// Create the shader program
	GLuint program = programManager::sharedInstance().createProgram
					("default",
					VERTEX_SHADER_PATH,
					FRAGMENT_SHADER_PATH);
	glUseProgram(program);

	// Uniform Variables
	_fillColorUV  = glGetUniformLocation(program, "fillColor");
	_centerUV = glGetUniformLocation(program, "center");
	_radiusUV = glGetUniformLocation(program, "radius");
	_lightUV = glGetUniformLocation(program, "light");

	
	/*** Create the vertices of the prototype ball ***/
		
	float vertices[CIRCLE_EDGES_AMOUNT * SCALARS_PER_VERTEX];
		
	int vertex = 0;
	float angle = 0;
	for (vertex = 0; vertex < CIRCLE_EDGES_AMOUNT; vertex++, angle += 2.f * PI / CIRCLE_EDGES_AMOUNT){
		vertices[SCALARS_PER_VERTEX * vertex] = std::cos(-angle);		// x coordinate
		vertices[SCALARS_PER_VERTEX * vertex + 1] = std::sin(-angle);	// y coordinate
		/* Note: The negative angle is so we have clock-wise vertices */
	}

	
	// Create and load Vertex Buffer Object:
	glGenBuffers(1, &_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		
		

	// set array interpretation
	_posAttrib = glGetAttribLocation(program, "position");
	glEnableVertexAttribArray(_posAttrib);
	glVertexAttribPointer(_posAttrib,			// attribute handle
							SCALARS_PER_VERTEX,	// number of scalars per vertex
							GL_FLOAT,				// scalar type
							GL_FALSE,
							0,
							0);
		


	

	return true;
}

void BouncingBalls::Run() {
	GLUTBackendRun(this);
}

void BouncingBalls::KeyboardCB(OGLDEV_KEY OgldevKey, OGLDEV_KEY_STATE OgldevKeyState) {

	switch (OgldevKey) {

	case OGLDEV_KEY_q:
		GLUTBackendLeaveMainLoop();
		break;

	case OGLDEV_KEY_k:
		_animate = !_animate;
		break;

	}
}

void BouncingBalls::MouseCB(OGLDEV_MOUSE Button, OGLDEV_KEY_STATE State, int x, int y) {

	if (Button == OGLDEV_MOUSE_BUTTON_LEFT && State == OGLDEV_KEY_STATE_PRESS)
		addBall((float)x * 2 / glutGet(GLUT_WINDOW_WIDTH) - 1,
			1 - (float)y * 2 / glutGet(GLUT_WINDOW_HEIGHT));

}

void BouncingBalls::RenderSceneCB() {

	// Clear the screen buffer
	glClear(GL_COLOR_BUFFER_BIT);

	//GLenum polygonMode = GL_POINT;   // Also try using GL_FILL and GL_POINT
	GLenum polygonMode = GL_FILL;   // Also try using GL_FILL and GL_POINT
	glPolygonMode(GL_FRONT_AND_BACK, polygonMode);


	// Bind buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);

	// Move and Draw balls
	moveBalls();
	handleWallsCollisions();
	handleBallsCollisions();
	drawBalls();

	GLUTBackendSwapBuffers();

}

void BouncingBalls::TimerCB(int value) {
	if (_animate)
		glutPostRedisplay();


}



void BouncingBalls::addBall(float x, float y)
{
	Ball ball = Ball(x, y);

	ball._bounciness = 0.85f;

	// Pick a color
	ball._color = pickAColor();

	// Draw a velocity
	ball._velo.x = rand_float(3) * 0.1f / 2;
	ball._velo.y = rand_float(3) * 0.1f / 2;
	/*ball._velo.x  = 0;
	ball._velo.y = 0;*/

	// Find the default radius, first check the walls then the other balls
	float radius = std::min(DFLT_RADIUS, x + 1);
	radius = std::min(radius, 1 - x);
	radius = std::min(radius, y + 1);
	radius = std::min(radius, 1 - y);
	if (radius <= 0.f) return;
	//ball._def_radius = (ball._radius = radius);
	ball._radius = radius;

	// add to balls list
	_balls.push_back(ball);
}

inline void BouncingBalls::handleBallsCollisions()
{
	for (auto it1 = _balls.begin(); it1 != _balls.end(); it1++)
	for (auto it2 = it1 + 1; it2 != _balls.end(); it2++)
		it1->ballCollision(*it2);
}

inline void BouncingBalls::moveBalls()
{
	static float GRAVITY_PER_FRAME = FRAME_RATE_MILIS * 1.0f / (128 * 64 * 1.25);


	for (auto it = _balls.begin(); it != _balls.end(); it++){
		// move
		it->_pos.x += it->_velo.x;
		it->_pos.y += it->_velo.y;

		// Apply Gravity:
		/*if(!it->onFloor())
			it->_velo.y -= GRAVITY_PER_FRAME;	*/
		if (it->insideFloor()) { // "touching" floor, several cases:

			// if tangent to floor and no vertical movement: keep it still, don't apply gravity
			if (it->_velo.y == 0.f && it->_pos.y == -1 + it->_radius)
				continue;


			else if (it->_velo.y >= 0.f)	// going up from within floor, couple case:

				if (it->_velo.y < GRAVITY_PER_FRAME) {	// too slow up, make the ball "rest" vertically
					it->_pos.y = it->_radius - 1;
					it->_velo.y = 0.f;
				}
				else	// not too slow up, apply gravity
					it->_velo.y -= GRAVITY_PER_FRAME;

			else	// touching floor and going down: don't apply gravity
				continue;
		}

		else 	// not touching floor - apply gravity
			it->_velo.y -= GRAVITY_PER_FRAME;
	

	}	// done traversing balls

}

inline void BouncingBalls::handleWallsCollisions()
{
	for (auto it = _balls.begin(); it != _balls.end(); it++)
		it->wallCollison();
}

inline void BouncingBalls::drawBalls()
{
	float lightPoint[2];
	float distFromLight;
	for (auto it = _balls.begin(); it != _balls.end(); ++it)
	{
		glUniform2f(_centerUV, it->_pos.x, it->_pos.y);
		glUniform1f(_radiusUV, it->_radius);
		glUniform4f(_fillColorUV, it->_color[0], it->_color[1], it->_color[2], it->_color[3]);

		// Deal with lighting... extract the light point on the ball
		distFromLight = std::sqrt(std::pow(it->_pos.x - _lightPos[0], 2) +
								  std::pow(it->_pos.y - _lightPos[1], 2));
		lightPoint[0] = it->_pos.x + 0.33f * (_lightPos[0] - it->_pos.x) * it->_radius / distFromLight;
		lightPoint[0] = it->_pos.y + 0.33f * (_lightPos[1] - it->_pos.y) * it->_radius / distFromLight;
		
		glUniform2f(_lightUV, it->_pos.x + 0.33f * (_lightPos[0] - it->_pos.x) * it->_radius / distFromLight,
			it->_pos.y + 0.33f * (_lightPos[1] - it->_pos.y) * it->_radius / distFromLight);

		glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_EDGES_AMOUNT + 2);
		//glDrawArrays(GL_TRIANGLES, 0, CIRCLE_EDGES_AMOUNT + 2);
		//glDrawArrays(GL_LINES, 0, CIRCLE_EDGES_AMOUNT + 2);
		
	}
}


