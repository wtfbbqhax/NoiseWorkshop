//
//  ParticleSystemGPU.cpp
//  ParticlesGPU
//
//  Created by Andreas Müller on 11/01/2015.
//
//

#include "ParticleSystemOpticalFlow.h"

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::init()
{
	string xmlSettingsPath = "Settings/Particles.xml";
	gui.setup( "Particles", xmlSettingsPath );

	gui.add( textureSize.set("Texture Size", 128, 2, 1024) );
	
	gui.add( particleMaxAge.set("Particle Max Age", 10.0f, 0.0f, 20.0f) );
	gui.add( startScale.set("Mesh Start Scale", 1.0f, 0.001f, 10.0f) );
	gui.add( endScale.set("Mesh End Scale", 1.0f, 0.001f, 10.0f) );

	gui.add( maxVel.set("Max Vel", 1.0, 0.0f, 20.0f) );
	
	gui.add( noiseMagnitude.set("Noise Magnitude", 0.075, 0.01f, 20.0f) );
	gui.add( noisePositionScale.set("Noise Position Scale", 10.5f, 0.0001f, 350.0f) );
	gui.add( noiseTimeScale.set("Noise Time Scale", 0.1, 0.001f, 1.0f) );
	gui.add( noisePersistence.set("Noise Persistence", 0.2, 0.001f, 1.0f) );
	gui.add( oldVelToUse.set("Old Vel", 0.1, 0.0f, 1.0f) );

	gui.add( flowMagnitude.set("Flow Magnitude", 1.0, 0.0f, 10.0f) );
	gui.add( flowMaxLength.set("Flow Max", 5.0, 0.0001f, 10.0f) );

	gui.add( averageFlowMagnitude.set("Average Flow Magnitude", 1.0, 0.0001f, 10.0f) );
	
	gui.add( baseSpeed.set("Wind", ofVec3f(0.5,0,0), ofVec3f(-5), ofVec3f(5)) );
	gui.add( startColor.set("Start Color", ofColor::white, ofColor(0,0,0,0), ofColor(255,255,255,255)) );
	gui.add( endColor.set("End Color", ofColor(0,0,0,0), ofColor(0,0,0,0), ofColor(255,255,255,255)) );
	//gui.add( twistNoiseTimeScale.set("Twist Noise Time Scale", 0.01, 0.0f, 0.5f) );
	//gui.add( twistNoisePosScale.set("Twist Noise Pos Scale", 0.25, 0.0f, 2.0f) );
	//gui.add( twistMinAng.set("Twist Min Ang", -1, -5, 5) );
	//gui.add( twistMaxAng.set("Twist Max Ang", 2.5, -5, 5) );
	
	gui.loadFromFile( xmlSettingsPath );
	gui.minimizeAll();
	
	textureSize.addListener( this, &ParticleSystemOpticalFlow::intParamChanged );
	particleMaxAge.addListener( this, &ParticleSystemOpticalFlow::floatParamChanged );
	
	//trackerMaximumDistance.addListener(this, &KinectManager::trackerFloatParamChanged);
	
	// UI for the light and material
	string xmlSettingsPathLight = "Settings/LightAndMaterial.xml";
	guiMaterial.setup( "Light", xmlSettingsPathLight );
	guiMaterial.add( materialShininess.set("Material Shininess",  20,  0, 127) );
	guiMaterial.add( materialAmbient.set("Material Ambient",   	 ofColor(50,50,50), 	ofColor(0,0,0,0), ofColor(255,255,255,255)) );
	guiMaterial.add( materialSpecular.set("Material Specular",   ofColor(255,255,255),  ofColor(0,0,0,0), ofColor(255,255,255,255)) );
	guiMaterial.add( materialEmissive.set("Material Emmissive",  ofColor(255,255,255),  ofColor(0,0,0,0), ofColor(255,255,255,255)) );
	guiMaterial.loadFromFile( xmlSettingsPathLight );
	guiMaterial.setPosition( ofVec2f( ofGetWidth(), 10) - ofVec2f(guiMaterial.getWidth(), 0) );
	guiMaterial.minimizeAll();
	
	
	// Load shaders
	particleUpdate.load("Shaders/InstancedSpawnTexture/GL2/Update");
	particleDraw.load("Shaders/InstancedSpawnTexture/GL2/DrawInstancedGeometry");
	
	debugDrawOpticalFlow.load("Shaders/Debug/GL2/DrawOpticalFlow");

	// Set how many particles we are going to have, this is based on data texture size
	numParticles = textureSize * textureSize;
	
	reinitParticles();
	reinitParticlePosAndAge();
	
	ofPrimitiveMode primitiveMode = OF_PRIMITIVE_TRIANGLES; // as we'll be drawing ths mesh instanced many times, we need to have many triangles as opposed to one long triangle strip
	ofMesh tmpMesh;
	
	ofConePrimitive cone( 0.1, 0.1,  5, 2, primitiveMode );
	//tmpMesh = cone.getMesh();

	ofBoxPrimitive box( 0.15, 0.15, 1 ); // we gotta face in the -Z direction
	tmpMesh = box.getMesh();
	
	singleParticleMesh.append( tmpMesh );
	singleParticleMesh.setMode( primitiveMode );
	
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::reinitParticles()
{

	// Allocate buffers
	ofFbo::Settings fboSettings;
	fboSettings.width  = textureSize;
	fboSettings.height = textureSize;
	
	// We can create several color buffers for one FBO if we want to store velocity for instance,
	// then draw to them simultaneously from a shader using gl_FragData[0], gl_FragData[1], etc.
	fboSettings.numColorbuffers = 2;
	
	fboSettings.useDepth = false;
	fboSettings.internalformat = GL_RGBA32F;	// Gotta store the data as floats, they won't be clamped to 0..1
	fboSettings.textureTarget = GL_TEXTURE_2D;
	fboSettings.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
	fboSettings.wrapModeVertical = GL_CLAMP_TO_EDGE;
	fboSettings.minFilter = GL_NEAREST; // No interpolation, that would mess up data reads later!
	fboSettings.maxFilter = GL_NEAREST;
	
	ofDisableTextureEdgeHack();
	
		particleDataFbo.allocate( fboSettings );
	
	ofEnableTextureEdgeHack();
	
	// We are going to encode our data into the FBOs like this
	//
	//	Buffer 1: XYZ pos, W age
	//	Buffer 2: XYZ vel, W not used
	//
	

}


//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::reinitParticlePosAndAge()
{
	numParticles = textureSize * textureSize;
	
	// Initialise the starting and static data
	ofVec4f* startPositionsAndAge = new ofVec4f[numParticles];
	
	spawnPosBuffer.allocate( textureSize, textureSize, 3 );
	
	int tmpIndex = 0;
	for( int y = 0; y < textureSize; y++ )
	{
		for( int x = 0; x < textureSize; x++ )
		{
			ofVec3f pos(0,0,0);
			float startAge = ofRandom( particleMaxAge ); // position is not very important, but age is, by setting the lifetime randomly somewhere in the middle we can get a steady stream emitting
			
			startPositionsAndAge[tmpIndex] = ofVec4f( pos.x, pos.y, pos.z, startAge );
			
			ofVec3f spawnPos = MathUtils::randomPointOnSphere() * 0.02;
			
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 0 ] = spawnPos.x;
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 1 ] = spawnPos.y;
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 2 ] = spawnPos.z;
			
			tmpIndex++;
		}
	}
	
	// Upload it to the source texture
	particleDataFbo.source()->getTextureReference(0).loadData( (float*)&startPositionsAndAge[0].x,	 textureSize, textureSize, GL_RGBA );
	
	ofDisableTextureEdgeHack();
	
		spawnPosTexture.allocate( spawnPosBuffer, false );
		spawnPosTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
		spawnPosTexture.setTextureWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );
		spawnPosTexture.loadData( spawnPosBuffer );
	
	ofEnableTextureEdgeHack();
	
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::floatParamChanged( float & _param )
{
	reinitParticles();
	reinitParticlePosAndAge();
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::intParamChanged( int & _param )
{
	reinitParticles();
	reinitParticlePosAndAge();
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::allocateOpticalFlow( int _w, int _h )
{
	opticalFlowBuffer.allocate( _w, _h, 3 );
	
	ofDisableTextureEdgeHack();
	
		opticalFlowTexture.allocate( opticalFlowBuffer, false );
		opticalFlowTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
		opticalFlowTexture.setTextureWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );
		opticalFlowTexture.loadData( opticalFlowBuffer );
	
	ofEnableTextureEdgeHack();
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::setWorldToFlowParameters( ofMatrix4x4 _worldToKinect )
{
	worldToKinect = _worldToKinect;
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::setAverageFlow( ofVec2f _flow )
{
	averageFlow = _flow;
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::update( float _time, float _timeStep )
{

	particleMaterial.setAmbientColor( materialAmbient.get() );
	particleMaterial.setSpecularColor( materialSpecular.get() );
	particleMaterial.setEmissiveColor( materialEmissive.get() );
	particleMaterial.setShininess( materialShininess );
	
	
	/*
	// TEMP, update the spawn buffer
	float time = ofGetElapsedTimef();
	//ofVec3f spawnMiddle = ofVec3f(0,0.2,0).getRotated( time * 30.0, ofVec3f(0,0,1) ).getRotated( time * 25.0, ofVec3f(0,1,0) );
	ofVec3f spawnMiddle = ofVec3f( cosf(time), 0, sinf(time) ) * 3.0;
	
	int tmpIndex = 0;
	for( int y = 0; y < textureSize; y++ )
	{
		for( int x = 0; x < textureSize; x++ )
		{
			ofVec3f spawnPos = spawnMiddle + MathUtils::randomPointOnUnitSphere() * 1.0;
			
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 0 ] = spawnPos.x;
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 1 ] = spawnPos.y;
			spawnPosBuffer.getPixels()[ (tmpIndex * 3) + 2 ] = spawnPos.z;
			
			tmpIndex++;
		}
	}
	
	spawnPosTexture.loadData( spawnPosBuffer );
	*/
	
	
	updateParticles( _time, _timeStep );
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::draw( ofCamera* _camera )
{

	drawParticles( &particleDraw, _camera );
	
	ofSetColor( ofColor::white );
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::drawGui()
{
	gui.draw();
	guiMaterial.draw();
	
	//spawnPosTexture.draw( gui.getPosition() + ofVec2f(0,gui.getHeight() + 10 ), 128, 128);
	
	ofEnableBlendMode( OF_BLENDMODE_DISABLED );
	ofSetColor( ofColor::white );
	
	debugDrawOpticalFlow.begin();
	debugDrawOpticalFlow.setUniformTexture("opticalFlowMap", opticalFlowTexture, 0 );
	
		opticalFlowTexture.draw( gui.getPosition() + ofVec2f(0,gui.getHeight() + 10 ), 128, 128 );

	debugDrawOpticalFlow.end();
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::updateParticles( float _time, float _timeStep )
{
	ofTextureData tmpTexDat = opticalFlowTexture.getTextureData();
	
	ofEnableBlendMode( OF_BLENDMODE_DISABLED ); // Important! We just want to write the data as is to the target fbo
	
	particleDataFbo.dest()->begin();
	
		particleDataFbo.dest()->activateAllDrawBuffers(); // if we have multiple color buffers in our FBO we need this to activate all of them
		
		particleUpdate.begin();
	
			particleUpdate.setUniformTexture( "u_particlePosAndAgeTexture",	particleDataFbo.source()->getTextureReference(0), 0 );
			particleUpdate.setUniformTexture( "u_particleVelTexture",		particleDataFbo.source()->getTextureReference(1), 1 );
			particleUpdate.setUniformTexture( "u_spawnPosTexture",			spawnPosTexture,								  2 );
			particleUpdate.setUniformTexture( "u_opticalFlowMap",			opticalFlowTexture,								  3 );
	
			particleUpdate.setUniformMatrix4f("u_worldToKinect",  worldToKinect );
	
			double HFOV = 1.01447;	// Grabbed these by querying with OpenNI, so will probably only work with the Kinect V1
			double VFOV = 0.789809;
	
			particleUpdate.setUniform1f("u_fXToZ", tan(HFOV/2)*2 );
			particleUpdate.setUniform1f("u_fYToZ", tan(VFOV/2)*2 );
	
			particleUpdate.setUniform1f("u_time", _time );
			particleUpdate.setUniform1f("u_timeStep", _timeStep );
	
			particleUpdate.setUniform2f("u_pixelDepthmap", 1.0 / tmpTexDat.tex_w, 1.0 / tmpTexDat.tex_h );
			particleUpdate.setUniform1f("u_maxFlowMag", flowMaxLength );
			particleUpdate.setUniform1f("u_flowMagnitude", flowMagnitude );
	
			particleUpdate.setUniform2fv("u_averageFlow", averageFlow.getPtr() );
			particleUpdate.setUniform1f("u_averageMagnitude", averageFlowMagnitude );
	
			particleUpdate.setUniform1f("u_particleMaxAge", particleMaxAge );

			particleUpdate.setUniform1f("u_maxVel", maxVel );

			particleUpdate.setUniform1f("u_noisePositionScale", noisePositionScale / 1000.0 );
			particleUpdate.setUniform1f("u_noiseTimeScale", noiseTimeScale );
			particleUpdate.setUniform1f("u_noisePersistence", noisePersistence );
			particleUpdate.setUniform1f("u_noiseMagnitude", noiseMagnitude );
			particleUpdate.setUniform3f("u_wind", baseSpeed.get().x, baseSpeed.get().y, baseSpeed.get().z );

			particleUpdate.setUniform1f("u_oldVelToUse", oldVelToUse );
	
	
			particleDataFbo.source()->draw(0,0);
		
		particleUpdate.end();
		
	particleDataFbo.dest()->end();
	
	particleDataFbo.swap();
}

//-----------------------------------------------------------------------------------------
//
void ParticleSystemOpticalFlow::drawParticles( ofShader* _shader, ofCamera* _camera )
{
	ofFloatColor particleStartCol = startColor.get();
	ofFloatColor particleEndCol = endColor.get();

	ofSetColor( ofColor::white );
	ofEnableBlendMode( OF_BLENDMODE_ALPHA );
	
	_shader->begin();

		_shader->setUniformTexture("u_particlePosAndAgeTexture", particleDataFbo.source()->getTextureReference(0), 1 );
		_shader->setUniformTexture("u_particleVelTexture", particleDataFbo.source()->getTextureReference(1), 2 );
	
		_shader->setUniform1f("u_meshStartScale", startScale );
		_shader->setUniform1f("u_meshEndScale", endScale );
	
		_shader->setUniform2f("u_resolution", particleDataFbo.source()->getWidth(), particleDataFbo.source()->getHeight() );
		_shader->setUniform1f("u_time", ofGetElapsedTimef() );
	
		_shader->setUniformMatrix4f("u_modelViewMatrix", _camera->getModelViewMatrix() );
		_shader->setUniformMatrix4f("u_projectionMatrix", _camera->getProjectionMatrix() );
		_shader->setUniformMatrix4f("u_modelViewProjectionMatrix", _camera->getModelViewProjectionMatrix() );

		_shader->setUniform1f("u_particleMaxAge", particleMaxAge );

		_shader->setUniform1i("u_numLights", 1 );
	
		_shader->setUniform4fv("u_particleStartColor", particleStartCol.v );
		_shader->setUniform4fv("u_particleEndColor", particleEndCol.v );

		// Calling begin() on the material sets the OpenGL state that we then read in the shader
		particleMaterial.begin();
	
			singleParticleMesh.drawInstanced( OF_MESH_FILL, numParticles );

		particleMaterial.end();
	
	_shader->end();
	
}
