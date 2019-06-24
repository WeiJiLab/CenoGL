#include "../include/window.h"
#include "../include/log.h"
#include <list>
#include <algorithm>


using namespace CenoGL;

Window::Window() {

}

Window::Window(int windowWidth, int windowHeight, const char *windowName){
    this->windowHeight = windowHeight;
    this->windowWidth = windowWidth;
    this->windowName = (char *)windowName;
}

void Window::OnEvent(SDL_Event* Event) {

}

bool Window::Init() {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        Log("Unable to Init SDL: %s", SDL_GetError());
        return false;
    }

    if(!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        Log("Unable to Init hinting: %s", SDL_GetError());
    }

    if((window = SDL_CreateWindow(windowName,
                    SDL_WINDOWPOS_UNDEFINED, 
                    SDL_WINDOWPOS_UNDEFINED,
                    windowWidth, windowHeight,
                    SDL_WINDOW_SHOWN)) == NULL) {
        Log("Unable to create SDL Window: %s", SDL_GetError());
        return false;
    }

    primarySurface = SDL_GetWindowSurface(window);

    if((renderer = SDL_GetRenderer(window)) == NULL) {
        Log("Unable to create renderer '%s'", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);
	
	initPixelMatrixBuffer();	// init pixel buffer

    return true;
}

void Window::initPixelMatrixBuffer(){
	this->pixelMatrix = new PixelMatrix();
	this->pixelMatrix->setHeight(this->windowHeight);
	this->pixelMatrix->setWidth(this->windowWidth);

	Pixel** pixels = new Pixel*[this->windowHeight];
	for (int rows = 0; rows < this->windowHeight; rows++) {
		pixels[rows] = new Pixel[this->windowWidth];
    	for (int cols = 0; cols < this->windowWidth; cols ++) {
			Pixel *pixel = new Pixel();
			pixel->setColor(new Color(
				 0x00, 0xFF, 0xB8, 0xFF
			));
			pixels[rows][cols] = *pixel;
    	}
    }
	this->pixelMatrix->setPixels(pixels);

	this->gl2d = new gl2D(this->pixelMatrix);
	this->gl3d = new gl3D(this->pixelMatrix);

	this->meshCube.loadFromObjFiles("teapot.obj");

	// Projection Matrix
	this->matProjection = this->gl3d->glMatrixMakeProjection(90.0f, (float)this->windowHeight / (float)this->windowWidth, 0.1f, 1000.0f);
}

float fTheta = 0.0f;
void Window::Update() {
	// this->gl2d->drawLine(0,0,200,200,0xFF000000);
	// this->gl2d->drawCircle(100,100,20,0x00FF0000);
	// this->gl2d->drawTriangle(10,10,100,10,10,100,0xFFFFFF00);
	// this->gl2d->fillTriangle(120,120,210,120,120,210,0xFF00FF00);
	// this->gl2d->fillCircle(210,210,20,0xFFFF0000);

	// Rotation Z and X
	Mat4x4 matRotZ, matRotX;
	// fTheta +=0.1f;
	matRotZ = this->gl3d->glMatrixMakeRotationZ(fTheta);
	matRotX = this->gl3d->glMatrixMakeRotationX(fTheta);

	Mat4x4 matTrans = this->gl3d->glMatrixMakeTranslation(0.0f,0.0f,10.0f);

	Mat4x4 matWorld;
	matWorld = this->gl3d->glMatrixMakeIdentity();
	matWorld = this->gl3d->glMatrixMultiplyMatrix(matRotZ,matRotX);
	matWorld = this->gl3d->glMatrixMultiplyMatrix(matWorld,matTrans);

	Vec3D vUp; vUp.x = 0; vUp.y = 1; vUp.z = 0;
	Vec3D vTarget; vTarget.x = 0; vTarget.y = 0; vTarget.z = 1;
	Mat4x4 matCmaeraRota = this->gl3d->glMatrixMakeRotationX(fYaw);
	this->vLookDir = this->gl3d->glMatrixMultiplyVector(matCmaeraRota,vTarget);
	vTarget = this->gl3d->glVectorAdd(this->vCamera,this->vLookDir);

	Mat4x4 matCamera = this->gl3d->glMatrixPointAt(this->vCamera,vTarget,vUp);

	// camera view matrix
	Mat4x4 matView = this->gl3d->glMatrixQuickInverse(matCamera);

	std::vector<Triangle> vecTriangleToRaster;

	for(auto tri : this->meshCube.tris){
		Triangle triProjected, triTransformed,triViewed;
		tri.color = 0xFFD700FF;
		triTransformed.p[0] = this->gl3d->glMatrixMultiplyVector(matWorld,tri.p[0]);
		triTransformed.p[1] = this->gl3d->glMatrixMultiplyVector(matWorld,tri.p[1]);
		triTransformed.p[2] = this->gl3d->glMatrixMultiplyVector(matWorld,tri.p[2]);

		Vec3D normal,line1,line2;
		line1 = this->gl3d->glVectorSub(triTransformed.p[1],triTransformed.p[0]);
		line2 = this->gl3d->glVectorSub(triTransformed.p[2],triTransformed.p[0]);
		normal = this->gl3d->glVectorCrossProduct(line1,line2);
		normal = this->gl3d->glVectorNormalise(normal);

		Vec3D cameraRay = this->gl3d->glVectorSub(triTransformed.p[0],this->vCamera);

		if(this->gl3d->glVectorDotProduct(normal,cameraRay) < 0.0f){
			
			//Illumination
			Vec3D lightDirection;
			lightDirection.x = -1.0f;
			lightDirection.y = 1.0f;
			lightDirection.z =-1.0f;
			lightDirection = this->gl3d->glVectorNormalise(lightDirection);

			float dp = fmax(0.1f,this->gl3d->glVectorDotProduct(lightDirection,normal));

			uint32_t color = this->gl3d->getLumColor(tri.color,dp);
			triTransformed.color = color;

			// convert world space to view space
			triViewed.p[0] = this->gl3d->glMatrixMultiplyVector(matView,triTransformed.p[0]);
			triViewed.p[1] = this->gl3d->glMatrixMultiplyVector(matView,triTransformed.p[1]);
			triViewed.p[2] = this->gl3d->glMatrixMultiplyVector(matView,triTransformed.p[2]);

			//clip viewed triangles against near plane
			int nClippedTriangles = 0;
			Triangle clipped[2];
			Vec3D near; near.x = 0.0f; near.y = 0.0f; near.z = 0.1f;
			Vec3D nor; nor.x = 0.0f; nor.y = 0.0f; nor.z = 1.0f;
			nClippedTriangles = this->gl3d->glTriangleClipAgainstPlane(near,nor,triViewed,clipped[0],clipped[1]);

			for(int i = 0; i<nClippedTriangles; i++){
				// Project triangles from 3D --> 2D
				triProjected.p[0] = this->gl3d->glMatrixMultiplyVector(this->matProjection,clipped[i].p[0]);
				triProjected.p[1] = this->gl3d->glMatrixMultiplyVector(this->matProjection,clipped[i].p[1]);
				triProjected.p[2] = this->gl3d->glMatrixMultiplyVector(this->matProjection,clipped[i].p[2]);
				triProjected.color = triTransformed.color;

				// scale into view 
				triProjected.p[0] = this->gl3d->glVectorDiv(triProjected.p[0],triProjected.p[0].w);
				triProjected.p[1] = this->gl3d->glVectorDiv(triProjected.p[1],triProjected.p[1].w);
				triProjected.p[2] = this->gl3d->glVectorDiv(triProjected.p[2],triProjected.p[2].w);

				// Scale into view
				Vec3D offsetView;
				offsetView.x = 1.0f;
				offsetView.y = 1.0f;
				offsetView.z = 0.0f;
				triProjected.p[0] = this->gl3d->glVectorAdd(triProjected.p[0],offsetView);
				triProjected.p[1] = this->gl3d->glVectorAdd(triProjected.p[1],offsetView);
				triProjected.p[2] = this->gl3d->glVectorAdd(triProjected.p[2],offsetView);

				triProjected.p[0].x *= 0.4f * (float)this->windowWidth;
				triProjected.p[0].y *= 0.4f * (float)this->windowHeight;
				triProjected.p[1].x *= 0.4f * (float)this->windowWidth;
				triProjected.p[1].y *= 0.4f * (float)this->windowHeight;
				triProjected.p[2].x *= 0.4f * (float)this->windowWidth;
				triProjected.p[2].y *= 0.4f * (float)this->windowHeight;

				vecTriangleToRaster.push_back(triProjected);
			}
		}
	}

	// sort triangles from back to front
	sort(vecTriangleToRaster.begin(),vecTriangleToRaster.end(),[](Triangle &t1,Triangle &t2){
		float z1 = (t1.p[0].z + t1.p[1].z + t1.p[2].z) / 3.0f;
		float z2 = (t2.p[0].z + t2.p[1].z + t2.p[2].z) / 3.0f;
		return z1 > z2;
	});

	for(auto &triToRaster : vecTriangleToRaster){
		// Clip triangles against all four screen edges, this could yield
			// a bunch of triangles, so create a queue that we traverse to 
			//  ensure we only test new triangles generated against planes
			Triangle clipped[2];
			std::list<Triangle> listTriangles;

			// Add initial triangle
			listTriangles.push_back(triToRaster);
			int nNewTriangles = 1;

			for (int p = 0; p < 4; p++)
			{
				int nTrisToAdd = 0;
				while (nNewTriangles > 0)
				{
					// Take triangle from front of queue
					Triangle test = listTriangles.front();
					listTriangles.pop_front();
					nNewTriangles--;

					// Clip it against a plane. We only need to test each 
					// subsequent plane, against subsequent new triangles
					// as all triangles after a plane clip are guaranteed
					// to lie on the inside of the plane. I like how this
					// comment is almost completely and utterly justified
					switch (p)
					{
						case 0:	{
							Vec3D planeP; planeP.x = 0.0f; planeP.y = 0.0f; planeP.z = 0.0f;
							Vec3D planeN; planeN.x = 0.0f; planeN.y = 1.0f; planeN.z = 0.0f;
							nTrisToAdd = this->gl3d->glTriangleClipAgainstPlane(planeP, planeN, test, clipped[0], clipped[1]); 
							break;
						}
						case 1:	{
							Vec3D planeP; planeP.x = 0.0f; planeP.y = (float)this->windowHeight - 1; planeP.z = 0.0f;
							Vec3D planeN; planeN.x = 0.0f; planeN.y = -1.0f; planeN.z = 0.0f;
							nTrisToAdd = this->gl3d->glTriangleClipAgainstPlane(planeP, planeN, test, clipped[0], clipped[1]); 
							break;
						}
						case 2:{
							Vec3D planeP; planeP.x = 0.0f; planeP.y = 0.0f; planeP.z = 0.0f;
							Vec3D planeN; planeN.x = 1.0f; planeN.y = 0.0f; planeN.z = 0.0f;
							nTrisToAdd = this->gl3d->glTriangleClipAgainstPlane(planeP, planeN, test, clipped[0], clipped[1]); 
							break;
						}
						case 3:	{
							Vec3D planeP; planeP.x =(float)this->windowWidth - 1; planeP.y = 0.0f; planeP.z = 0.0f;
							Vec3D planeN; planeN.x = -1.0f; planeN.y = 0.0f; planeN.z = 0.0f;
							nTrisToAdd = this->gl3d->glTriangleClipAgainstPlane(planeP, planeN , test, clipped[0], clipped[1]); 
							break;
						}
					}

					// Clipping may yield a variable number of triangles, so
					// add these new ones to the back of the queue for subsequent
					// clipping against next planes
					for (int w = 0; w < nTrisToAdd; w++){
						listTriangles.push_back(clipped[w]);
					}
				}
				nNewTriangles = listTriangles.size();
			}


			// Draw the transformed, viewed, clipped, projected, sorted, clipped triangles
			for (auto &t : listTriangles)
			{
				this->gl2d->fillTriangle(t.p[0].x, t.p[0].y, t.p[1].x, t.p[1].y, t.p[2].x, t.p[2].y, t.color);
				this->gl2d->drawTriangle(t.p[0].x, t.p[0].y, t.p[1].x, t.p[1].y, t.p[2].x, t.p[2].y, 0xFFFFFFFF);
			}
	}
}

void Window::Render() {
    SDL_RenderClear(renderer);
    for(int i = 0; i< this->pixelMatrix->getHeight(); i++){
        for(int j = 0; j< this->pixelMatrix->getWidth(); j++){
            SDL_SetRenderDrawColor(renderer, 
				pixelMatrix->getPixels()[i][j].getColor()->getR(),
			 	pixelMatrix->getPixels()[i][j].getColor()->getG(), 
			 	pixelMatrix->getPixels()[i][j].getColor()->getB(), 255);
            SDL_RenderDrawPoint(renderer, j, i);
        }
    }
    SDL_RenderPresent(renderer);
	this->clear();
}

void Window::clear(){
	for(int i = 0; i< this->pixelMatrix->getHeight(); i++){
		float r = 34.0f;
		float g = 89.0f;
		float b = 206.0f;
    	for(int j = 0; j< this->pixelMatrix->getWidth(); j++){
		  this->pixelMatrix->setPixel(j,i,new Pixel(new Color(r,g,b,0xFF)));
		  if(r>0){
			  r-=0.2f;
		  }
		  if(g>0){
			  g-=0.2f;
		  }
		  if(b>0){
			  b-=0.2f;
		  }
    	}
 	}
}
void Window::Cleanup() {
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if(window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

template<size_t size>
void Window::OnKeyPressed(bool(&keys)[size]){
	if(keys[SDLK_ESCAPE]){
		this->running = false;
		exit(0);
	}
	if(keys[0xFF & SDLK_LEFT]){
		this->vCamera.y-=0.5;
	}
	if(keys[0xFF & SDLK_RIGHT]){
		this->vCamera.y+=0.5;
	}
	if(keys[0xFF & SDLK_UP]){
		this->vCamera.x-=0.5;
	}
	if(keys[0xFF & SDLK_DOWN]){
		this->vCamera.x+=0.5;
	}
	
	Vec3D vForward = this->gl3d->glVectorMul(this->vLookDir,0.5f);
	if(keys[SDLK_w]){
		this->vCamera = this->gl3d->glVectorAdd(this->vCamera,vForward);
	}
	if(keys[SDLK_s]){
		this->vCamera = this->gl3d->glVectorSub(this->vCamera,vForward);
	}
	if(keys[SDLK_a]){
		this->fYaw-=0.1f;
	}
	if(keys[SDLK_d]){
		this->fYaw+=0.1f;
	}
}

int Window::Execute() {
    if(!Init()) return 0;
        SDL_Event Event;
		bool keysHeld[323] = {false};
        while(running) {
            while(SDL_PollEvent(&Event) != 0) {
                OnEvent(&Event);
                if(Event.type == SDL_QUIT) {
					running = false;
				}
				if (Event.type == SDL_KEYDOWN){
					keysHeld[0xFF & Event.key.keysym.sym] = true;	
				}	
				if (Event.type == SDL_KEYUP){
					keysHeld[0xFF & Event.key.keysym.sym] = false;
				}
        	}
			this->OnKeyPressed(keysHeld);
            Update();
            Render();
            SDL_Delay(20); // Breath
        }
        Cleanup();
        return 1;
}


int Window::GetWindowWidth()  { return windowWidth;  }
int Window::GetWindowHeight() { return windowHeight;  }