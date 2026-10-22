#include <tamashii/engine/scene/scene_graph.hpp>

T_USE_NAMESPACE

TRS::TRS(): translation(glm::vec3(0.0f)), rotation(glm::vec4(0.0f)), scale(glm::vec3(1.0f)),
                  trans_anim_inter(Interpolation::NONE), rot_anim_inter(Interpolation::NONE),
                  scale_anim_inter(Interpolation::NONE), time(0)
{
}

bool TRS::hasTranslation() const
{
	return any(notEqual(translation, glm::vec3(0.0f)));
}

bool TRS::hasRotation() const
{
	return any(notEqual(rotation, glm::vec4(0.0f)));
}

bool TRS::hasScale() const
{
	return any(notEqual(scale, glm::vec3(1.0f)));
}

bool TRS::hasTranslationAnimation() const
{
	return (trans_anim_inter != Interpolation::NONE) && !translation_time_steps.empty();
}

bool TRS::hasRotationAnimation() const
{
	return (rot_anim_inter != Interpolation::NONE) && !rotation_time_steps.empty();
}

bool TRS::hasScaleAnimation() const
{
	return (scale_anim_inter != Interpolation::NONE) && !scale_time_steps.empty();
}

glm::mat4 TRS::getMatrix(const float aTime)
{
	time = std::isnan(aTime) ? 0.0f : aTime;
	auto localModelMatrix = glm::mat4(1.0f);
	computeTranslation(aTime, localModelMatrix);
	computeRotation(aTime, localModelMatrix);
	computeScale(aTime, localModelMatrix);
	return localModelMatrix;
}

void TRS::computeTranslation(const float& aTime, glm::mat4& aLocalModelMatrix) const
{
	if (trans_anim_inter == Interpolation::LINEAR)
	{
		int previous = 0;
		int next = translation_time_steps.size() - 1;
		for (int i = 0; i < translation_time_steps.size(); i++)
		{
			const float timeStep = translation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime)
			{
				next = i;
				break;
			}
		}
		const float interpolationValue = (aTime - translation_time_steps.at(previous)) / (translation_time_steps.at(next) -
			translation_time_steps.at(previous));
		glm::vec3 currentTranslation;
		// if previous and next is same because time is not in animation range 
		if (std::isinf(interpolationValue)) currentTranslation = translation_steps.at(previous);
		else currentTranslation = mix(translation_steps.at(previous), translation_steps.at(next), interpolationValue);
		aLocalModelMatrix = translate(aLocalModelMatrix, currentTranslation);
	}
	else if (trans_anim_inter == Interpolation::STEP)
	{
		int previous = 0;
		for (int i = 0; i < translation_time_steps.size(); i++)
		{
			const float timeStep = translation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime) break;
		}
		aLocalModelMatrix = translate(aLocalModelMatrix, translation_steps.at(previous));
	}
	else if (trans_anim_inter == Interpolation::CUBICSPLINE)
	{
		int previous = 0;
		int next = translation_time_steps.size() - 1;
		for (int i = 0; i < translation_time_steps.size(); i++)
		{
			const float timeStep = translation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime)
			{
				next = i;
				break;
			}
		}
		const float deltaTime = translation_time_steps.at(next) - translation_time_steps.at(previous);
		const glm::vec3 previousTangent = deltaTime * translation_steps.at((previous * 3) + 2);
		const glm::vec3 nextTangent = deltaTime * translation_steps.at((next * 3));
		const glm::vec3 previousValue = translation_steps.at((previous * 3) + 1);
		const glm::vec3 nextValue = translation_steps.at((next * 3) + 1);

		if (deltaTime == 0.0f)
		{
			aLocalModelMatrix = translate(aLocalModelMatrix, previousValue);
			return;
		}

		const float t = (aTime - translation_time_steps.at(previous)) / (translation_time_steps.at(next) -
			translation_time_steps.at(previous));
		const float t2 = t * t;
		const float t3 = t2 * t;

		const glm::vec3 currentTranslation = (2 * t3 - 3 * t2 + 1) * previousValue + (t3 - 2 * t2 + t) * previousTangent + (-2
			* t3 + 3 * t2) * nextValue + (t3 - t2) * nextTangent;
		aLocalModelMatrix = translate(aLocalModelMatrix, currentTranslation);
	}
	else if (hasTranslation()) aLocalModelMatrix = translate(aLocalModelMatrix, translation);
}

void TRS::computeRotation(const float& aTime, glm::mat4& aLocalModelMatrix)
{
	if (rot_anim_inter == Interpolation::LINEAR)
	{
		int previous = 0;
		int next = rotation_time_steps.size() - 1;
		for (int i = 0; i < rotation_time_steps.size(); i++)
		{
			const float timeStep = rotation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime)
			{
				next = i;
				break;
			}
		}
		const float interpolationValue = (aTime - rotation_time_steps.at(previous)) / (rotation_time_steps.at(next) -
			rotation_time_steps.at(previous));
		glm::vec4 prev_rot = rotation_steps.at(previous);
		glm::vec4 next_rot = rotation_steps.at(next);
		// if previous and next is same because time is not in animation range 
		if (std::isinf(interpolationValue))
		{
			aLocalModelMatrix *= toMat4(glm::quat(prev_rot[3], prev_rot[0], prev_rot[1], prev_rot[2]));
			return;
		}
		const glm::quat interpolatedQuat = slerp(glm::quat(prev_rot[3], prev_rot[0], prev_rot[1], prev_rot[2]),
			glm::quat(next_rot[3], next_rot[0], next_rot[1], next_rot[2]),
			interpolationValue);
		aLocalModelMatrix *= toMat4(interpolatedQuat);
	}
	else if (rot_anim_inter == Interpolation::STEP)
	{
		int previous = 0;
		for (int i = 0; i < rotation_time_steps.size(); i++)
		{
			const float timeStep = rotation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime) break;
		}
		glm::vec4 prevRot = rotation_steps.at(previous);
		const auto prevQuat = glm::quat(prevRot[3], prevRot);
		aLocalModelMatrix *= toMat4(prevQuat);
	}
	else if (rot_anim_inter == Interpolation::CUBICSPLINE)
	{
		int previous = 0;
		int next = rotation_time_steps.size() - 1;
		for (int i = 0; i < rotation_time_steps.size(); i++)
		{
			const float timeStep = rotation_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime)
			{
				next = i;
				break;
			}
		}
		const float deltaTime = rotation_time_steps.at(next) - rotation_time_steps.at(previous);
		const glm::vec4 previousTangent = deltaTime * rotation_steps.at((previous * 3) + 2);
		const glm::vec4 nextTangent = deltaTime * rotation_steps.at((next * 3));
		glm::vec4 previousValue = rotation_steps.at((previous * 3) + 1);
		const glm::vec4 nextValue = rotation_steps.at((next * 3) + 1);

		if (deltaTime == 0.0f)
		{
			aLocalModelMatrix *= toMat4(
				normalize(glm::quat(previousValue[3], previousValue[0], previousValue[1], previousValue[2])));
			return;
		}

		const float t = (aTime - rotation_time_steps.at(previous)) / (rotation_time_steps.at(next) - rotation_time_steps.
			at(previous));
		const float t2 = t * t;
		const float t3 = t2 * t;

		glm::vec4 currentRotation = (2 * t3 - 3 * t2 + 1) * previousValue + (t3 - 2 * t2 + t) * previousTangent + (-2 *
			t3 + 3 * t2) * nextValue + (t3 - t2) * nextTangent;
		aLocalModelMatrix *= toMat4(normalize(glm::quat(currentRotation[3], currentRotation[0], currentRotation[1],
			currentRotation[2])));
	}
	else if (hasRotation()) aLocalModelMatrix *= toMat4(
		glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]));
}

void TRS::computeScale(const float& aTime, glm::mat4& aLocalModelMatrix) const
{
	if (scale_anim_inter == Interpolation::LINEAR)
	{
		int previous = 0;
		int next = scale_time_steps.size() - 1;
		for (int i = 0; i < scale_time_steps.size(); i++)
		{
			const float timeStep = scale_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime)
			{
				next = i;
				break;
			}
		}
		const float interpolationValue = (aTime - scale_time_steps.at(previous)) / (scale_time_steps.at(next) -
			scale_time_steps.at(previous));
		glm::vec3 currentScale;
		// if previous and next is same because time is not in animation range 
		if (std::isinf(interpolationValue)) currentScale = scale_steps.at(previous);
		else currentScale = mix(scale_steps.at(previous), scale_steps.at(next), interpolationValue);
		aLocalModelMatrix = glm::scale(aLocalModelMatrix, currentScale);
	}
	else if (scale_anim_inter == Interpolation::STEP)
	{
		int previous = 0;
		for (int i = 0; i < scale_time_steps.size(); i++)
		{
			const float timeStep = scale_time_steps.at(i);
			if (timeStep <= aTime) previous = i;
			else if (timeStep >= aTime) break;
		}
		aLocalModelMatrix = glm::scale(aLocalModelMatrix, scale_steps.at(previous));
	}
	else if (scale_anim_inter == Interpolation::CUBICSPLINE)
	{
		int previous = 0;
		int next = scale_time_steps.size() - 1;
		for (int i = 0; i < scale_time_steps.size(); i++)
		{
			const float time_step = scale_time_steps.at(i);
			if (time_step <= aTime) previous = i;
			else if (time_step >= aTime)
			{
				next = i;
				break;
			}
		}
		const float deltaTime = scale_time_steps.at(next) - scale_time_steps.at(previous);
		const glm::vec3 previousTangent = deltaTime * scale_steps.at((previous * 3) + 2);
		const glm::vec3 nextTangent = deltaTime * scale_steps.at((next * 3));
		const glm::vec3 previousValue = scale_steps.at((previous * 3) + 1);
		const glm::vec3 nextValue = scale_steps.at((next * 3) + 1);

		if (deltaTime == 0.0f)
		{
			aLocalModelMatrix = glm::scale(aLocalModelMatrix, previousValue);
			return;
		}

		const float t = (aTime - scale_time_steps.at(previous)) / (scale_time_steps.at(next) - scale_time_steps.at(previous));
		const float t2 = t * t;
		const float t3 = t2 * t;

		const glm::vec3 currentScale = (2 * t3 - 3 * t2 + 1) * previousValue + (t3 - 2 * t2 + t) * previousTangent + (-2 * t3
			+ 3 * t2) * nextValue + (t3 - t2) * nextTangent;
		aLocalModelMatrix = glm::scale(aLocalModelMatrix, currentScale);
	}
	else if (hasScale()) aLocalModelMatrix = glm::scale(aLocalModelMatrix, scale);
}

Node::Node(const std::string& aName): Asset(Type::NODE, aName), mModel(nullptr),
                                      mCamera(nullptr), mLight(nullptr), mTrs()
{
}

Node* Node::alloc(const std::string& aName)
{
	return new Node(aName);
}

uint32_t Node::numNodes() const
{
	return mChildren.size();
}

Node* Node::getNode(const int aIndex) const
{
	return mChildren[aIndex];
}

void Node::addNode(Node* aNode)
{
	mChildren.push_back(aNode);
}

bool Node::hasModel() const
{
	return mModel != nullptr;
}

bool Node::hasCamera() const
{
	return mCamera != nullptr;
}

bool Node::hasLight() const
{
	return mLight != nullptr;
}

Model* Node::getModel() const
{
	return mModel;
}

Camera* Node::getCamera() const
{
	return mCamera;
}

Light* Node::getLight() const
{
	return mLight;
}

void Node::setModel(Model* aModel)
{
	mModel = aModel;
}

void Node::setCamera(Camera* aCamera)
{
	mCamera = aCamera;
}

void Node::setLight(Light* aLight)
{
	mLight = aLight;
}

TRS* Node::getTRS()
{
	return &mTrs;
}

bool Node::hasAnimation() const
{
	return hasTranslationAnimation() || hasRotationAnimation() || hasScaleAnimation();
}

bool Node::hasLocalTransform() const
{
	return hasTranslation() || hasRotation() || hasScale() || hasAnimation();
}

bool Node::hasTranslation() const
{
	return mTrs.hasTranslation();
}

bool Node::hasRotation() const
{
	return mTrs.hasRotation();
}

bool Node::hasScale() const
{
	return mTrs.hasScale();
}

bool Node::hasTranslationAnimation() const
{
	return mTrs.hasTranslationAnimation();
}

bool Node::hasRotationAnimation() const
{
	return mTrs.hasRotationAnimation();
}

bool Node::hasScaleAnimation() const
{
	return mTrs.hasScaleAnimation();
}

void Node::setTranslationAnimation(const TRS::Interpolation aInterpolation, const std::vector<float>& aTimeSteps,
                                   const std::vector<glm::vec3>& aTranslationSteps)
{
	mTrs.trans_anim_inter = aInterpolation;
	mTrs.translation_time_steps = aTimeSteps;
	mTrs.translation_steps = aTranslationSteps;
}

void Node::setRotationAnimation(const TRS::Interpolation aInterpolation, const std::vector<float>& aTimeSteps,
                                const std::vector<glm::vec4>& aRotationSteps)
{
	mTrs.rot_anim_inter = aInterpolation;
	mTrs.rotation_time_steps = aTimeSteps;
	mTrs.rotation_steps = aRotationSteps;
}

void Node::setScaleAnimation(const TRS::Interpolation aInterpolation, const std::vector<float>& aTimeSteps, 
							 const std::vector<glm::vec3>& aScaleSteps)
{
	mTrs.scale_anim_inter = aInterpolation;
	mTrs.scale_time_steps = aTimeSteps;
	mTrs.scale_steps = aScaleSteps;
}

std::vector<Node*>::iterator Node::begin()
{
	return mChildren.begin();
}

std::vector<Node*>::const_iterator Node::begin() const
{
	return mChildren.begin();
}

std::vector<Node*>::iterator Node::end()
{
	return mChildren.end();
}

std::vector<Node*>::const_iterator Node::end() const
{
	return mChildren.end();
}

SceneInfo_s::SceneInfo_s(): mCycleTime(0)
{
}

SceneInfo_s* SceneInfo_s::alloc()
{
	return new SceneInfo_s();
}

Node::~Node()
{
	for (const Node* n : *this) delete n;
}

glm::mat4 Node::getLocalModelMatrix() const
{
	auto localModelMatrix = glm::mat4(1.0f);
	if (hasTranslation()) localModelMatrix = translate(localModelMatrix, mTrs.translation);
	if (hasRotation()) localModelMatrix *= toMat4(glm::quat(mTrs.rotation[3], mTrs.rotation[0],
	                                                        mTrs.rotation[1], mTrs.rotation[2]));
	if (hasScale()) localModelMatrix = scale(localModelMatrix, mTrs.scale);
	return localModelMatrix;
}

void Node::setTranslation(const glm::vec3& aTranslation)
{
	mTrs.translation = aTranslation;
}

void Node::setRotation(const glm::vec4& aRotation)
{
	mTrs.rotation = aRotation;
}

void Node::setScale(const glm::vec3& aScale)
{
	mTrs.scale = aScale;
}

void Node::applyFunction(const std::function<void(Node*)>& aFunction)
{
	aFunction(this);
	for (Node* n : *this) n->applyFunction(aFunction);
}

SceneGraph::~SceneGraph()
{
	for (const Node* n : *this) delete n;
}

void SceneGraph::applyFunction(const std::function<void(Node*)>& aFunction)
{
	for (Node* n : *this) n->applyFunction(aFunction);
}


SceneGraph::SceneGraph(const std::string& aName) : Asset(Type::SCENE, aName)
{
}

SceneGraph* SceneGraph::alloc(const std::string& aName)
{
	return new SceneGraph(aName);
}

Node* SceneGraph::getRootNode(const int aIndex) const
{
	return mRoots[aIndex];
}

std::vector<Node*>& SceneGraph::getRootNodes()
{
	return mRoots;
}

void SceneGraph::addRootNode(Node* aNode)
{
	mRoots.push_back(aNode);
}

std::vector<Node*>::iterator SceneGraph::begin()
{
	return mRoots.begin();
}

std::vector<Node*>::const_iterator SceneGraph::begin() const
{
	return mRoots.begin();
}

std::vector<Node*>::iterator SceneGraph::end()
{
	return mRoots.end();
}

std::vector<Node*>::const_iterator SceneGraph::end() const
{
	return mRoots.end();
}
