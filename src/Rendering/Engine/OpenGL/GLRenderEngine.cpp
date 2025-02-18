#include "GLRenderEngine.h"
#include "GLRenderHelper.h"
#include "GLVisualModule.h"

#include "Utility.h"
#include "ShadowMap.h"
#include "SSAO.h"
#include "FXAA.h"

// dyno
#include "SceneGraph.h"
#include "Action.h"

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>

#include <OrbitCamera.h>
#include <TrackballCamera.h>
#include <unordered_set>
#include <memory>

namespace dyno
{
	GLRenderEngine::GLRenderEngine()
	{

	}

	GLRenderEngine::~GLRenderEngine()
	{

	}

	void GLRenderEngine::initialize()
	{
		if (!gladLoadGL()) {
			printf("Failed to load OpenGL context!");
			exit(-1);
		}

		// some basic opengl settings
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_PROGRAM_POINT_SIZE);
		glDepthFunc(GL_LEQUAL);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		setupInternalFramebuffer();

		// OIT
		setupTransparencyPass();

		// create uniform block for transform
		mTransformUBO.create(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		// create uniform block for light
		mLightUBO.create(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		// create uniform bnlock for global variables
		mVariableUBO.create(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW);
		gl::glCheckError();

		// create a screen quad
		mScreenQuad = gl::Mesh::ScreenQuad();

		mRenderHelper = new GLRenderHelper();
		mShadowMap = new ShadowMap(2048, 2048);
		mFXAAFilter = new FXAA;

	}

	void GLRenderEngine::terminate()
	{
		// release render modules
		for (auto item : mRenderItems) {
			item.visualModule->destroyGL();
		}

		// release framebuffer
		mFramebuffer.release();
		mColorTex.release();
		mDepthTex.release();
		mIndexTex.release();

		// release linked-list OIT objects
		mFreeNodeIdx.release();
		mLinkedListBuffer.release();
		mHeadIndexTex.release();
		mBlendProgram->release();
		delete mBlendProgram;

		// release uniform buffers
		mTransformUBO.release();
		mLightUBO.release();
		mVariableUBO.release();

		// release other objects
		mScreenQuad->release();
		delete mScreenQuad;

		delete mRenderHelper;
		delete mShadowMap;
		delete mFXAAFilter;

	}

	void GLRenderEngine::setupTransparencyPass()
	{
		mFreeNodeIdx.create(GL_ATOMIC_COUNTER_BUFFER, GL_DYNAMIC_DRAW);
		mFreeNodeIdx.allocate(sizeof(int));

		mLinkedListBuffer.create(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW);
		struct NodeType
		{
			glm::vec4 color;
			float	  depth;
			unsigned int next;
			unsigned int idx0;
			unsigned int idx1;
		};
		mLinkedListBuffer.allocate(sizeof(NodeType) * MAX_OIT_NODES);

		// transparency
		mHeadIndexTex.internalFormat = GL_R32UI;
		mHeadIndexTex.format = GL_RED_INTEGER;
		mHeadIndexTex.type = GL_UNSIGNED_INT;
		mHeadIndexTex.create();
		mHeadIndexTex.resize(1, 1);

		mBlendProgram = gl::ShaderFactory::createShaderProgram("screen.vert", "blend.frag");
	}


	void GLRenderEngine::setupInternalFramebuffer()
	{
		// create render textures
		mColorTex.maxFilter = GL_LINEAR;
		mColorTex.minFilter = GL_LINEAR;
		mColorTex.format = GL_RGBA;
		mColorTex.internalFormat = GL_RGBA;
		mColorTex.type = GL_BYTE;
		mColorTex.create();
		mColorTex.resize(1, 1);

		mDepthTex.internalFormat = GL_DEPTH_COMPONENT32;
		mDepthTex.format = GL_DEPTH_COMPONENT;
		mDepthTex.create();
		mDepthTex.resize(1, 1);

		// index
		mIndexTex.internalFormat = GL_RGBA32I;
		mIndexTex.format = GL_RGBA_INTEGER;
		mIndexTex.type   = GL_INT;
		//mIndexTex.wrapS = GL_CLAMP_TO_EDGE;
		//mIndexTex.wrapT = GL_CLAMP_TO_EDGE;
		mIndexTex.create();
		mIndexTex.resize(1, 1);

		// create framebuffer
		mFramebuffer.create();

		// bind framebuffer texture
		mFramebuffer.bind();
		mFramebuffer.setTexture2D(GL_DEPTH_ATTACHMENT, mDepthTex.id);
		mFramebuffer.setTexture2D(GL_COLOR_ATTACHMENT0, mColorTex.id);
		mFramebuffer.setTexture2D(GL_COLOR_ATTACHMENT1, mIndexTex.id);

		const GLenum buffers[] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1
		};
		mFramebuffer.drawBuffers(2, buffers);

		mFramebuffer.checkStatus();
		mFramebuffer.unbind();
		gl::glCheckError();
	}

	void GLRenderEngine::updateRenderModules(dyno::SceneGraph* scene)
	{

		std::vector<RenderItem> items;

		for (auto iter = scene->begin(); iter != scene->end(); iter++) {
			auto node = iter.get();

			for (auto m : node->graphicsPipeline()->activeModules()) {
				auto vm = std::dynamic_pointer_cast<GLVisualModule>(m);
				if (vm) {
					items.push_back({ node, vm });
				}
			}
		}

		
		for (auto item : mRenderItems) {
			// release GL resource for unreferenced visual module
			if (std::find(items.begin(), items.end(), item) == items.end())
			{
				item.visualModule->destroyGL();
			}
		}

		mRenderItems = items;


		// initialize modules
		for (auto item : mRenderItems) {
			if (!item.visualModule->isGLInitialized)
				item.visualModule->isGLInitialized = item.visualModule->initializeGL();

			if (!item.visualModule->isGLInitialized)
				printf("Warning: failed to initialize %s\n", item.visualModule->getName().c_str());
		}

		// update if necessary
		for (auto item : mRenderItems) {
			if (item.visualModule->isGLInitialized)
			{
				// check update
				if (item.visualModule->changed > item.visualModule->updated) {
					item.visualModule->updateGL();
					item.visualModule->updated = GLVisualModule::clock::now();
				}
			}
		}

	}

	void GLRenderEngine::draw(dyno::SceneGraph* scene, Camera* camera, const RenderParams& rparams)
	{
		updateRenderModules(scene);

		// preserve current framebuffer
		GLint fbo;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

		// update framebuffer size
		resizeFramebuffer(camera->viewportWidth(), camera->viewportHeight());

		// update shadow map
		mShadowMap->update(scene, camera, rparams);

		// setup scene transform matrices
		struct
		{
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 projection;
			int width;
			int height;
		} sceneUniformBuffer;
		sceneUniformBuffer.model = glm::mat4(1);
		sceneUniformBuffer.view = camera->getViewMat();
		sceneUniformBuffer.projection = camera->getProjMat();
		sceneUniformBuffer.width = camera->viewportWidth();
		sceneUniformBuffer.height = camera->viewportHeight();

		mTransformUBO.load(&sceneUniformBuffer, sizeof(sceneUniformBuffer));
		mTransformUBO.bindBufferBase(0);

		// setup light block
		RenderParams::Light light = rparams.light;
		light.mainLightDirection = glm::vec3(camera->getViewMat() * glm::vec4(light.mainLightDirection, 0));
		mLightUBO.load(&light, sizeof(light));
		mLightUBO.bindBufferBase(1);
						
		mFramebuffer.bind(GL_DRAW_FRAMEBUFFER);

		// clear color and depth
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// clear index buffer
		//GLint clearIndex[]{11, -1, -1, -1};
		//glClearBufferiv(GL_COLOR, 1, clearIndex);
		glViewport(0, 0, camera->viewportWidth(), camera->viewportHeight());
		// draw background color
		Vec3f c0 = Vec3f(rparams.bgColor0.x, rparams.bgColor0.y, rparams.bgColor0.z);
		Vec3f c1 = Vec3f(rparams.bgColor1.x, rparams.bgColor1.y, rparams.bgColor1.z);
		mRenderHelper->drawBackground(c0, c1);
		
		mVariableUBO.bindBufferBase(2);

		// render opacity objects
		for (int i = 0; i < mRenderItems.size(); i++) {

			if (mRenderItems[i].node->isVisible() && !mRenderItems[i].visualModule->isTransparent())
			{
				mVariableUBO.load(&i, sizeof(i));
				mRenderItems[i].visualModule->draw(GLRenderPass::COLOR);
			}
		}

		// render transparency objects
		{
			// reset free node index
			const int zero = 0;
			mFreeNodeIdx.load((void*)&zero, sizeof(int));
			// reset head index
			const int clear = 0xFFFFFFFF;
			mHeadIndexTex.clear((void*)&clear);

			glBindImageTexture(0, mHeadIndexTex.id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
			mFreeNodeIdx.bindBufferBase(0);
			mLinkedListBuffer.bindBufferBase(0);

			// draw to no attachments
			mFramebuffer.drawBuffers(0, 0);

			glDepthMask(false);
			for (int i = 0; i < mRenderItems.size(); i++) {

				if (mRenderItems[i].node->isVisible() && mRenderItems[i].visualModule->isTransparent())
				{
					mVariableUBO.load(&i, sizeof(i));
					mRenderItems[i].visualModule->draw(GLRenderPass::TRANSPARENCY);
				}
			}

			// draw a ruler plane
			if (rparams.showGround)
			{
				mRenderHelper->drawGround(rparams.planeScale, rparams.rulerScale);
			}

			glDepthMask(true);

			// blend alpha
			const unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			mFramebuffer.drawBuffers(2, attachments);

			mBlendProgram->use();

			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);

			mScreenQuad->draw();

			glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}


		// draw scene bounding box
		if (rparams.showSceneBounds && scene != 0)
		{
			// get bounding box of the scene
			auto p0 = scene->getLowerBound();
			auto p1 = scene->getUpperBound();
			mRenderHelper->drawBBox(p0, p1);
		}

		// draw to final framebuffer with fxaa filter
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
				
		if (bEnableFXAA)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, camera->viewportWidth(), camera->viewportHeight());

			mColorTex.bind(GL_TEXTURE1);			
			mFXAAFilter->apply(camera->viewportWidth(), camera->viewportHeight());
		}
		else
		{
			mFramebuffer.bind(GL_READ_FRAMEBUFFER);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(
				0, 0, camera->viewportWidth(), camera->viewportHeight(),
				0, 0, camera->viewportWidth(), camera->viewportHeight(),
				GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}

		gl::glCheckError();
	}


	void GLRenderEngine::resizeFramebuffer(int w, int h)
	{
		// resize internal framebuffer
		mColorTex.resize(w, h);
		mDepthTex.resize(w, h);
		mIndexTex.resize(w, h);

		// transparency
		mHeadIndexTex.resize(w, h);

		gl::glCheckError();
	}

	std::string GLRenderEngine::name() const
	{
		return std::string("Native OpenGL");
	}

	Selection GLRenderEngine::select(int x, int y, int w, int h)
	{
		// TODO: check valid input
		w = std::max(1, w);
		h = std::max(1, h);

		// read pixels
		std::vector<glm::ivec4> indices(w * h);

		mFramebuffer.bind(GL_READ_FRAMEBUFFER);
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		//glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, y, w, h, GL_RGBA_INTEGER, GL_INT, indices.data());
		gl::glCheckError();

		// use unordered set to get unique id
		std::unordered_set<glm::ivec4> uniqueIdx(indices.begin(), indices.end());

		Selection result;
		result.x = x;
		result.y = y;
		result.w = w;
		result.h = h;

		for (const auto& idx : uniqueIdx) {
			const int nodeIdx = idx.x;
			const int instIdx = idx.y;
			const int primIdx = idx.z;

			if (nodeIdx >= 0 && nodeIdx < mRenderItems.size()) {
				result.items.push_back({mRenderItems[nodeIdx].node,	instIdx, primIdx});
			}
		}

		return result;
	}

}